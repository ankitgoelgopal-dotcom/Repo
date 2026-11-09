#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "os_api.h"
/* malloc/free/calloc/realloc live in malloc.c */
int atoi(const char *s) { int v = 0, neg = 0; while (*s == ' ') s++; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return neg ? -v : v; }
int abs(int x) { return x < 0 ? -x : x; }
void exit(int code) { os_exit(code); }
