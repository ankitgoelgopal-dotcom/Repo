#ifndef QAR_MULTIBOOT2_H
#define QAR_MULTIBOOT2_H
#include <stdint.h>
#define MB2_BOOT_MAGIC 0x36D76289u
#define MB2_TAG_END 0
#define MB2_TAG_CMDLINE 1
#define MB2_TAG_BOOTLOADER 2
#define MB2_TAG_MMAP 6
#define MB2_TAG_FRAMEBUFFER 8
#define MB2_MMAP_AVAILABLE 1

struct mb2_info { uint32_t total_size, reserved; } __attribute__((packed));
struct mb2_tag  { uint32_t type, size; } __attribute__((packed));
struct mb2_mmap_tag {
    uint32_t type, size, entry_size, entry_version;
} __attribute__((packed));
struct mb2_mmap_entry { uint64_t addr, len; uint32_t type, zero; } __attribute__((packed));
struct mb2_fb_tag {
    uint32_t type, size;
    uint64_t addr;
    uint32_t pitch, width, height;
    uint8_t  bpp, fb_type, reserved;
} __attribute__((packed));
#endif
