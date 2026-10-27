#ifndef QAR_MM_H
#define QAR_MM_H
#include <stdint.h>
#include <stddef.h>
#define PAGE 4096ULL
#define USER_BASE 0x40000000ULL     /* user space: [1 GiB, 1 GiB + 256 MiB) */
#define USER_TOP  0x50000000ULL
#define USER_STACK_PAGES 16
#define PTE_P 1ULL
#define PTE_W 2ULL
#define PTE_U 4ULL

/* physical memory (identity-mapped below 1 GiB for now) */
void     pmm_init(uint64_t mbi);
void     pmm_reserve(uint64_t start, uint64_t end);
uint64_t pmm_alloc(void);                 /* zeroed frame, 0 on OOM */
uint64_t pmm_alloc_contig(size_t frames); /* zeroed, 0 on failure */
void     pmm_free(uint64_t phys);
uint64_t pmm_free_frames(void);

/* kernel heap */
void  heap_init(void);
void *kmalloc(size_t n);
void  kfree(void *p);

/* address spaces */
struct aspace { uint64_t *pml4, *pdpt, *pd; };
struct aspace *aspace_create(void);
void  aspace_destroy(struct aspace *a);
void  aspace_activate(struct aspace *a);   /* NULL = kernel space */
int   aspace_map(struct aspace *a, uint64_t va, uint64_t pa, uint64_t flags);
int   user_range_ok(struct aspace *a, uint64_t va, uint64_t len, int write);
void  vmm_init(void);
#endif
