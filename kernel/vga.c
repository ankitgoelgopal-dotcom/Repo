#include "kernel.h"
#define VGA_W 80
#define VGA_H 25
static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;
static int cx, cy;
static const uint8_t attr = 0x0F;

static void scroll(void) {
    for (int i = 0; i < VGA_W * (VGA_H - 1); i++) vga[i] = vga[i + VGA_W];
    for (int i = 0; i < VGA_W; i++) vga[VGA_W * (VGA_H - 1) + i] = (attr << 8) | ' ';
    cy = VGA_H - 1;
}
void vga_init(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++) vga[i] = (attr << 8) | ' ';
    cx = cy = 0;
}
void vga_putc(char c) {
    if (c == '\n') { cx = 0; cy++; }
    else if (c == '\r') { cx = 0; }
    else { vga[cy * VGA_W + cx] = (attr << 8) | (uint8_t)c; if (++cx >= VGA_W) { cx = 0; cy++; } }
    if (cy >= VGA_H) scroll();
}
