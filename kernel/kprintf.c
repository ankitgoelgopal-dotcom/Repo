#include "kernel.h"

void kputc(char c) {
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
    vga_putc(c);
}

static void put_num(unsigned long long v, unsigned base, int width, char pad) {
    char buf[32]; int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = "0123456789abcdef"[v % base]; v /= base; }
    while (n < width) buf[n++] = pad;
    while (n--) kputc(buf[n]);
}

int kprintf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    for (; *fmt; fmt++) {
        if (*fmt != '%') { kputc(*fmt); continue; }
        fmt++;
        char pad = ' '; int width = 0, lng = 0;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        while (*fmt == 'l') { lng++; fmt++; }
        switch (*fmt) {
        case 'c': kputc((char)va_arg(ap, int)); break;
        case 's': { const char *s = va_arg(ap, const char *); if (!s) s = "(null)"; while (*s) kputc(*s++); break; }
        case 'd': {
            long long v = lng ? va_arg(ap, long long) : va_arg(ap, int);
            if (v < 0) { kputc('-'); v = -v; }
            put_num((unsigned long long)v, 10, width, pad); break; }
        case 'u': put_num(lng ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), 10, width, pad); break;
        case 'x': put_num(lng ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), 16, width, pad); break;
        case 'p': kputc('0'); kputc('x'); put_num((unsigned long long)va_arg(ap, void *), 16, 16, '0'); break;
        case '%': kputc('%'); break;
        default: kputc('%'); kputc(*fmt); break;
        }
    }
    va_end(ap);
    return 0;
}

void kpanic(const char *msg) {
    kprintf("\n*** KERNEL PANIC: %s ***\n", msg);
    for (;;) __asm__ volatile("cli; hlt");
}

size_t kstrlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int kstrncmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) { if (*a != *b) return (unsigned char)*a - (unsigned char)*b; if (!*a) break; }
    return 0;
}
const char *kstrstr(const char *h, const char *n) {
    size_t nl = kstrlen(n);
    if (!nl) return h;
    for (; *h; h++) if (!kstrncmp(h, n, nl)) return h;
    return 0;
}
