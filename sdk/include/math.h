/* QAR-OS <math.h>: C99 double/float math, implemented by musl's libm (third_party/musl-math). */
#ifndef QAR_MATH_H
#define QAR_MATH_H
#ifdef __cplusplus
extern "C" {
#endif
typedef float float_t;
typedef double double_t;
#define FLT_EVAL_METHOD_QAR 0

#define M_E 2.7182818284590452354
#define M_LOG2E 1.4426950408889634074
#define M_LOG10E 0.43429448190325182765
#define M_LN2 0.69314718055994530942
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.78539816339744830962
#define M_1_PI 0.31830988618379067154
#define M_2_PI 0.63661977236758134308
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2 1.41421356237309504880
#define M_SQRT1_2 0.70710678118654752440

#define HUGE_VAL  (__builtin_huge_val())
#define HUGE_VALF (__builtin_huge_valf())
#define INFINITY  (__builtin_inff())
#define NAN       (__builtin_nanf(""))

#define FP_NAN       0
#define FP_INFINITE  1
#define FP_ZERO      2
#define FP_SUBNORMAL 3
#define FP_NORMAL    4
#define FP_ILOGB0 (-1 - 0x7fffffff)
#define FP_ILOGBNAN (-1 - 0x7fffffff)
#define MATH_ERRNO 1
#define MATH_ERREXCEPT 2
#define math_errhandling 2

#define fpclassify(x) __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x)
#define isinf(x)      __builtin_isinf(x)
#define isnan(x)      __builtin_isnan(x)
#define isnormal(x)   __builtin_isnormal(x)
#define isfinite(x)   __builtin_isfinite(x)
#define signbit(x)    __builtin_signbit(x)
#define isgreater(x,y)      __builtin_isgreater(x,y)
#define isgreaterequal(x,y) __builtin_isgreaterequal(x,y)
#define isless(x,y)         __builtin_isless(x,y)
#define islessequal(x,y)    __builtin_islessequal(x,y)
#define islessgreater(x,y)  __builtin_islessgreater(x,y)
#define isunordered(x,y)    __builtin_isunordered(x,y)

#define QAR_M1(f) double f(double); float f##f(float);
QAR_M1(acos) QAR_M1(acosh) QAR_M1(asin) QAR_M1(asinh) QAR_M1(atan) QAR_M1(atanh) QAR_M1(cbrt) QAR_M1(ceil)
QAR_M1(cos) QAR_M1(cosh) QAR_M1(erf) QAR_M1(erfc) QAR_M1(exp) QAR_M1(exp2) QAR_M1(expm1) QAR_M1(fabs)
QAR_M1(floor) QAR_M1(lgamma) QAR_M1(log) QAR_M1(log10) QAR_M1(log1p) QAR_M1(log2) QAR_M1(logb)
QAR_M1(nearbyint) QAR_M1(rint) QAR_M1(round) QAR_M1(sin) QAR_M1(sinh) QAR_M1(sqrt) QAR_M1(tan) QAR_M1(tanh)
QAR_M1(tgamma) QAR_M1(trunc) QAR_M1(exp10) QAR_M1(j0) QAR_M1(j1) QAR_M1(y0) QAR_M1(y1)
#undef QAR_M1
#define QAR_M2(f) double f(double, double); float f##f(float, float);
QAR_M2(atan2) QAR_M2(copysign) QAR_M2(fdim) QAR_M2(fmax) QAR_M2(fmin) QAR_M2(fmod) QAR_M2(hypot)
QAR_M2(nextafter) QAR_M2(pow) QAR_M2(remainder)
#undef QAR_M2
double fma(double, double, double);          float fmaf(float, float, float);
double frexp(double, int *);                 float frexpf(float, int *);
double ldexp(double, int);                   float ldexpf(float, int);
double scalbn(double, int);                  float scalbnf(float, int);
double scalbln(double, long);                float scalblnf(float, long);
double modf(double, double *);               float modff(float, float *);
double remquo(double, double, int *);        float remquof(float, float, int *);
int ilogb(double);                           int ilogbf(float);
long lrint(double);                          long lrintf(float);
long long llrint(double);                    long long llrintf(float);
long lround(double);                         long lroundf(float);
long long llround(double);                   long long llroundf(float);
double nan(const char *);                    float nanf(const char *);
double lgamma_r(double, int *);              float lgammaf_r(float, int *);
double jn(int, double);                      double yn(int, double);
void sincos(double, double *, double *);     void sincosf(float, float *, float *);
long double fabsl(long double);  long double copysignl(long double, long double);
long double fmodl(long double, long double); long double scalbnl(long double, int);
extern int signgam;
#ifdef __cplusplus
}
#endif
#endif
