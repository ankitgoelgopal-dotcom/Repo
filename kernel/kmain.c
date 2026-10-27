#include "kernel.h"
#include "multiboot2.h"

extern char __kernel_start[], __kernel_end[];

static uint64_t usable_ram;

static void parse_multiboot(uint64_t mbi_addr) {
    struct mb2_info *info = (struct mb2_info *)mbi_addr;
    struct mb2_tag *tag = (struct mb2_tag *)(mbi_addr + 8);
    while (tag->type != MB2_TAG_END && (uint64_t)tag < mbi_addr + info->total_size) {
        switch (tag->type) {
        case MB2_TAG_CMDLINE:
            kprintf("[boot] cmdline: %s\n", (char *)(tag + 1)); break;
        case MB2_TAG_BOOTLOADER:
            kprintf("[boot] loader : %s\n", (char *)(tag + 1)); break;
        case MB2_TAG_MMAP: {
            struct mb2_mmap_tag *m = (struct mb2_mmap_tag *)tag;
            uint8_t *p = (uint8_t *)(m + 1);
            while (p < (uint8_t *)tag + tag->size) {
                struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
                kprintf("[mem ] %016lx - %016lx %s\n", e->addr, e->addr + e->len,
                        e->type == MB2_MMAP_AVAILABLE ? "usable" : "reserved");
                if (e->type == MB2_MMAP_AVAILABLE) usable_ram += e->len;
                p += m->entry_size;
            }
            break; }
        case MB2_TAG_FRAMEBUFFER: {
            struct mb2_fb_tag *f = (struct mb2_fb_tag *)tag;
            kprintf("[gfx ] framebuffer %ux%ux%u @ %p (type %u)\n",
                    f->width, f->height, (unsigned)f->bpp, (void *)f->addr, (unsigned)f->fb_type);
            break; }
        }
        tag = (struct mb2_tag *)(((uint64_t)tag + tag->size + 7) & ~7ull);
    }
}

void kmain(uint64_t mbi_addr, uint64_t magic) {
    serial_init();
    vga_init();

    kprintf("%s %s\n", QAR_OS_NAME, QAR_OS_VERSION);
    if ((uint32_t)magic != MB2_BOOT_MAGIC) kpanic("not booted by a Multiboot2 loader");

    kprintf("[boot] long mode active, kernel %p - %p (%lu KiB)\n",
            (void *)__kernel_start, (void *)__kernel_end,
            (unsigned long)((__kernel_end - __kernel_start) / 1024));
    parse_multiboot(mbi_addr);
    kprintf("[mem ] usable RAM: %lu MiB\n", (unsigned long)(usable_ram >> 20));

    kprintf("QAR-OS: BOOT OK\n");
    for (;;) __asm__ volatile("hlt");
}
