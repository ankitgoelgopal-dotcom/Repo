#include <fenv.h>
static inline uint32_t get(void) { uint32_t v; __asm__ volatile("stmxcsr %0" : "=m"(v)); return v; }
static inline void put(uint32_t v) { __asm__ volatile("ldmxcsr %0" :: "m"(v)); }
int feclearexcept(int e) { put(get() & ~(uint32_t)(e & FE_ALL_EXCEPT)); return 0; }
int feraiseexcept(int e) { put(get() | (uint32_t)(e & FE_ALL_EXCEPT)); return 0; }   /* sets the flag; traps stay masked */
int fetestexcept(int e) { return (int)(get() & (uint32_t)(e & FE_ALL_EXCEPT)); }
int fegetround(void) { return (int)(((get() >> 13) & 3) << 10); }
int fesetround(int r) {
    if (r != FE_TONEAREST && r != FE_DOWNWARD && r != FE_UPWARD && r != FE_TOWARDZERO) return -1;
    put((get() & ~(3u << 13)) | ((uint32_t)(r >> 10) << 13)); return 0;
}
int fegetenv(fenv_t *e) { e->mxcsr = get(); return 0; }
int fesetenv(const fenv_t *e) { put(e == FE_DFL_ENV ? 0x1F80 : e->mxcsr); return 0; }
int feholdexcept(fenv_t *e) { e->mxcsr = get(); put(e->mxcsr & ~(uint32_t)FE_ALL_EXCEPT); return 0; }
int feupdateenv(const fenv_t *e) { uint32_t raised = get() & FE_ALL_EXCEPT; fesetenv(e); feraiseexcept((int)raised); return 0; }
