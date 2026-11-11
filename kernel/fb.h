#ifndef QAR_FB_H
#define QAR_FB_H
#include <stdint.h>
/* Linear 32-bit framebuffer (xRGB8888) handed over by GRUB through the Multiboot2 framebuffer tag. */
struct fb { int ok; uint32_t w, h, pitch; volatile uint32_t *front; uint32_t *back; };
extern struct fb fb;
void fb_boot_info(uint64_t addr, uint32_t pitch, uint32_t w, uint32_t h, uint8_t bpp, uint8_t type,
                  uint8_t rpos, uint8_t gpos, uint8_t bpos);
int  fb_init(void);                                       /* maps the framebuffer, allocates the back buffer; 0 = ok */
void fb_present(int x0, int y0, int x1, int y1);          /* copy back buffer rect [x0,x1) x [y0,y1) to the screen */
void fb_fill32(uint32_t *dst, uint32_t value, uint64_t count);   /* fast fill (rep stosl) */
void fb_copy32(uint32_t *dst, const uint32_t *src, uint64_t count);
#endif
