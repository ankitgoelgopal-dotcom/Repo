#ifndef QAR_PS2_H
#define QAR_PS2_H
#include <stdint.h>
/* Raw input events produced in the IRQ handlers and consumed by the GUI thread. */
enum { RAW_KEY = 1, RAW_MOUSE = 2 };
struct raw_ev {
    uint8_t kind;
    uint8_t scancode, ext, release;      /* RAW_KEY: set-1 scancode, E0-prefixed?, key released? */
    int16_t dx, dy; int8_t dz; uint8_t buttons;   /* RAW_MOUSE: relative motion (dy > 0 = up), wheel, button bits 0=L 1=R 2=M */
};
int  ps2_init(void);          /* controller, keyboard and mouse; returns bit0 = keyboard ok, bit1 = mouse ok */
void ps2_irq(void);           /* called from IRQ 1 and IRQ 12 */
int  ps2_pop(struct raw_ev *out);   /* 1 if an event was returned */
#endif
