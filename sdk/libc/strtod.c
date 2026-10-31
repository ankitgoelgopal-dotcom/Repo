/* strtod/strtof/atof: exact (correctly rounded) decimal and hex parsing via musl's floatscan (MIT). */
#include <stdlib.h>
#include <stddef.h>
#include "../../third_party/musl-math/scan/shgetc.h"
#include "../../third_party/musl-math/scan/floatscan.h"
static long double strtox(const char *s, char **p, int prec) {
    FILE f;
    sh_fromstring(&f, s);
    shlim(&f, 0);
    long double y = __floatscan(&f, prec, 1);
    off_t cnt = shcnt(&f);
    if (p) *p = cnt ? (char *)s + cnt : (char *)s;
    return y;
}
float strtof(const char *s, char **p) { return (float)strtox(s, p, 0); }
double strtod(const char *s, char **p) { return (double)strtox(s, p, 1); }
double atof(const char *s) { return strtod(s, 0); }
