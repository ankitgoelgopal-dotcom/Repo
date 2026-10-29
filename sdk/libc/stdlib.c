#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "os_api.h"
/* First-fit allocator on top of the kernel heap (sbrk). The heap grows in 256 KiB steps and is never returned. */
struct blk { size_t size; int free; struct blk *next; };
static struct blk *head, *tail;
static int grow(size_t need) {
    size_t step = (need + sizeof(struct blk) + 262143) & ~(size_t)262143;
    long old = os_sbrk_raw((long)step);
    if (old < 0) return -1;
    struct blk *b = (struct blk *)old; b->size = step - sizeof *b; b->free = 1; b->next = 0;
    if (tail) tail->next = b; else head = b;
    tail = b;
    for (struct blk *c = head; c; c = c->next)                      /* merge adjacent free blocks */
        while (c->free && c->next && c->next->free && (uint8_t *)(c + 1) + c->size == (uint8_t *)c->next) {
            c->size += sizeof *c + c->next->size; if (c->next == tail) tail = c; c->next = c->next->next; }
    return 0;
}
void *malloc(size_t n) {
    n = (n + 15) & ~(size_t)15; if (!n) n = 16;
    for (int pass = 0; pass < 2; pass++) {
        for (struct blk *b = head; b; b = b->next) {
            if (!b->free || b->size < n) continue;
            if (b->size >= n + sizeof *b + 16) {
                struct blk *r = (struct blk *)((uint8_t *)(b + 1) + n);
                r->size = b->size - n - sizeof *b; r->free = 1; r->next = b->next;
                if (b == tail) tail = r;
                b->size = n; b->next = r;
            }
            b->free = 0; return b + 1;
        }
        if (grow(n)) return 0;
    }
    return 0;
}
void free(void *p) {
    if (!p) return;
    ((struct blk *)p - 1)->free = 1;
    for (struct blk *c = head; c; c = c->next)
        while (c->free && c->next && c->next->free && (uint8_t *)(c + 1) + c->size == (uint8_t *)c->next) {
            c->size += sizeof *c + c->next->size; if (c->next == tail) tail = c; c->next = c->next->next; }
}
void *calloc(size_t n, size_t sz) {
    if (sz && n > (size_t)-1 / sz) return 0;
    void *p = malloc(n * sz); if (p) memset(p, 0, n * sz); return p;
}
void *realloc(void *p, size_t n) {
    if (!p) return malloc(n);
    struct blk *b = (struct blk *)p - 1; if (b->size >= n) return p;
    void *q = malloc(n); if (!q) return 0; memcpy(q, p, b->size); free(p); return q;
}
int atoi(const char *s) { int v = 0, neg = 0; while (*s == ' ') s++; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return neg ? -v : v; }
int abs(int x) { return x < 0 ? -x : x; }
void exit(int code) { os_exit(code); }
