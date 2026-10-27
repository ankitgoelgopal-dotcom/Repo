#ifndef QAR_ELF_H
#define QAR_ELF_H
#include "mm.h"
/* Loads a static ELF64 executable into aspace a. Returns 0 and sets *entry, or <0 on error. */
int elf_load(struct aspace *a, const uint8_t *img, uint64_t len, uint64_t *entry);
#endif
