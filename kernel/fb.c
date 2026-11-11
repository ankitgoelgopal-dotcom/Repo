#include "kernel.h"
#include "fb.h"
#include "mm.h"

struct fb fb;
static struct { uint64_t addr; uint32_t pitch, w, h; uint8_t bpp, type, rpos, gpos, bpos; int have; } boot;

void fb_boot_info(uint64_t addr, uint32_t pitch, uint32_t w, uint32_t h, uint8_t bpp, uint8_t type,
                  uint8_t rpos, uint8_t gpos, uint8_t bpos) {
    boot.addr = addr; boot.pitch = pitch; boot.w = w; boot.h = h; boot.bpp = bpp; boot.type = type;
    boot.rpos = rpos; boot.gpos = gpos; boot.bpos = bpos; boot.have = 1;
}

void fb_fill32(uint32_t *dst, uint32_t value, uint64_t count) {
    __asm__ volatile("rep stosl" : "+D"(dst), "+c"(count) : "a"(value) : "memory");
}
void fb_copy32(uint32_t *dst, const uint32_t *src, uint64_t count) {
    uint64_t pairs = count >> 1;
    __asm__ volatile("rep movsq" : "+D"(dst), "+S"(src), "+c"(pairs) :: "memory");
    if (count & 1) *dst = *src;
}

int fb_init(void) {
    if (!boot.have) { kprintf("[gfx ] no framebuffer from the bootloader (text mode only)\n"); return -1; }
    if (boot.type != 1 || boot.bpp != 32 || boot.rpos != 16 || boot.gpos != 8 || boot.bpos != 0) {
        kprintf("[gfx ] unsupported framebuffer (type %u, %u bpp): need 32-bit xRGB\n", boot.type, boot.bpp); return -1; }
    if (boot.pitch < boot.w * 4 || boot.w < 640 || boot.h < 480 || boot.w > 4096 || boot.h > 4096) {
        kprintf("[gfx ] unsupported framebuffer size %ux%u\n", boot.w, boot.h); return -1; }
    uint64_t bytes = (uint64_t)boot.pitch * boot.h;
    if (vmm_map_mmio(boot.addr, bytes)) { kprintf("[gfx ] framebuffer at %p is outside the mappable device window\n", (void *)boot.addr); return -1; }
    uint64_t frames = ((uint64_t)boot.w * boot.h * 4 + PAGE - 1) / PAGE;
    uint64_t back = pmm_alloc_contig(frames);
    if (!back) { kprintf("[gfx ] out of memory for the back buffer\n"); return -1; }
    fb.w = boot.w; fb.h = boot.h; fb.pitch = boot.pitch;
    fb.front = (volatile uint32_t *)boot.addr; fb.back = (uint32_t *)back; fb.ok = 1;
    kprintf("[gfx ] framebuffer %ux%u xRGB at %p, back buffer %lu KiB\n", fb.w, fb.h, (void *)boot.addr, (unsigned long)(frames * 4));
    return 0;
}

void fb_present(int x0, int y0, int x1, int y1) {
    if (!fb.ok) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
    if (x1 > (int)fb.w) x1 = fb.w; if (y1 > (int)fb.h) y1 = fb.h;
    if (x1 <= x0 || y1 <= y0) return;
    for (int y = y0; y < y1; y++)
        fb_copy32((uint32_t *)((uint8_t *)fb.front + (uint64_t)y * fb.pitch) + x0, fb.back + (uint64_t)y * fb.w + x0, x1 - x0);
}
