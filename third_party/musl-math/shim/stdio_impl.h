/* Minimal pseudo-FILE used by musl's float scanner when parsing from a string (see shgetc.h). */
#ifndef QAR_STDIO_IMPL_H
#define QAR_STDIO_IMPL_H
#include <stddef.h>
typedef long off_t;
#define FILE QAR_SCANFILE
typedef struct QAR_SCANFILE {
    unsigned char *buf, *rpos, *rend, *shend;
    off_t shlim, shcnt;
} FILE;
#ifndef EOF
#define EOF (-1)
#endif
static inline int __uflow(FILE *f) { (void)f; return -1; }   /* string input never refills */
#endif
