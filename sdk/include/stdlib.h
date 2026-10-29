#ifndef QAR_STDLIB_H
#define QAR_STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void  free(void *p);
void *calloc(size_t n, size_t sz);
void *realloc(void *p, size_t n);
int   atoi(const char *s);
int   abs(int x);
void  exit(int code) __attribute__((noreturn));
#endif
