/* Extra libc pieces needed by larger C libraries (QuickJS etc.): string helpers, FILE output, time, assert. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include "os_api.h"

void *memchr(const void *s, int c, size_t n) { const unsigned char *p = s; for (; n; n--, p++) if (*p == (unsigned char)c) return (void *)p; return 0; }
void *memrchr(const void *s, int c, size_t n) { const unsigned char *p = s; while (n--) if (p[n] == (unsigned char)c) return (void *)(p + n); return 0; }
size_t strnlen(const char *s, size_t n) { size_t i = 0; while (i < n && s[i]) i++; return i; }
char *strrchr(const char *s, int c) { const char *r = 0; do { if (*s == (char)c) r = s; } while (*s++); return (char *)r; }
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strncat(char *d, const char *s, size_t n) { char *e = d + strlen(d); while (n-- && *s) *e++ = *s++; *e = 0; return d; }
char *strdup(const char *s) { size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p; }
static int lc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int strcasecmp(const char *a, const char *b) { while (*a && lc(*a) == lc(*b)) { a++; b++; } return lc((unsigned char)*a) - lc((unsigned char)*b); }
int strncasecmp(const char *a, const char *b, size_t n) { for (; n; n--, a++, b++) { if (lc(*a) != lc(*b)) return lc((unsigned char)*a) - lc((unsigned char)*b); if (!*a) break; } return 0; }
size_t strspn(const char *s, const char *a) { size_t n = 0; while (s[n] && strchr(a, s[n])) n++; return n; }
size_t strcspn(const char *s, const char *r) { size_t n = 0; while (s[n] && !strchr(r, s[n])) n++; return n; }
char *strpbrk(const char *s, const char *a) { s += strcspn(s, a); return *s ? (char *)s : 0; }
char *strerror(int e) { (void)e; return "error"; }
void abort(void) { fputs("abort()\n", stderr); os_exit(134); }
char *getenv(const char *n) { (void)n; return 0; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }
void __assert_fail(const char *e, const char *f, int l, const char *fn) {
    fprintf(stderr, "assertion failed: %s (%s:%d, %s)\n", e, f, l, fn); os_exit(134);
}

/* ---- minimal FILE: stdout/stderr write straight to the console ---- */
struct qar_stdio_file { int fd; };
static struct qar_stdio_file f_in = {0}, f_out = {1}, f_err = {2};
FILE *stdin = &f_in, *stdout = &f_out, *stderr = &f_err;
size_t fwrite(const void *p, size_t sz, size_t n, FILE *f) {
    size_t total = sz * n, done = 0; if (f->fd == 0) return 0;
    while (done < total) { long r = os_write(f->fd, (const char *)p + done, total - done > 4096 ? 4096 : total - done); if (r <= 0) break; done += (size_t)r; }
    return sz ? done / sz : 0;
}
int fputs(const char *s, FILE *f) { return fwrite(s, 1, strlen(s), f) == strlen(s) ? 1 : EOF; }
int fputc(int c, FILE *f) { char ch = (char)c; return fwrite(&ch, 1, 1, f) == 1 ? (unsigned char)c : EOF; }
int putc(int c, FILE *f) { return fputc(c, f); }
int fflush(FILE *f) { (void)f; return 0; }
int vfprintf(FILE *f, const char *fmt, va_list ap) {
    va_list c; va_copy(c, ap); int n = vsnprintf(0, 0, fmt, c); va_end(c);
    if (n <= 0) return n;
    char *b = malloc((size_t)n + 1); if (!b) return -1;
    vsnprintf(b, (size_t)n + 1, fmt, ap); fwrite(b, 1, (size_t)n, f); free(b); return n;
}
int fprintf(FILE *f, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfprintf(f, fmt, ap); va_end(ap); return r; }
int sprintf(char *b, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(b, 1 << 30, fmt, ap); va_end(ap); return r; }

/* ---- time: SYS_TIME_UNIX seconds roll over exactly at uptime multiples of 1000 ms, so uptime%1000 is the sub-second part ---- */
int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz; uint64_t up = os_uptime_ms(); tv->tv_sec = (time_t)os_time_unix(); tv->tv_usec = (long)(up % 1000) * 1000; return 0;
}
int clock_gettime(clockid_t id, struct timespec *ts) {
    uint64_t up = os_uptime_ms();
    if (id == CLOCK_MONOTONIC) { ts->tv_sec = (time_t)(up / 1000); ts->tv_nsec = (long)(up % 1000) * 1000000L; }
    else { ts->tv_sec = (time_t)os_time_unix(); ts->tv_nsec = (long)(up % 1000) * 1000000L; }
    return 0;
}
clock_t clock(void) { return (clock_t)(os_uptime_ms() * 1000); }
int nanosleep(const struct timespec *r, struct timespec *rem) { (void)rem; os_sleep_ms((unsigned)(r->tv_sec * 1000 + r->tv_nsec / 1000000)); return 0; }
struct tm *localtime_r(const time_t *t, struct tm *o) { return gmtime_r(t, o); }
struct tm *gmtime(const time_t *t) { static struct tm tm; return gmtime_r(t, &tm); }
struct tm *localtime(const time_t *t) { return gmtime(t); }
time_t timegm(struct tm *tm) {
    long long y = tm->tm_year + 1900LL, m = tm->tm_mon; y += m / 12; m %= 12; if (m < 0) { m += 12; y--; }
    y -= m < 2; long long era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m < 2 ? 10 : -2)) + 2) / 5 + tm->tm_mday - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; long long days = era * 146097 + (long long)doe - 719468;
    return (time_t)(days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}
time_t mktime(struct tm *tm) { return timegm(tm); }
