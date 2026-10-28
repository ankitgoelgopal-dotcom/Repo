/* PCI enumeration + Intel e1000 (82540EM / 82545EM) driver, polled. This is the NIC QEMU emulates with -device e1000. */
#include "kernel.h"
#include "mm.h"
#include "net.h"
#include "mem.h"

static uint32_t pci_read(int bus, int dev, int fn, int off) {
    uint32_t addr = 0x80000000u | bus << 16 | dev << 11 | fn << 8 | (off & 0xFC);
    outl(0xCF8, addr); return inl(0xCFC);
}
static void pci_write(int bus, int dev, int fn, int off, uint32_t val) {
    uint32_t addr = 0x80000000u | bus << 16 | dev << 11 | fn << 8 | (off & 0xFC);
    outl(0xCF8, addr); outl(0xCFC, val);
}

#define NRX 32
#define NTX 16
struct rxd { uint64_t addr; uint16_t len, csum; uint8_t status, errors; uint16_t special; } __attribute__((packed));
struct txd { uint64_t addr; uint16_t len; uint8_t cso, cmd, status, css; uint16_t special; } __attribute__((packed));
static volatile uint8_t *regs;
static struct rxd *rxr; static struct txd *txr;
static uint8_t *rxbuf[NRX], *txbuf[NTX];
static unsigned rx_cur, tx_cur;

static uint32_t rd(int r) { return *(volatile uint32_t *)(regs + r); }
static void wr(int r, uint32_t v) { *(volatile uint32_t *)(regs + r) = v; }
#define CTRL 0x0000
#define STATUS 0x0008
#define EERD 0x0014
#define IMC 0x00D8
#define RCTL 0x0100
#define TCTL 0x0400
#define TIPG 0x0410
#define RDBAL 0x2800
#define RDBAH 0x2804
#define RDLEN 0x2808
#define RDH 0x2810
#define RDT 0x2818
#define TDBAL 0x3800
#define TDBAH 0x3804
#define TDLEN 0x3808
#define TDH 0x3810
#define TDT 0x3818
#define MTA 0x5200
#define RAL0 0x5400
#define RAH0 0x5404

static uint16_t eeprom(int a) {
    wr(EERD, 1 | (uint32_t)a << 8);
    for (int i = 0; i < 100000; i++) { uint32_t v = rd(EERD); if (v & 0x10) return (uint16_t)(v >> 16); }
    return 0;
}

int e1000_init(void) {
    int fb = -1, fd = -1, ff = -1;
    for (int b = 0; b < 4 && fb < 0; b++) for (int d = 0; d < 32 && fb < 0; d++) for (int f = 0; f < 8 && fb < 0; f++) {
        uint32_t id = pci_read(b, d, f, 0);
        if ((id & 0xFFFF) == 0xFFFF) { if (f == 0) break; continue; }
        if ((id & 0xFFFF) == 0x8086 && ((id >> 16) == 0x100E || (id >> 16) == 0x100F)) { fb = b; fd = d; ff = f; }
    }
    if (fb < 0) { kprintf("[pci ] no e1000 NIC found (in your emulator choose NIC model: e1000)\n"); return -1; }
    uint32_t bar0 = pci_read(fb, fd, ff, 0x10) & ~0xFu;
    kprintf("[pci ] 8086:100e e1000 at %02x:%02x.%x BAR0=%p\n", fb, fd, ff, (void *)(uint64_t)bar0);
    if (bar0 < 0xC0000000u) { kprintf("[pci ] BAR0 outside mapped MMIO window\n"); return -1; }
    pci_write(fb, fd, ff, 0x04, pci_read(fb, fd, ff, 0x04) | 0x7);      /* IO, memory, bus master */
    regs = (volatile uint8_t *)(uint64_t)bar0;

    wr(IMC, 0xFFFFFFFF);
    wr(CTRL, rd(CTRL) | (1u << 26));                                    /* reset */
    for (volatile int i = 0; i < 200000; i++) ;
    wr(IMC, 0xFFFFFFFF);
    wr(CTRL, (rd(CTRL) | (1u << 6) | (1u << 5)) & ~(1u << 3));          /* SLU, ASDE */

    uint32_t ral = rd(RAL0), rah = rd(RAH0);
    if (!ral && !(rah & 0xFFFF)) { uint16_t w0 = eeprom(0), w1 = eeprom(1), w2 = eeprom(2); ral = w0 | (uint32_t)w1 << 16; rah = w2 | (1u << 31); }
    for (int i = 0; i < 4; i++) nif.mac[i] = (uint8_t)(ral >> (8 * i));
    nif.mac[4] = (uint8_t)rah; nif.mac[5] = (uint8_t)(rah >> 8);
    wr(RAL0, ral); wr(RAH0, (rah & 0xFFFF) | (1u << 31));
    for (int i = 0; i < 128; i++) wr(MTA + i * 4, 0);

    uint64_t rp = pmm_alloc_contig(1), tp = pmm_alloc_contig(1);        /* descriptor rings (identity mapped: phys == virt) */
    if (!rp || !tp) return -1;
    rxr = (struct rxd *)rp; txr = (struct txd *)tp;
    for (int i = 0; i < NRX; i++) { rxbuf[i] = (uint8_t *)pmm_alloc(); rxr[i].addr = (uint64_t)rxbuf[i]; }
    for (int i = 0; i < NTX; i++) { txbuf[i] = (uint8_t *)pmm_alloc(); txr[i].addr = (uint64_t)txbuf[i]; txr[i].status = 1; }
    wr(RDBAL, (uint32_t)rp); wr(RDBAH, 0); wr(RDLEN, NRX * 16); wr(RDH, 0); wr(RDT, NRX - 1);
    wr(RCTL, (1u << 1) | (1u << 15) | (1u << 26));                      /* EN, BAM (broadcast), SECRC, 2048B buffers */
    wr(TDBAL, (uint32_t)tp); wr(TDBAH, 0); wr(TDLEN, NTX * 16); wr(TDH, 0); wr(TDT, 0);
    wr(TIPG, 0x0060200A);
    wr(TCTL, (1u << 1) | (1u << 3) | (0x0F << 4) | (0x40 << 12));      /* EN, PSP, CT, COLD */
    rx_cur = tx_cur = 0;
    nif.hw_up = 1;
    kprintf("[net ] e1000 MAC %02x:%02x:%02x:%02x:%02x:%02x link %s\n", nif.mac[0], nif.mac[1], nif.mac[2],
            nif.mac[3], nif.mac[4], nif.mac[5], (rd(STATUS) & 2) ? "up" : "down");
    return 0;
}

int e1000_tx(const void *frame, size_t len) {
    if (!nif.hw_up || len > 1514) return -1;
    struct txd *d = &txr[tx_cur];
    for (int i = 0; i < 100000 && !(d->status & 1); i++) ;
    if (!(d->status & 1)) return -1;
    memcpy(txbuf[tx_cur], frame, len);
    d->len = (uint16_t)(len < 60 ? 60 : len); if (len < 60) memset(txbuf[tx_cur] + len, 0, 60 - len);
    d->cmd = 0x0B; d->status = 0;                                       /* EOP | IFCS | RS */
    tx_cur = (tx_cur + 1) % NTX; wr(TDT, tx_cur);
    return 0;
}

int e1000_rx(void *buf, size_t cap) {
    if (!nif.hw_up) return 0;
    struct rxd *d = &rxr[rx_cur];
    if (!(d->status & 1)) return 0;
    int ret = -1;                                                       /* -1: frame consumed but unusable */
    if (d->len <= cap && (d->status & 2) && !d->errors) { memcpy(buf, rxbuf[rx_cur], d->len); ret = d->len; }
    d->status = 0; wr(RDT, rx_cur); rx_cur = (rx_cur + 1) % NRX;
    return ret;
}
