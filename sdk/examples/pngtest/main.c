/* libpng on QAR-OS: decodes PNGs made by an independent encoder (Python) and checks every pixel via CRC32. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>
#include <zlib.h>
#include "png_cases.h"

static int pass, fail;
#define CHECK(c, name) do { if (c) pass++; else { fail++; printf("PNG FAIL %s\n", name); } } while (0)

struct src { const unsigned char *p; unsigned long n, pos; };
static void rd(png_structp png, png_bytep out, size_t n) {
    struct src *s = png_get_io_ptr(png);
    if (s->pos + n > s->n) png_error(png, "unexpected end of data");
    memcpy(out, s->p + s->pos, n); s->pos += n;
}
static int errors_seen;
static void on_error(png_structp png, png_const_charp msg) { (void)msg; errors_seen++; longjmp(png_jmpbuf(png), 1); }
static void on_warn(png_structp png, png_const_charp msg) { (void)png; (void)msg; }

/* decodes to RGBA8; returns malloc'd pixels or NULL on error (libpng error -> longjmp) */
static unsigned char *decode(const unsigned char *data, unsigned long len, unsigned *w, unsigned *h) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, on_error, on_warn);
    png_infop info = png_create_info_struct(png);
    unsigned char *pix = NULL; png_bytep *rows = NULL;
    struct src s = { data, len, 0 };
    if (setjmp(png_jmpbuf(png))) { free(pix); free(rows); png_destroy_read_struct(&png, &info, NULL); return NULL; }
    png_set_read_fn(png, &s, rd);
    png_read_info(png, info);
    *w = png_get_image_width(png, info); *h = png_get_image_height(png, info);
    png_set_expand(png); png_set_strip_16(png); png_set_gray_to_rgb(png);
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER); png_set_interlace_handling(png);
    png_read_update_info(png, info);
    pix = malloc((size_t)*w * *h * 4); rows = malloc(*h * sizeof *rows);
    for (unsigned y = 0; y < *h; y++) rows[y] = pix + (size_t)y * *w * 4;
    png_read_image(png, rows); png_read_end(png, NULL);
    free(rows); png_destroy_read_struct(&png, &info, NULL);
    return pix;
}

int main(void) {
    for (int i = 0; i < PNG_NCASES; i++) {
        unsigned w = 0, h = 0; unsigned char *px = decode(png_cases[i].data, png_cases[i].len, &w, &h);
        CHECK(px != NULL, png_cases[i].name);
        if (!px) continue;
        unsigned long crc = crc32(0, px, w * h * 4);
        CHECK(w == png_cases[i].w && h == png_cases[i].h, "dimensions");
        CHECK(crc == png_cases[i].crc, png_cases[i].name);
        printf("PNG %-18s %ux%u crc=%08lx %s\n", png_cases[i].name, w, h, crc, crc == png_cases[i].crc ? "ok" : "MISMATCH");
        free(px);
    }
    /* damaged files must fail cleanly through libpng's error path, never crash */
    unsigned w, h; unsigned long n = png_cases[0].len;
    unsigned char *bad = malloc(n); memcpy(bad, png_cases[0].data, n);
    CHECK(decode(bad, n / 2, &w, &h) == NULL, "truncated file rejected");
    bad[n / 2] ^= 0xFF;                                  /* corrupt IDAT data: zlib/CRC error */
    unsigned char *r = decode(bad, n, &w, &h); CHECK(r == NULL, "corrupted file rejected"); free(r);
    memset(bad, 0, 8);
    CHECK(decode(bad, n, &w, &h) == NULL, "bad signature rejected");
    CHECK(errors_seen >= 3, "error callback ran");
    printf("PNGTEST DONE pass=%d fail=%d libpng=%s\n", pass, fail, PNG_LIBPNG_VER_STRING);
    return fail ? 1 : 0;
}
