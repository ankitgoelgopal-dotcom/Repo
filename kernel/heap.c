#include "kernel.h"
#include "mm.h"
/* First-fit kernel heap with block splitting and forward coalescing. */
struct blk { size_t size; int free; struct blk *next; };
static struct blk *head;
#define HEAP_FRAMES 1024                  /* 4 MiB */
void heap_init(void) {
    uint64_t base = pmm_alloc_contig(HEAP_FRAMES);
    if (!base) kpanic("heap: out of memory");
    head = (struct blk *)base; head->size = HEAP_FRAMES * PAGE - sizeof(struct blk); head->free = 1; head->next = 0;
}
void *kmalloc(size_t n) {
    n = (n + 15) & ~15ul;
    for (struct blk *b = head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + sizeof(struct blk) + 16) {
            struct blk *r = (struct blk *)((uint8_t *)(b + 1) + n);
            r->size = b->size - n - sizeof(struct blk); r->free = 1; r->next = b->next;
            b->size = n; b->next = r;
        }
        b->free = 0; return b + 1;
    }
    return 0;
}
void kfree(void *p) {
    if (!p) return;
    struct blk *b = (struct blk *)p - 1; b->free = 1;
    for (struct blk *c = head; c; c = c->next)
        while (c->free && c->next && c->next->free) { c->size += sizeof(struct blk) + c->next->size; c->next = c->next->next; }
}
