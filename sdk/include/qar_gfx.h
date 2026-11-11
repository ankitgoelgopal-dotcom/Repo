/* Tiny software drawing helpers for QAR-OS apps (xRGB8888 canvases, built-in 8x8 font). */
#ifndef QAR_GFX_H
#define QAR_GFX_H
#include <stdint.h>
typedef struct { uint32_t *px; int w, h; } qar_canvas;           /* rows are w pixels apart */
#define QAR_RGB(r, g, b) ((((uint32_t)(r)) << 16) | (((uint32_t)(g)) << 8) | (uint32_t)(b))
void qar_fill(qar_canvas *c, int x, int y, int w, int h, uint32_t color);
void qar_rect(qar_canvas *c, int x, int y, int w, int h, uint32_t color);          /* 1 px outline */
void qar_text(qar_canvas *c, int x, int y, const char *s, uint32_t color, int scale);   /* ASCII, 8x8 glyphs times scale */
int  qar_text_width(const char *s, int scale);
void qar_blit(qar_canvas *dst, int dx, int dy, const uint32_t *src, int sw, int sh);    /* opaque copy, clipped */
#endif
