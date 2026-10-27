#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "os_api.h"
/* Simple first-fit allocator over a static 256 KiB arena (no sbrk syscall yet). */
struct blk { size_t size; int free; struct blk *next; };
static uint8_t arena[256 * 1024] __attribute__((aligned(16)));
static struct blk *head;
void *malloc(size_t n) {
    if (!head) { head = (struct blk *)arena; head->size = sizeof arena - sizeof *head; head->free = 1; head->next = 0; }
    n = (n + 15) & ~15ul;
    for (struct blk *b = head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + sizeof *b + 16) {
            struct blk *r = (struct blk *)((uint8_t *)(b + 1) + n);
            r->size = b->size - n - sizeof *b; r->free = 1; r->next = b->next; b->size = n; b->next = r;
        }
        b->free = 0; return b + 1;
    }
    return 0;
}
void free(void *p) {
    if (!p) return;
    ((struct blk *)p - 1)->free = 1;
    for (struct blk *c = head; c; c = c->next)
        while (c->free && c->next && c->next->free) { c->size += sizeof *c + c->next->size; c->next = c->next->next; }
}
void *calloc(size_t n, size_t sz) { void *p = malloc(n * sz); if (p) memset(p, 0, n * sz); return p; }
int atoi(const char *s) { int v = 0, neg = 0; while (*s == ' ') s++; if (*s == '-') { neg = 1; s++; } while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0'; return neg ? -v : v; }
int abs(int x) { return x < 0 ? -x : x; }
void exit(int code) { os_exit(code); }
