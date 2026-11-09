/* QAR-OS malloc: boundary-tag allocator (dlmalloc-style) with size-class bins, O(1) coalescing and a "top"
 * chunk that grows through the sbrk syscall. 16-byte aligned. Single-threaded (no locking).
 * Chunk layout: [prev_size][size|flags][payload...]; the payload may use the *next* chunk's prev_size word. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "malloc.h"
#ifdef QAR_MALLOC_TEST
extern long test_sbrk(long inc);
#define SBRK(n) test_sbrk(n)
#else
#include "os_api.h"
#define SBRK(n) os_sbrk_raw(n)
#endif

struct chunk { size_t prev_size, size; struct chunk *fd, *bk; };      /* fd/bk valid only while free */
#define PREV_INUSE 1u
#define FLAGS 7u
#define OVERHEAD 8u
#define MIN_CHUNK 32u
#define NBINS 200
#define GROW_MIN (1u << 20)

static struct chunk *bins[NBINS];
static struct chunk *top;                    /* wilderness; its size word holds the true size */
static int ready;

static inline size_t csize(struct chunk *c) { return c->size & ~(size_t)FLAGS; }
static inline struct chunk *nextc(struct chunk *c) { return (struct chunk *)((char *)c + csize(c)); }
static inline void *c2mem(struct chunk *c) { return (char *)c + 16; }
static inline struct chunk *mem2c(void *p) { return (struct chunk *)((char *)p - 16); }
static inline size_t req2size(size_t n) { size_t s = (n + OVERHEAD + 15) & ~(size_t)15; return s < MIN_CHUNK ? MIN_CHUNK : s; }

static int bin_of(size_t s) {
    if (s < 1024) return (int)(s >> 4);                                 /* exact 16-byte classes: 2..63 */
    int lg = 63 - __builtin_clzl(s);                                    /* >= 10 */
    int b = 64 + (lg - 10) * 4 + (int)((s >> (lg - 2)) & 3);
    return b < NBINS ? b : NBINS - 1;
}
static void bin_insert(struct chunk *c) {
    int i = bin_of(csize(c)); c->fd = bins[i]; c->bk = 0; if (bins[i]) bins[i]->bk = c; bins[i] = c;
}
static void bin_unlink(struct chunk *c) {
    if (c->bk) c->bk->fd = c->fd; else bins[bin_of(csize(c))] = c->fd;
    if (c->fd) c->fd->bk = c->bk;
}
/* mark c (size s, previous chunk in use) as free: write footer, clear next's PREV_INUSE, put in a bin */
static void make_free(struct chunk *c, size_t s) {
    c->size = s | PREV_INUSE;
    struct chunk *n = (struct chunk *)((char *)c + s); n->prev_size = s; n->size &= ~(size_t)PREV_INUSE;
    bin_insert(c);
}
static int grow(size_t need) {
    size_t step = need > GROW_MIN ? need : GROW_MIN; step = (step + 4095) & ~(size_t)4095;
    long old = SBRK((long)step);
    if (old < 0) return -1;
    if (!ready) {                                                       /* first call: top starts at the old break */
        uintptr_t a = ((uintptr_t)old + 15) & ~(uintptr_t)15; size_t lost = a - (uintptr_t)old;
        top = (struct chunk *)a; top->size = (step - lost) | PREV_INUSE; ready = 1; return 0;
    }
    if ((char *)old != (char *)top + csize(top)) return -1;             /* break moved by someone else */
    top->size += step; return 0;
}
void *malloc(size_t n) {
    if (n > ((size_t)1 << 40)) return 0;
    size_t s = req2size(n); struct chunk *c = 0;
    for (int i = bin_of(s); i < NBINS && !c; i++)
        for (struct chunk *b = bins[i]; b; b = b->fd) if (csize(b) >= s) { c = b; break; }
    if (c) {
        bin_unlink(c); size_t cs = csize(c);
        if (cs - s >= MIN_CHUNK) {
            c->size = s | PREV_INUSE;
            struct chunk *r = (struct chunk *)((char *)c + s); make_free(r, cs - s);
            r->size = (cs - s) | PREV_INUSE;
        } else { c->size = cs | PREV_INUSE; nextc(c)->size |= PREV_INUSE; }
        return c2mem(c);
    }
    if (!ready || csize(top) < s + MIN_CHUNK) { if (grow(ready ? s + MIN_CHUNK - csize(top) : s + MIN_CHUNK + 16)) return 0; }
    c = top; size_t ts = csize(top), pf = top->size & PREV_INUSE;
    top = (struct chunk *)((char *)c + s); top->size = (ts - s) | PREV_INUSE;
    c->size = s | pf; return c2mem(c);
}
void free(void *p) {
    if (!p) return;
    struct chunk *c = mem2c(p); size_t s = csize(c);
    if (!(c->size & PREV_INUSE)) {                                      /* merge backward */
        struct chunk *pc = (struct chunk *)((char *)c - c->prev_size);
        bin_unlink(pc); s += csize(pc); c = pc;
    }
    struct chunk *n = (struct chunk *)((char *)c + s);
    if (n == top) { c->size = (s + csize(top)) | PREV_INUSE; top = c; return; }   /* merge into the wilderness */
    struct chunk *nn = (struct chunk *)((char *)n + csize(n));          /* n is free iff the chunk after it says so */
    if (!(nn->size & PREV_INUSE)) { bin_unlink(n); s += csize(n); }     /* (nn may be top: its flag is maintained) */
    make_free(c, s);
}
size_t malloc_usable_size(void *p) { return p ? csize(mem2c(p)) - OVERHEAD : 0; }
void *calloc(size_t n, size_t sz) {
    if (sz && n > (size_t)-1 / sz) return 0;
    void *p = malloc(n * sz); if (p) memset(p, 0, n * sz); return p;
}
void *realloc(void *p, size_t n) {
    if (!p) return malloc(n);
    if (!n) { free(p); return 0; }
    struct chunk *c = mem2c(p); size_t have = csize(c), want = req2size(n);
    if (want <= have) return p;
    struct chunk *nx = nextc(c);
    if (nx == top && have + csize(top) >= want + MIN_CHUNK) {           /* grow in place into the wilderness */
        size_t ts = csize(top) - (want - have);
        c->size = want | (c->size & PREV_INUSE);
        top = (struct chunk *)((char *)c + want); top->size = ts | PREV_INUSE; return p;
    }
    void *q = malloc(n); if (!q) return 0;
    memcpy(q, p, have - OVERHEAD); free(p); return q;
}
