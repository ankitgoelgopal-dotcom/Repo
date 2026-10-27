#include "kernel.h"
#include "mm.h"
#include "mem.h"

static uint64_t kernel_cr3;
static uint64_t *kernel_pdpt0;           /* kernel's identity-mapped first GiB (supervisor-only) */

void vmm_init(void) {
    __asm__ volatile("mov %%cr3,%0" : "=r"(kernel_cr3));
    kernel_pdpt0 = (uint64_t *)(((uint64_t *)kernel_cr3)[0] & ~0xFFFULL);
}
static uint64_t *tbl(void) { return (uint64_t *)pmm_alloc(); }

struct aspace *aspace_create(void) {
    struct aspace *a = kmalloc(sizeof *a);
    if (!a) return 0;
    a->pml4 = tbl(); a->pdpt = tbl(); a->pd = tbl();
    if (!a->pml4 || !a->pdpt || !a->pd) return 0;
    a->pml4[0] = (uint64_t)a->pdpt | PTE_P | PTE_W | PTE_U;
    a->pdpt[0] = kernel_pdpt0[0];                         /* shared kernel mapping, no U bit */
    a->pdpt[1] = (uint64_t)a->pd | PTE_P | PTE_W | PTE_U; /* private user GiB */
    return a;
}
void aspace_destroy(struct aspace *a) {
    if (!a) return;
    for (int i = 0; i < 512; i++) if (a->pd[i] & PTE_P) {
        uint64_t *pt = (uint64_t *)(a->pd[i] & ~0xFFFULL);
        for (int j = 0; j < 512; j++) if (pt[j] & PTE_P) pmm_free(pt[j] & ~0xFFFULL);
        pmm_free((uint64_t)pt);
    }
    pmm_free((uint64_t)a->pd); pmm_free((uint64_t)a->pdpt); pmm_free((uint64_t)a->pml4);
    kfree(a);
}
void aspace_activate(struct aspace *a) {
    uint64_t cr3 = a ? (uint64_t)a->pml4 : kernel_cr3;
    __asm__ volatile("mov %0,%%cr3" :: "r"(cr3) : "memory");
}
int aspace_map(struct aspace *a, uint64_t va, uint64_t pa, uint64_t flags) {
    if (va < USER_BASE || va >= USER_TOP || (va & 0xFFF)) return -1;
    uint64_t off = va - USER_BASE; int di = (off >> 21) & 0x1FF, ti = (off >> 12) & 0x1FF;
    if (!(a->pd[di] & PTE_P)) {
        uint64_t *pt = tbl(); if (!pt) return -1;
        a->pd[di] = (uint64_t)pt | PTE_P | PTE_W | PTE_U;
    }
    ((uint64_t *)(a->pd[di] & ~0xFFFULL))[ti] = pa | flags | PTE_P | PTE_U;
    return 0;
}
int user_range_ok(struct aspace *a, uint64_t va, uint64_t len, int write) {
    if (!a || va < USER_BASE || va + len < va || va + len > USER_TOP) return 0;
    for (uint64_t p = va & ~0xFFFULL; p < va + len; p += PAGE) {
        uint64_t off = p - USER_BASE; int di = (off >> 21) & 0x1FF, ti = (off >> 12) & 0x1FF;
        if (!(a->pd[di] & PTE_P)) return 0;
        uint64_t e = ((uint64_t *)(a->pd[di] & ~0xFFFULL))[ti];
        if (!(e & PTE_P) || !(e & PTE_U) || (write && !(e & PTE_W))) return 0;
    }
    return 1;
}
