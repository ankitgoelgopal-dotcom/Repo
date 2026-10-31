#ifndef QAR_CPU_H
#define QAR_CPU_H
#include <stdint.h>
struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err, rip, cs, rflags, rsp, ss;
};
void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);
void idt_init(void);
void pic_init(void);
void pit_init(unsigned hz);
extern volatile uint64_t ticks;
#define TICK_HZ 100
void switch_context(uint64_t *old_sp, uint64_t new_sp);
void enter_user(uint64_t rip, uint64_t rsp);
void fpu_init(void);
extern uint8_t fpu_default[512];
static inline void fxsave(void *p)  { __asm__ volatile("fxsave (%0)"  :: "r"(p) : "memory"); }
static inline void fxrstor(void *p) { __asm__ volatile("fxrstor (%0)" :: "r"(p) : "memory"); }
static inline void sti(void) { __asm__ volatile("sti"); }
static inline void cli(void) { __asm__ volatile("cli"); }
#endif
