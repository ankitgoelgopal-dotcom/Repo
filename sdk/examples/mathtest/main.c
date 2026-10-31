/* Prints libm results as raw IEEE-754 bit patterns, plus printf/strtod cases; scripts/check_math.py verifies them on the host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <errno.h>
#include <stdint.h>

static uint64_t bits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
#define T1(f, x) printf("M1 " #f " %016llx %016llx\n", (unsigned long long)bits(x), (unsigned long long)bits(f(x)))
#define T2(f, x, y) printf("M2 " #f " %016llx %016llx %016llx\n", (unsigned long long)bits(x), (unsigned long long)bits(y), (unsigned long long)bits(f(x, y)))

int main(void) {
    volatile double v;                       /* volatile: stop the compiler from constant-folding the tests away */
    double in1[] = { 0.5, 1e-5, 3.0, 100.0, -2.5, 1e10, 0.3, -0.9, 1.5, 2.0, 7.25, 1e-300, 27.0, 12345.0, 0.001, 700.0 };
    for (unsigned i = 0; i < sizeof in1 / sizeof *in1; i++) {
        v = in1[i]; double x = v;
        T1(sin, x); T1(cos, x); T1(tan, x); T1(atan, x); T1(exp, x < 700.5 ? x : 1.0); T1(sinh, x < 50 ? x : 1.0);
        T1(cosh, x < 50 ? x : 1.0); T1(tanh, x); T1(cbrt, x); T1(fabs, x); T1(floor, x); T1(ceil, x); T1(trunc, x); T1(round, x);
        if (x > 0) { T1(log, x); T1(log2, x); T1(log10, x); T1(sqrt, x); T1(log1p, x); T1(tgamma, x < 100 ? x : 5.5); T1(lgamma, x); }
        if (x >= -1 && x <= 1) { T1(asin, x); T1(acos, x); T1(erf, x); }
        T1(expm1, x < 50 ? x : 1.0);
    }
    double in2[][2] = { {2, 10}, {2.5, 3.5}, {10, -3}, {0.5, 0.25}, {1, 2}, {-3, -4}, {3, 4}, {10.5, 3}, {-7, 2}, {1e300, 1e300}, {5, 0.5} };
    for (unsigned i = 0; i < sizeof in2 / sizeof *in2; i++) {
        double x = in2[i][0], y = in2[i][1]; volatile double vx = x; x = vx;
        T2(pow, x, y); T2(atan2, x, y); T2(hypot, x, y); T2(fmod, x, y); T2(fmax, x, y); T2(fmin, x, y); T2(copysign, x, y);
    }
    /* special values print as text, not bits */
    v = -1.0; printf("SP sqrt(-1)=%f log(0)=%f pow(0,-1)=%f\n", sqrt(v), log(0.0 * v + 0.0), pow(0.0, v));
    v = 0.0; printf("SP 1/0=%f -1/0=%f 0/0=%f\n", 1.0 / v, -1.0 / v, v / v);
    printf("SP isnan=%d isinf=%d signbit(-0.0)=%d fpclassify(1e-310)=%d\n", isnan(v / v), isinf(1.0 / v), signbit(-1.0 * (v + 0.0) ), fpclassify(1e-310 * (1.0 + v)) == FP_SUBNORMAL);
    feclearexcept(FE_ALL_EXCEPT); v = 1.0; volatile double z = 0.0; volatile double q = v / z; (void)q;
    printf("SP fenv: divbyzero=%d invalid=%d round=%d\n", !!fetestexcept(FE_DIVBYZERO), !!fetestexcept(FE_INVALID), fegetround() == FE_TONEAREST);

    /* printf formatting: Python formats the same value with the same spec and compares */
    static const struct { const char *fmt; double v; } fc[] = {
        {"%f", 3.14159265358979}, {"%.3f", 2.0005}, {"%10.4f", -1234.56789}, {"%-12.2e|", 123456.789}, {"%e", 0.000123456},
        {"%g", 100000.0}, {"%g", 1000000.0}, {"%g", 0.0001}, {"%g", 0.00001}, {"%.10g", 1.0 / 3.0}, {"%+g", 2.5}, {"% f", 42.0},
        {"%.0f", 0.5}, {"%.0f", 1.5}, {"%.0f", 2.5}, {"%f", 1e22}, {"%.20f", 0.1}, {"%e", 1.7976931348623157e308}, {"%g", 5e-324},
        {"%08.3f", -3.14159}, {"%G", 1e-10}, {"%E", 12345.6789}, {"%.15g", 0.1}, {"%.17g", 0.1}, {"%#g", 1.5}, {"%#.0f", 3.0}, {"%f", 123456789.123456789},
    };
    for (unsigned i = 0; i < sizeof fc / sizeof *fc; i++) {
        char b[160]; snprintf(b, sizeof b, fc[i].fmt, fc[i].v);
        printf("F %016llx %s ::%s\n", (unsigned long long)bits(fc[i].v), fc[i].fmt, b);
    }
    static const struct { const char *fmt; long long v; } ic[] = {
        {"%5d|", 42}, {"%-5d|", 42}, {"%05d|", -42}, {"%+d", 7}, {"% d", 7}, {"%x", 255}, {"%#x", 255}, {"%#o", 8}, {"%X", 48879},
        {"%.5d", 42}, {"%8.5d|", 42}, {"%u", 4000000000LL}, {"%lld", 9000000000000000000LL}, {"%ld", -5}, {"%o", 64}, {"%.0d", 0},
    };
    for (unsigned i = 0; i < sizeof ic / sizeof *ic; i++) {
        char b[80]; const char *f = ic[i].fmt;
        if (strstr(f, "ll")) snprintf(b, sizeof b, f, ic[i].v); else if (strchr(f, 'l')) snprintf(b, sizeof b, f, (long)ic[i].v); else snprintf(b, sizeof b, f, (int)ic[i].v);
        printf("I %lld %s ::%s\n", ic[i].v, f, b);
    }
    char sb[64]; snprintf(sb, sizeof sb, "[%10s][%-10s][%.3s][%c][%5c]", "right", "left", "truncate", 'Z', 'y'); printf("S1 %s\n", sb);
    printf("S2 %d %s\n", snprintf(0, 0, "%s", "length probe"), "(snprintf returns the would-be length)");

    /* strtod: exact decimal -> double */
    static const char *sc[] = { "3.14159", "1e300", "-0.000123", "0x1.8p3", "inf", "-INF", "1e-320", "123456789012345678901234567890",
        "2.2250738585072011e-308", "0.1", "  42abc", "1e400", "4.9406564584124654e-324", "9007199254740993", ".5e1", "1.7976931348623157e308" };
    for (unsigned i = 0; i < sizeof sc / sizeof *sc; i++) {
        char *end; double d = strtod(sc[i], &end);
        printf("D %016llx %d %s\n", (unsigned long long)bits(d), (int)(end - sc[i]), sc[i]);
    }
    printf("L %ld %ld %lu %lld %ld\n", strtol("  -123xyz", 0, 10), strtol("0x1F", 0, 0), strtoul("4294967295", 0, 10), strtoll("-9223372036854775808", 0, 10), strtol("777", 0, 8));
    int arr[] = { 5, 3, 9, 1, 7, 2, 8 }; int cmp_i(const void *a, const void *b);
    qsort(arr, 7, sizeof arr[0], cmp_i); printf("Q %d %d %d %d %d %d %d\n", arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6]);
    printf("MATHTEST DONE\n");
    return 0;
}
int cmp_i(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
