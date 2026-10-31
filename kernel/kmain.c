#include "kernel.h"
#include "multiboot2.h"
#include "mm.h"
#include "cpu.h"
#include "sched.h"
#include "elf.h"
#include "qar.h"
#include "net.h"
#include "rtc.h"
#include "trusted_key.h"

extern char __kernel_start[], __kernel_end[];

static uint64_t usable_ram;
#define MAX_MODS 16
static struct { uint32_t start, end; char name[48]; } mods[MAX_MODS];
static int nmods;

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
        case MB2_TAG_MODULE: {
            if (nmods < MAX_MODS) {
                uint32_t *w = (uint32_t *)(tag + 1); mods[nmods].start = w[0]; mods[nmods].end = w[1];
                const char *n = (const char *)(w + 2); int i = 0;
                for (; i < 47 && n[i]; i++) mods[nmods].name[i] = n[i];
                mods[nmods].name[i] = 0; nmods++;
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

static int launch(const char *name, const uint8_t *blob, uint64_t len) {
    struct qar_info info;
    int r = qar_verify(blob, len, trusted_keys, TRUSTED_KEY_COUNT, &info);
    if (r) { kprintf("[qar ] REJECTED %s: %s\n", name, qar_strerror(r)); return -1; }
    kprintf("[qar ] verified %s %s v%s caps=[%s]\n", name, info.id, info.version, info.caps);
    struct aspace *as = aspace_create(); uint64_t entry;
    if (!as) { kprintf("[qar ] out of memory\n"); return -1; }
    int e = elf_load(as, info.entry_data, info.entry_size, &entry);
    if (e) { kprintf("[qar ] %s: ELF load failed (%d)\n", info.id, e); aspace_destroy(as); return -1; }
    uint32_t caps = kstrstr(info.caps, "net.connect") ? CAP_NET : 0;
    int pid = task_spawn_user(info.id, as, entry, caps);
    if (pid < 0) { aspace_destroy(as); return -1; }
    kprintf("[proc] started %s as pid %d%s\n", info.id, pid, caps ? " (net.connect granted)" : "");
    return pid;
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

    pmm_init(mbi_addr);
    for (int i = 0; i < nmods; i++) pmm_reserve(mods[i].start, mods[i].end);
    kprintf("[mem ] frame allocator ready: %lu MiB free\n", (unsigned long)(pmm_free_frames() * 4 / 1024));
    heap_init(); vmm_init();
    gdt_init(); idt_init(); fpu_init(); pic_init(); pit_init(TICK_HZ);
    sched_init();
    kprintf("[cpu ] GDT/TSS/IDT loaded, FPU/SSE on, timer at %d Hz\n", TICK_HZ);
    kprintf("QAR-OS: BOOT OK\n");
    net_init();
    entropy_init();

    for (int i = 0; i < nmods; i++)
        launch(mods[i].name, (const uint8_t *)(uint64_t)mods[i].start, mods[i].end - mods[i].start);

    sti();
    for (;;) {                                 /* pid 0: idle + reaper */
        sched_reap();
        static int done;
        if (!done && sched_live_user_tasks() == 0) { done = 1; kprintf("QAR-OS: ALL APPS FINISHED\n"); }
        __asm__ volatile("hlt");
    }
}
