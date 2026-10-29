#include <string.h>
#include <stdint.h>
void *memcpy(void *d, const void *s, size_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; return d; }
void *memmove(void *d, const void *s, size_t n) {
    uint8_t *a = d; const uint8_t *b = s;
    if (a < b) while (n--) *a++ = *b++; else { a += n; b += n; while (n--) *--a = *--b; }
    return d; }
void *memset(void *d, int c, size_t n) { uint8_t *a = d; while (n--) *a++ = (uint8_t)c; return d; }
int memcmp(const void *a, const void *b, size_t n) { const uint8_t *x = a, *y = b; for (; n; n--, x++, y++) if (*x != *y) return *x - *y; return 0; }
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char *a, const char *b, size_t n) { for (; n; n--, a++, b++) { if (*a != *b) return (unsigned char)*a - (unsigned char)*b; if (!*a) break; } return 0; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) ; return r; }
char *strchr(const char *s, int c) { for (; *s; s++) if (*s == (char)c) return (char *)s; return c ? 0 : (char *)s; }
char *strstr(const char *h, const char *n) { size_t l = strlen(n); if (!l) return (char *)h; for (; *h; h++) if (!strncmp(h, n, l)) return (char *)h; return 0; }
char *strncpy(char *d, const char *s, size_t n) { size_t i = 0; for (; i < n && s[i]; i++) d[i] = s[i]; for (; i < n; i++) d[i] = 0; return d; }
void explicit_bzero(void *p, size_t n) { volatile uint8_t *v = p; while (n--) *v++ = 0; }
