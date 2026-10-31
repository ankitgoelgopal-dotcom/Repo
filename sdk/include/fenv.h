/* QAR-OS <fenv.h>: floating-point environment via the SSE MXCSR register (double/float arithmetic). */
#ifndef QAR_FENV_H
#define QAR_FENV_H
#include <stdint.h>
#define FE_INVALID    1
#define FE_DENORMAL   2
#define FE_DIVBYZERO  4
#define FE_OVERFLOW   8
#define FE_UNDERFLOW  16
#define FE_INEXACT    32
#define FE_ALL_EXCEPT 63
#define FE_TONEAREST  0
#define FE_DOWNWARD   0x400
#define FE_UPWARD     0x800
#define FE_TOWARDZERO 0xc00
typedef struct { uint32_t mxcsr; } fenv_t;
typedef unsigned short fexcept_t;
#define FE_DFL_ENV ((const fenv_t *)-1)
int feclearexcept(int);
int feraiseexcept(int);
int fetestexcept(int);
int fegetround(void);
int fesetround(int);
int fegetenv(fenv_t *);
int fesetenv(const fenv_t *);
int feholdexcept(fenv_t *);
int feupdateenv(const fenv_t *);
#endif
