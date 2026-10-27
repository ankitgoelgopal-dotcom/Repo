#include "kernel.h"
#include "mm.h"
#include "mem.h"
#include "multiboot2.h"

#define MAX_FRAMES (1ULL << 18)           /* 1 GiB / 4 KiB */
static uint8_t bitmap[MAX_FRAMES / 8];    /* 1 = used */
static uint64_t nfree;

static void mark(uint64_t f, int used) {
    int was = bitmap[f >> 3] >> (f & 7) & 1;
    if (used && !was) { bitmap[f >> 3] |= 1 << (f & 7); nfree--; }
    if (!used && was) { bitmap[f >> 3] &= ~(1 << (f & 7)); nfree++; }
}
void pmm_reserve(uint64_t s, uint64_t e) {
    for (uint64_t f = s / PAGE; f < (e + PAGE - 1) / PAGE && f < MAX_FRAMES; f++) mark(f, 1);
}
void pmm_init(uint64_t mbi_addr) {
    memset(bitmap, 0xFF, sizeof bitmap); nfree = 0;
    struct mb2_info *info = (struct mb2_info *)mbi_addr;
    for (struct mb2_tag *t = (struct mb2_tag *)(mbi_addr + 8); t->type != MB2_TAG_END;
         t = (struct mb2_tag *)(((uint64_t)t + t->size + 7) & ~7ull)) {
        if (t->type != MB2_TAG_MMAP) continue;
        struct mb2_mmap_tag *m = (struct mb2_mmap_tag *)t;
        for (uint8_t *p = (uint8_t *)(m + 1); p < (uint8_t *)t + t->size; p += m->entry_size) {
            struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
            if (e->type != MB2_MMAP_AVAILABLE) continue;
            uint64_t s = (e->addr + PAGE - 1) / PAGE, en = (e->addr + e->len) / PAGE;
            for (uint64_t f = s; f < en && f < MAX_FRAMES; f++) mark(f, 0);
        }
    }
    pmm_reserve(0, 0x100000);                              /* BIOS, VGA, low mem */
    extern char __kernel_start[], __kernel_end[];
    pmm_reserve((uint64_t)__kernel_start, (uint64_t)__kernel_end);
    pmm_reserve(mbi_addr, mbi_addr + info->total_size);    /* boot info */
}
uint64_t pmm_alloc(void) {
    for (uint64_t f = 0; f < MAX_FRAMES; f++) if (!(bitmap[f >> 3] >> (f & 7) & 1)) {
        mark(f, 1); memset((void *)(f * PAGE), 0, PAGE); return f * PAGE; }
    return 0;
}
uint64_t pmm_alloc_contig(size_t n) {
    uint64_t run = 0;
    for (uint64_t f = 0; f < MAX_FRAMES; f++) {
        if (bitmap[f >> 3] >> (f & 7) & 1) { run = 0; continue; }
        if (++run == n) {
            uint64_t s = f + 1 - n;
            for (uint64_t i = s; i <= f; i++) mark(i, 1);
            memset((void *)(s * PAGE), 0, n * PAGE); return s * PAGE;
        }
    }
    return 0;
}
void pmm_free(uint64_t p) { if (p) mark(p / PAGE, 0); }
uint64_t pmm_free_frames(void) { return nfree; }
