#include <stdio.h>
#include <string.h>
#include "os_api.h"

struct out { char *buf; size_t cap, len; };
static void put(struct out *o, char c) { if (o->len + 1 < o->cap) o->buf[o->len] = c; o->len++; }
static void num(struct out *o, unsigned long long v, unsigned base, int width, char pad) {
    char t[24]; int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = "0123456789abcdef"[v % base]; v /= base; }
    while (n < width) t[n++] = pad;
    while (n--) put(o, t[n]);
}
int vsnprintf(char *buf, size_t cap, const char *f, va_list ap) {
    struct out o = { buf, cap, 0 };
    for (; *f; f++) {
        if (*f != '%') { put(&o, *f); continue; }
        f++; char pad = ' '; int w = 0, l = 0;
        if (*f == '0') { pad = '0'; f++; }
        while (*f >= '0' && *f <= '9') w = w * 10 + *f++ - '0';
        while (*f == 'l') { l++; f++; }
        switch (*f) {
        case 'c': put(&o, (char)va_arg(ap, int)); break;
        case 's': { const char *s = va_arg(ap, const char *); if (!s) s = "(null)"; while (*s) put(&o, *s++); break; }
        case 'd': { long long v = l ? va_arg(ap, long long) : va_arg(ap, int); if (v < 0) { put(&o, '-'); v = -v; } num(&o, v, 10, w, pad); break; }
        case 'u': num(&o, l ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), 10, w, pad); break;
        case 'x': num(&o, l ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), 16, w, pad); break;
        case 'p': put(&o, '0'); put(&o, 'x'); num(&o, (unsigned long long)va_arg(ap, void *), 16, 16, '0'); break;
        case '%': put(&o, '%'); break;
        default: put(&o, '%'); put(&o, *f); break;
        }
    }
    if (cap) buf[o.len < cap ? o.len : cap - 1] = 0;
    return (int)o.len;
}
int snprintf(char *b, size_t n, const char *f, ...) { va_list ap; va_start(ap, f); int r = vsnprintf(b, n, f, ap); va_end(ap); return r; }
int printf(const char *f, ...) {
    char buf[512]; va_list ap; va_start(ap, f); int r = vsnprintf(buf, sizeof buf, f, ap); va_end(ap);
    size_t n = (size_t)r < sizeof buf ? (size_t)r : sizeof buf - 1; os_write(1, buf, n); return r;
}
int puts(const char *s) { os_write(1, s, strlen(s)); os_write(1, "\n", 1); return 0; }
int putchar(int c) { char ch = (char)c; os_write(1, &ch, 1); return c; }
