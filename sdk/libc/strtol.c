#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
static unsigned long long conv(const char *s, char **end, int base, int *neg, int *ovf, unsigned long long limit) {
    const char *p = s; *neg = 0; *ovf = 0;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '-' || *p == '+') { *neg = *p == '-'; p++; }
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && isxdigit((unsigned char)p[2])) { p += 2; base = 16; }
    else if (base == 0) base = p[0] == '0' ? 8 : 10;
    unsigned long long v = 0; int any = 0;
    for (;; p++) {
        int c = (unsigned char)*p, d;
        if (isdigit(c)) d = c - '0'; else if (isalpha(c)) d = tolower(c) - 'a' + 10; else break;
        if (d >= base) break;
        any = 1;
        if (v > (limit - (unsigned)d) / (unsigned)base) *ovf = 1; else v = v * (unsigned)base + (unsigned)d;
    }
    if (end) *end = (char *)(any ? p : s);
    return v;
}
long long strtoll(const char *s, char **end, int base) {
    int neg, ovf; unsigned long long lim = (unsigned long long)LLONG_MAX + 1, v = conv(s, end, base, &neg, &ovf, lim);
    if (ovf || (!neg && v > (unsigned long long)LLONG_MAX) || v > lim) { errno = ERANGE; return neg ? LLONG_MIN : LLONG_MAX; }
    return neg ? (long long)(0ULL - v) : (long long)v;
}
unsigned long long strtoull(const char *s, char **end, int base) {
    int neg, ovf; unsigned long long v = conv(s, end, base, &neg, &ovf, ULLONG_MAX);
    if (ovf) { errno = ERANGE; return ULLONG_MAX; }
    return neg ? 0ULL - v : v;
}
long strtol(const char *s, char **end, int base) { return (long)strtoll(s, end, base); }
unsigned long strtoul(const char *s, char **end, int base) { return (unsigned long)strtoull(s, end, base); }
long atol(const char *s) { return strtol(s, 0, 10); }
long long atoll(const char *s) { return strtoll(s, 0, 10); }
