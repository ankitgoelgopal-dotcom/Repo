/* x87/SSE state management. The kernel itself never uses FP/SSE (built with -mgeneral-regs-only), so user
 * state survives interrupts untouched and only needs saving when the scheduler switches tasks (eager FXSAVE). */
#include "kernel.h"
#include "cpu.h"
#include "mem.h"

uint8_t fpu_default[512] __attribute__((aligned(16)));

void fpu_init(void) {
    uint32_t a = 1, b, c, d;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
    if (!(d & (1u << 24)) || !(d & (1u << 26))) kpanic("CPU lacks FXSR/SSE2");
    uint64_t cr0, cr4;
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);            /* EM = 0: no x87 emulation */
    cr0 |= (1ULL << 1);             /* MP = 1 */
    cr0 &= ~(1ULL << 3);            /* TS = 0 */
    __asm__ volatile("mov %0,%%cr0" :: "r"(cr0));
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10);   /* OSFXSR | OSXMMEXCPT */
    __asm__ volatile("mov %0,%%cr4" :: "r"(cr4));
    __asm__ volatile("fninit");
    uint32_t mxcsr = 0x1F80;        /* all SIMD exceptions masked, round-to-nearest */
    __asm__ volatile("ldmxcsr %0" :: "m"(mxcsr));
    fxsave(fpu_default);
}
