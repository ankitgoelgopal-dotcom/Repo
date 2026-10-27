#ifndef QAR_KERNEL_H
#define QAR_KERNEL_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define QAR_OS_NAME    "QAR-OS"
#define QAR_OS_VERSION "0.1.0-phase1"

/* port I/O */
static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }

/* serial (COM1) */
void serial_init(void);
void serial_putc(char c);

/* VGA text console */
void vga_init(void);
void vga_putc(char c);

/* kernel log: goes to serial + VGA */
void kputc(char c);
int  kprintf(const char *fmt, ...);
void kpanic(const char *msg) __attribute__((noreturn));

/* freestanding string helpers */
size_t kstrlen(const char *s);
int    kstrncmp(const char *a, const char *b, size_t n);
const char *kstrstr(const char *h, const char *n);

#endif
