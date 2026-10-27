#include "kernel.h"
#include "cpu.h"
#include "mem.h"
struct tss { uint32_t r0; uint64_t rsp0, rsp1, rsp2, r1, ist[7], r2; uint16_t r3, iomap; } __attribute__((packed));
static struct tss tss;
static uint64_t gdt[7];
extern void gdt_load(void *), tss_load(void);

void tss_set_rsp0(uint64_t v) { tss.rsp0 = v; }
void gdt_init(void) {
    static uint8_t istack[8192] __attribute__((aligned(16)));
    gdt[0] = 0;
    gdt[1] = 0x00AF9A000000FFFFULL;   /* 0x08 kernel code */
    gdt[2] = 0x00CF92000000FFFFULL;   /* 0x10 kernel data */
    gdt[3] = 0x00CFF2000000FFFFULL;   /* 0x18 user data (DPL3) */
    gdt[4] = 0x00AFFA000000FFFFULL;   /* 0x20 user code (DPL3, 64-bit) */
    memset(&tss, 0, sizeof tss); tss.iomap = sizeof tss; tss.rsp0 = (uint64_t)istack + sizeof istack;
    uint64_t base = (uint64_t)&tss, lim = sizeof tss - 1;
    gdt[5] = (lim & 0xFFFF) | (base & 0xFFFFFF) << 16 | 0x89ULL << 40 | ((lim >> 16) & 0xF) << 48 | ((base >> 24) & 0xFF) << 56;
    gdt[6] = base >> 32;
    struct { uint16_t lim; uint64_t base; } __attribute__((packed)) p = { sizeof gdt - 1, (uint64_t)gdt };
    gdt_load(&p); tss_load();
}
