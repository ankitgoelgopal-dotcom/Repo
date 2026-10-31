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
double atof(const char *s);
double strtod(const char *s, char **end);
float  strtof(const char *s, char **end);
long   atol(const char *s);
long long atoll(const char *s);
long   strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
long long strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);
void  qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));
#endif
