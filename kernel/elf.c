#include "kernel.h"
#include "elf.h"
#include "mem.h"
struct ehdr { uint8_t id[16]; uint16_t type, machine; uint32_t ver; uint64_t entry, phoff, shoff;
              uint32_t flags; uint16_t ehsz, phentsz, phnum, shentsz, shnum, shstrndx; } __attribute__((packed));
struct phdr { uint32_t type, flags; uint64_t off, vaddr, paddr, filesz, memsz, align; } __attribute__((packed));

int elf_load(struct aspace *a, const uint8_t *img, uint64_t len, uint64_t *entry) {
    if (len < sizeof(struct ehdr)) return -1;
    const struct ehdr *h = (const void *)img;
    if (memcmp(h->id, "\x7f" "ELF", 4) || h->id[4] != 2 || h->id[5] != 1) return -1;   /* ELF64, LE */
    if (h->type != 2 || h->machine != 62 || h->phentsz != sizeof(struct phdr)) return -2;
    if (h->phoff > len || h->phnum > 16 || h->phoff + (uint64_t)h->phnum * sizeof(struct phdr) > len) return -3;
    for (int i = 0; i < h->phnum; i++) {
        const struct phdr *p = (const void *)(img + h->phoff + i * sizeof *p);
        if (p->type != 1 || !p->memsz) continue;                                        /* PT_LOAD */
        if (p->filesz > p->memsz || p->off + p->filesz > len || p->off + p->filesz < p->off) return -4;
        if (p->vaddr < USER_BASE || p->vaddr + p->memsz > USER_TOP - USER_STACK_PAGES * PAGE) return -5;
        uint64_t start = p->vaddr & ~0xFFFULL, end = (p->vaddr + p->memsz + PAGE - 1) & ~0xFFFULL;
        for (uint64_t va = start; va < end; va += PAGE) {
            uint64_t fr = pmm_alloc(); if (!fr) return -6;
            for (uint64_t b = 0; b < PAGE; b++) {                                       /* copy file bytes that land here */
                uint64_t addr = va + b;
                if (addr >= p->vaddr && addr < p->vaddr + p->filesz) ((uint8_t *)fr)[b] = img[p->off + (addr - p->vaddr)];
            }
            if (aspace_map(a, va, fr, (p->flags & 2) ? PTE_W : 0)) return -7;
        }
    }
    for (int i = 1; i <= USER_STACK_PAGES; i++) {                                        /* user stack */
        uint64_t fr = pmm_alloc(); if (!fr) return -6;
        if (aspace_map(a, USER_TOP - i * PAGE, fr, PTE_W)) return -7;
    }
    if (h->entry < USER_BASE || h->entry >= USER_TOP) return -8;
    *entry = h->entry; return 0;
}
