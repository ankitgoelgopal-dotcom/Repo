#include "kernel.h"
#include "cpu.h"
#include "sched.h"
#include "rtc.h"
struct gate { uint16_t lo, sel; uint8_t ist, flags; uint16_t mid; uint32_t hi, zero; } __attribute__((packed));
static struct gate idt[256];
extern uint64_t isr_table[48];
extern void isr128(void), idt_load(void *);
volatile uint64_t ticks;

static void set(int n, uint64_t h, uint8_t flags) {
    idt[n] = (struct gate){ h & 0xFFFF, 0x08, 0, flags, (h >> 16) & 0xFFFF, h >> 32, 0 };
}
void idt_init(void) {
    for (int i = 0; i < 48; i++) set(i, isr_table[i], 0x8E);
    set(128, (uint64_t)isr128, 0xEE);                      /* int 0x80, callable from ring 3 */
    struct { uint16_t lim; uint64_t base; } __attribute__((packed)) p = { sizeof idt - 1, (uint64_t)idt };
    idt_load(&p);
}
/* 8259 PIC remap to vectors 32..47, PIT channel 0 as the scheduler tick */
void pic_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); outb(0x21, 32); outb(0xA1, 40);
    outb(0x21, 4); outb(0xA1, 2); outb(0x21, 1); outb(0xA1, 1);
    outb(0x21, 0xFE); outb(0xA1, 0xFF);                    /* only IRQ0 (timer) unmasked */
}
void pit_init(unsigned hz) {
    unsigned d = 1193182 / hz;
    outb(0x43, 0x36); outb(0x40, d & 0xFF); outb(0x40, d >> 8);
}

extern long syscall_dispatch(struct regs *r);

void isr_handler(struct regs *r) {
    if (r->int_no == 32) {                                 /* timer */
        outb(0x20, 0x20); ticks++; if (!(ticks & 7)) entropy_event(); sched_tick(); return;
    }
    if (r->int_no == 128) { r->rax = (uint64_t)syscall_dispatch(r); return; }
    if (r->int_no >= 32) { outb(0x20, 0x20); return; }     /* spurious / unhandled IRQ */
    uint64_t cr2 = 0; __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    if (r->cs & 3) {                                       /* fault in user mode: kill the process, keep the OS alive */
        kprintf("[proc] pid %d faulted: vector %lu err %lx rip %p addr %p -> killed\n",
                current_pid(), r->int_no, r->err, (void *)r->rip, (void *)cr2);
        task_exit(-11);
    }
    kprintf("\nEXCEPTION %lu err=%lx rip=%p cr2=%p\n", r->int_no, r->err, (void *)r->rip, (void *)cr2);
    kpanic("unhandled kernel exception");
}
