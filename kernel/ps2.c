/* PS/2 controller, keyboard (scancode set 1 via controller translation) and mouse (IntelliMouse wheel if available). */
#include "kernel.h"
#include "ps2.h"

#define DATA 0x60
#define CMD  0x64
#define QN 256
static struct raw_ev q[QN]; static volatile unsigned qh, qt;
static int mouse_len = 3;

static void push(const struct raw_ev *e) { unsigned n = (qt + 1) % QN; if (n == qh) return; q[qt] = *e; qt = n; }

int ps2_pop(struct raw_ev *out) {
    uint64_t f; __asm__ volatile("pushfq; pop %0; cli" : "=r"(f) :: "memory");
    int ok = 0;
    if (qh != qt) { *out = q[qh]; qh = (qh + 1) % QN; ok = 1; }
    if (f & 0x200) __asm__ volatile("sti" ::: "memory");
    return ok;
}

static int wait_in(void)  { for (int i = 0; i < 100000; i++) if (!(inb(CMD) & 2)) return 1; return 0; }
static int wait_out(void) { for (int i = 0; i < 100000; i++) if (inb(CMD) & 1) return 1; return 0; }
static void flush(void) { for (int i = 0; i < 64 && (inb(CMD) & 1); i++) (void)inb(DATA); }
static void ctl(uint8_t c) { wait_in(); outb(CMD, c); }
static int kbd_send(uint8_t b) { wait_in(); outb(DATA, b); return wait_out() ? inb(DATA) : -1; }
static int aux_send(uint8_t b) { ctl(0xD4); wait_in(); outb(DATA, b); return wait_out() ? inb(DATA) : -1; }

int ps2_init(void) {
    int ok = 0;
    flush();
    ctl(0xAD); ctl(0xA7); flush();                         /* disable both ports while configuring */
    ctl(0x20); uint8_t cfg = wait_out() ? inb(DATA) : 0;
    cfg |= 0x03 | 0x40;                                    /* IRQ1 + IRQ12 on, scancode translation to set 1 */
    cfg &= (uint8_t)~0x30;                                 /* both clocks on */
    ctl(0x60); wait_in(); outb(DATA, cfg);
    ctl(0xAE); ctl(0xA8);
    if (kbd_send(0xF4) == 0xFA) ok |= 1;                   /* enable scanning */
    if (aux_send(0xF6) == 0xFA) {                          /* mouse defaults */
        aux_send(0xF3); aux_send(200); aux_send(0xF3); aux_send(100); aux_send(0xF3); aux_send(80);
        int id = -1; if (aux_send(0xF2) == 0xFA && wait_out()) id = inb(DATA);
        if (id == 3) mouse_len = 4;
        aux_send(0xF3); aux_send(100);
        if (aux_send(0xF4) == 0xFA) ok |= 2;               /* enable reporting */
    }
    flush();
    kprintf("[ps2 ] keyboard %s, mouse %s%s\n", (ok & 1) ? "ok" : "not found", (ok & 2) ? "ok" : "not found", mouse_len == 4 ? " (wheel)" : "");
    return ok;
}

static void mouse_byte(uint8_t b) {
    static uint8_t pkt[4]; static int idx;
    if (idx == 0 && !(b & 8)) return;                      /* out of sync: first byte always has bit 3 */
    pkt[idx++] = b;
    if (idx < mouse_len) return;
    idx = 0;
    if (pkt[0] & 0xC0) return;                             /* overflow: drop */
    struct raw_ev e = { .kind = RAW_MOUSE };
    int dx = pkt[1] - ((pkt[0] << 4) & 0x100), dy = pkt[2] - ((pkt[0] << 3) & 0x100);
    e.dx = (int16_t)dx; e.dy = (int16_t)dy; e.buttons = pkt[0] & 7;
    if (mouse_len == 4) { int z = pkt[3] & 0x0F; if (z & 8) z -= 16; e.dz = (int8_t)z; }
    push(&e);
}

static void kbd_byte(uint8_t b) {
    static int ext;
    if (b == 0xE0) { ext = 1; return; }
    if (b == 0xE1 || b == 0xFA || b == 0xFE || b == 0xAA) { ext = 0; return; }
    struct raw_ev e = { .kind = RAW_KEY, .scancode = b & 0x7F, .ext = (uint8_t)ext, .release = (b & 0x80) != 0 };
    ext = 0; push(&e);
}

void ps2_irq(void) {
    for (int i = 0; i < 32; i++) {
        uint8_t st = inb(CMD);
        if (!(st & 1)) break;
        uint8_t b = inb(DATA);
        if (st & 0x20) mouse_byte(b); else kbd_byte(b);
    }
}
