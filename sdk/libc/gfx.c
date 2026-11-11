#include <string.h>
#include "qar_gfx.h"
#include "qar_font8x8.h"

void qar_fill(qar_canvas *c, int x, int y, int w, int h, uint32_t color) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > c->w ? c->w : x + w, y1 = y + h > c->h ? c->h : y + h;
    for (int yy = y0; yy < y1; yy++) { uint32_t *p = c->px + (long)yy * c->w; for (int xx = x0; xx < x1; xx++) p[xx] = color; }
}
void qar_rect(qar_canvas *c, int x, int y, int w, int h, uint32_t color) {
    qar_fill(c, x, y, w, 1, color); qar_fill(c, x, y + h - 1, w, 1, color);
    qar_fill(c, x, y, 1, h, color); qar_fill(c, x + w - 1, y, 1, h, color);
}
int qar_text_width(const char *s, int scale) { return (int)strlen(s) * 8 * scale; }
void qar_text(qar_canvas *c, int x, int y, const char *s, uint32_t color, int scale) {
    for (; *s; s++, x += 8 * scale) {
        const unsigned char *g = qar_font8x8[(unsigned char)*s & 0x7F];
        for (int row = 0; row < 8; row++) for (int col = 0; col < 8; col++)
            if (g[row] >> col & 1) qar_fill(c, x + col * scale, y + row * scale, scale, scale, color);
    }
}
void qar_blit(qar_canvas *d, int dx, int dy, const uint32_t *src, int sw, int sh) {
    for (int y = 0; y < sh; y++) {
        int yy = dy + y; if (yy < 0 || yy >= d->h) continue;
        int x0 = dx < 0 ? -dx : 0, x1 = dx + sw > d->w ? d->w - dx : sw;
        if (x1 > x0) memcpy(d->px + (long)yy * d->w + dx + x0, src + (long)y * sw + x0, (size_t)(x1 - x0) * 4);
    }
}
