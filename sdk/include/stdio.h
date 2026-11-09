#ifndef QAR_STDIO_H
#define QAR_STDIO_H
#include <stddef.h>
#include <stdarg.h>
int printf(const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int vprintf(const char *fmt, va_list ap);
int puts(const char *s);
int putchar(int c);
#ifndef QAR_NO_FILE            /* musl's float scanner brings its own pseudo-FILE (see third_party/musl-math/shim) */
typedef struct qar_stdio_file FILE;
extern FILE *stdin, *stdout, *stderr;
#define EOF (-1)
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int fputs(const char *s, FILE *f);
int fputc(int c, FILE *f);
int putc(int c, FILE *f);
size_t fwrite(const void *p, size_t size, size_t n, FILE *f);
int fflush(FILE *f);
int sprintf(char *buf, const char *fmt, ...);
#endif
#endif
