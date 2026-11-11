/* Two overlapping windows that react to keyboard, mouse and close events. CI drives it with injected input and
 * checks screenshots; every event is also printed in a machine-readable form ("GUIDEMO EV ..."). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "os_api.h"
#include "qar_gfx.h"

#define W 300
#define H 180
struct demo { int id, x, y; const char *name; uint32_t bg; uint32_t *px; int clicks; char typed[24]; int focus, alive; };

static void redraw(struct demo *d) {
    qar_canvas c = { d->px, W, H };
    qar_fill(&c, 0, 0, W, H, d->bg);
    qar_text(&c, 12, 12, d->name, QAR_RGB(255, 255, 255), 2);
    char line[48]; snprintf(line, sizeof line, "clicks: %d", d->clicks);
    qar_text(&c, 12, 44, line, QAR_RGB(255, 255, 255), 1);
    snprintf(line, sizeof line, "typed: %s", d->typed);
    qar_text(&c, 12, 60, line, QAR_RGB(255, 255, 255), 1);
    qar_fill(&c, 20, 110, 110, 36, d->focus ? QAR_RGB(255, 255, 255) : QAR_RGB(200, 200, 200));
    qar_text(&c, 34, 124, "Click me", QAR_RGB(0, 0, 0), 1);
    os_win_present(d->id, d->px, W * 4, NULL);
}

int main(void) {
    struct demo d[2] = {
        { 0, 200, 120, "Alpha", QAR_RGB(0x2E, 0x7D, 0x32), 0, 0, "", 0, 1 },
        { 0, 320, 200, "Beta",  QAR_RGB(0x15, 0x65, 0xC0), 0, 0, "", 1, 1 },
    };
    struct qar_screen scr;
    if (os_screen_info(&scr) < 0) { puts("GUIDEMO no screen"); return 1; }
    printf("GUIDEMO screen %ux%u\n", scr.w, scr.h);
    for (int i = 0; i < 2; i++) {
        struct qar_win_create wc = { d[i].x, d[i].y, W, H, 0, "" };
        strcpy(wc.title, d[i].name);
        d[i].id = os_win_create(&wc);
        if (d[i].id < 0) { printf("GUIDEMO create failed %d\n", d[i].id); return 1; }
        d[i].px = malloc(W * H * 4);
        redraw(&d[i]);
    }
    puts("GUIDEMO READY");
    uint64_t start = os_uptime_ms(); int alive = 2;
    while (alive > 0) {
        struct qar_event e; int r = os_win_poll(&e, 500);
        if (os_uptime_ms() - start > 150000) { puts("GUIDEMO TIMEOUT"); return 1; }
        if (r <= 0) continue;
        struct demo *w = e.win == (uint32_t)d[0].id ? &d[0] : (e.win == (uint32_t)d[1].id ? &d[1] : NULL);
        if (!w || !w->alive) continue;
        switch (e.type) {
        case QAR_EV_KEY_DOWN:
            printf("GUIDEMO EV win=%s type=key_down code=%u char=%c mods=%d\n", w->name, e.a, e.b ? (char)e.b : '-', e.x);
            if (e.b >= 32 && e.b < 127 && strlen(w->typed) < 20) { size_t n = strlen(w->typed); w->typed[n] = (char)e.b; w->typed[n + 1] = 0; redraw(w); }
            break;
        case QAR_EV_KEY_UP: break;
        case QAR_EV_MOUSE_DOWN:
            printf("GUIDEMO EV win=%s type=mouse_down x=%d y=%d btn=%u\n", w->name, e.x, e.y, e.a);
            if (e.x >= 20 && e.x < 130 && e.y >= 110 && e.y < 146) { w->clicks++; redraw(w); }
            break;
        case QAR_EV_MOUSE_UP:   printf("GUIDEMO EV win=%s type=mouse_up x=%d y=%d\n", w->name, e.x, e.y); break;
        case QAR_EV_WHEEL:      printf("GUIDEMO EV win=%s type=wheel delta=%d\n", w->name, (int)e.a); break;
        case QAR_EV_FOCUS:      printf("GUIDEMO EV win=%s type=focus gained=%u\n", w->name, e.a); w->focus = (int)e.a; redraw(w); break;
        case QAR_EV_CLOSE:
            printf("GUIDEMO EV win=%s type=close\n", w->name);
            os_win_destroy(w->id); w->alive = 0; alive--; break;
        default: break;
        }
    }
    puts("GUIDEMO DONE");
    return 0;
}
