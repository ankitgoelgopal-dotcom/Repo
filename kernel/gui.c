/* QAR-OS window system: software compositor (damage-tracked), window manager, input routing and the log window.
 * Windows own a kernel-side pixel surface; apps copy pixels in with WIN_PRESENT, so a crashing app can never
 * corrupt the screen or other windows. Runs as a kernel thread; IRQ handlers only queue raw input events. */
#include "kernel.h"
#include "cpu.h"
#include "mm.h"
#include "mem.h"
#include "fb.h"
#include "gui.h"
#include "ps2.h"
#include "sched.h"
#include "os_abi.h"
#include "qar_font8x8.h"

#define MAX_WINS 16
#define TB 22                       /* title bar height */
#define BD 2                        /* border width */
#define EVQ 64
#define CON_COLS 80
#define CON_ROWS 28
#define CON_W (CON_COLS * 8)
#define CON_H (CON_ROWS * 16)
#define C_BORDER   0x00101010u
#define C_TITLE_ON 0x00D08000u
#define C_TITLE_OFF 0x00555555u
#define C_CLOSE    0x00C0392Bu
#define C_CON_BG   0x00101418u
#define C_CON_FG   0x00D0D0D0u

int gui_active;

struct win {
    int used, dead, kernel, id, owner;
    int x, y, w, h;                                  /* content rectangle on screen */
    char title[48];
    uint32_t *pix; size_t frames;
    struct qar_event q[EVQ]; int qh, qt;
};
struct rect { int x0, y0, x1, y1; };

static struct win wins[MAX_WINS];
static int zlist[MAX_WINS], nz;                      /* back to front, slot indices */
static int focus = -1, next_id = 1;
static struct win *con;

static inline uint64_t irq_save(void) { uint64_t f; __asm__ volatile("pushfq; pop %0; cli" : "=r"(f) :: "memory"); return f; }
static inline void irq_restore(uint64_t f) { if (f & 0x200) __asm__ volatile("sti" ::: "memory"); }

/* ---------------------------------------------------------------- damage + drawing primitives */
static struct rect dirty; static volatile int have_dirty;

static int clip_to(struct rect *r, const struct rect *c) {
    if (r->x0 < c->x0) r->x0 = c->x0; if (r->y0 < c->y0) r->y0 = c->y0;
    if (r->x1 > c->x1) r->x1 = c->x1; if (r->y1 > c->y1) r->y1 = c->y1;
    return r->x1 > r->x0 && r->y1 > r->y0;
}
static void damage(int x0, int y0, int x1, int y1) {
    struct rect scr = { 0, 0, (int)fb.w, (int)fb.h }, r = { x0, y0, x1, y1 };
    if (!clip_to(&r, &scr)) return;
    uint64_t f = irq_save();
    if (!have_dirty) { dirty = r; have_dirty = 1; }
    else {
        if (r.x0 < dirty.x0) dirty.x0 = r.x0; if (r.y0 < dirty.y0) dirty.y0 = r.y0;
        if (r.x1 > dirty.x1) dirty.x1 = r.x1; if (r.y1 > dirty.y1) dirty.y1 = r.y1;
    }
    irq_restore(f);
}
static void fillr(const struct rect *clip, int x, int y, int w, int h, uint32_t col) {
    struct rect r = { x, y, x + w, y + h };
    if (!clip_to(&r, clip)) return;
    for (int yy = r.y0; yy < r.y1; yy++) fb_fill32(fb.back + (uint64_t)yy * fb.w + r.x0, col, r.x1 - r.x0);
}
static void drawtext(const struct rect *clip, int x, int y, const char *s, uint32_t col, int maxc) {
    for (; *s && maxc > 0; s++, x += 8, maxc--) {
        const unsigned char *g = qar_font8x8[(unsigned char)*s & 0x7F];
        for (int row = 0; row < 8; row++) {
            int yy = y + row; if (yy < clip->y0 || yy >= clip->y1) continue;
            for (int cx = 0; cx < 8; cx++) {
                int xx = x + cx;
                if ((g[row] >> cx & 1) && xx >= clip->x0 && xx < clip->x1) fb.back[(uint64_t)yy * fb.w + xx] = col;
            }
        }
    }
}
static const char *const cursor_art[19] = {
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ", "X.....X     ",
    "X......X    ", "X.......X   ", "X........X  ", "X.....XXXXX ", "X..X..X     ", "X.X X..X    ", "XX  X..X    ",
    "X    X..X   ", "     X..X   ", "      X..X  ", "      X..X  ", "       XX   " };
static int mx, my;
static void draw_cursor(const struct rect *clip) {
    for (int r = 0; r < 19; r++) for (int c = 0; c < 12; c++) {
        char ch = cursor_art[r][c]; int x = mx + c, y = my + r;
        if (ch == ' ' || x < clip->x0 || x >= clip->x1 || y < clip->y0 || y >= clip->y1) continue;
        fb.back[(uint64_t)y * fb.w + x] = ch == 'X' ? 0x00000000u : 0x00FFFFFFu;
    }
}
static uint32_t bg_row(int y) {                                   /* vertical gradient 0x16324F -> 0x2E6F95 */
    unsigned t = fb.h > 1 ? (unsigned)y * 256 / (fb.h - 1) : 0;
    unsigned r = 0x16 + (0x2E - 0x16) * t / 256, g = 0x32 + (0x6F - 0x32) * t / 256, b = 0x4F + (0x95 - 0x4F) * t / 256;
    return r << 16 | g << 8 | b;
}
static void outer(const struct win *w, struct rect *r) { r->x0 = w->x - BD; r->y0 = w->y - TB - BD; r->x1 = w->x + w->w + BD; r->y1 = w->y + w->h + BD; }

static void draw_window(const struct rect *clip, struct win *w, int focused) {
    struct rect o; outer(w, &o);
    if (!clip_to(&o, clip)) return;
    fillr(clip, w->x - BD, w->y - TB - BD, w->w + 2 * BD, w->h + TB + 2 * BD, C_BORDER);
    fillr(clip, w->x, w->y - TB, w->w, TB, focused ? C_TITLE_ON : C_TITLE_OFF);
    drawtext(clip, w->x + 8, w->y - TB + 7, w->title, focused ? 0x00FFFFFFu : 0x00C8C8C8u, (w->w - 40) / 8);
    if (!w->kernel) {
        int bx = w->x + w->w - 19, by = w->y - TB + 3;
        fillr(clip, bx, by, 16, 16, C_CLOSE);
        for (int i = 3; i < 13; i++) { fillr(clip, bx + i, by + i, 2, 1, 0x00FFFFFFu); fillr(clip, bx + 15 - i, by + i, 2, 1, 0x00FFFFFFu); }
    }
    struct rect c = { w->x, w->y, w->x + w->w, w->y + w->h };
    if (!clip_to(&c, clip)) return;
    for (int yy = c.y0; yy < c.y1; yy++)
        fb_copy32(fb.back + (uint64_t)yy * fb.w + c.x0, w->pix + (uint64_t)(yy - w->y) * w->w + (c.x0 - w->x), c.x1 - c.x0);
}
static void compose(struct rect c) {
    int zl[MAX_WINS], n, foc;
    uint64_t f = irq_save(); n = nz; for (int i = 0; i < n; i++) zl[i] = zlist[i]; foc = focus; irq_restore(f);
    for (int y = c.y0; y < c.y1; y++) fb_fill32(fb.back + (uint64_t)y * fb.w + c.x0, bg_row(y), c.x1 - c.x0);
    for (int i = 0; i < n; i++) { struct win *w = &wins[zl[i]]; if (w->used && !w->dead) draw_window(&c, w, zl[i] == foc); }
    draw_cursor(&c);
}

/* ---------------------------------------------------------------- windows */
static struct win *find_win(int id, int pid) {
    for (int i = 0; i < MAX_WINS; i++) if (wins[i].used && !wins[i].dead && wins[i].id == id && (pid < 0 || wins[i].owner == pid)) return &wins[i];
    return 0;
}
static void push_ev(struct win *w, const struct qar_event *e) {
    uint64_t f = irq_save();
    int nx = (w->qt + 1) % EVQ;
    if (nx != w->qh && !(e->type == QAR_EV_MOUSE_MOVE && ((w->qt + EVQ - w->qh) % EVQ) > EVQ / 2)) { w->q[w->qt] = *e; w->qt = nx; }
    irq_restore(f);
}
static void send(struct win *w, uint32_t type, int x, int y, uint32_t a, uint32_t b) {
    if (w->kernel) return;
    struct qar_event e = { type, (uint32_t)w->id, x, y, a, b }; push_ev(w, &e);
}
static void zremove(int slot) { int j = 0; for (int i = 0; i < nz; i++) if (zlist[i] != slot) zlist[j++] = zlist[i]; nz = j; }
static void set_focus(int slot) {
    if (slot == focus) return;
    if (focus >= 0 && wins[focus].used) { send(&wins[focus], QAR_EV_FOCUS, 0, 0, 0, 0); struct rect o; outer(&wins[focus], &o); damage(o.x0, o.y0, o.x1, o.y0 + TB + BD * 2); }
    focus = slot;
    if (slot >= 0) { send(&wins[slot], QAR_EV_FOCUS, 0, 0, 1, 0); struct rect o; outer(&wins[slot], &o); damage(o.x0, o.y0, o.x1, o.y0 + TB + BD * 2); }
}
static void raise_win(int slot) {
    uint64_t f = irq_save(); zremove(slot); zlist[nz++] = slot; irq_restore(f);
    struct rect o; outer(&wins[slot], &o); damage(o.x0, o.y0, o.x1, o.y1);
}
static struct win *alloc_win(int x, int y, int w, int h, const char *title, int owner, int kernel) {
    int slot = -1; for (int i = 0; i < MAX_WINS; i++) if (!wins[i].used) { slot = i; break; }
    if (slot < 0) return 0;
    size_t frames = ((size_t)w * h * 4 + PAGE - 1) / PAGE;
    uint64_t mem = pmm_alloc_contig(frames); if (!mem) return 0;
    struct win *k = &wins[slot]; memset(k, 0, sizeof *k);
    k->id = next_id++; k->owner = owner; k->kernel = kernel; k->x = x; k->y = y; k->w = w; k->h = h;
    k->pix = (uint32_t *)mem; k->frames = frames;
    fb_fill32(k->pix, 0x00202020u, (uint64_t)w * h);
    for (int i = 0; i < 47 && title[i]; i++) k->title[i] = title[i];
    uint64_t f = irq_save(); k->used = 1; zlist[nz++] = slot; irq_restore(f);
    set_focus(slot);
    struct rect o; outer(k, &o); damage(o.x0, o.y0, o.x1, o.y1);
    return k;
}
static void kill_win(struct win *w) {
    int slot = (int)(w - wins); struct rect o; outer(w, &o);
    uint64_t f = irq_save(); w->dead = 1; zremove(slot);
    if (focus == slot) focus = nz ? zlist[nz - 1] : -1;
    irq_restore(f);
    if (focus >= 0) { struct rect fo; outer(&wins[focus], &fo); damage(fo.x0, fo.y0, fo.x1, fo.y0 + TB + BD * 2); }
    damage(o.x0, o.y0, o.x1, o.y1);
}
static void reap_dead(void) {                                    /* memory is freed here, never while a frame may be reading it */
    for (int i = 0; i < MAX_WINS; i++) if (wins[i].used && wins[i].dead) {
        for (size_t k = 0; k < wins[i].frames; k++) pmm_free((uint64_t)wins[i].pix + k * PAGE);
        wins[i].used = 0;
    }
}
void gui_close_owner(int pid) {
    if (!gui_active) return;
    for (int i = 0; i < MAX_WINS; i++) if (wins[i].used && !wins[i].dead && wins[i].owner == pid && !wins[i].kernel) kill_win(&wins[i]);
}

/* ---------------------------------------------------------------- log window (kernel console) */
static char ccell[CON_ROWS][CON_COLS]; static int ccx, ccy; static volatile uint32_t crow_dirty = 0xFFFFFFFFu;
void gui_console_putc(char c) {
    static int init;
    if (!init) { memset(ccell, ' ', sizeof ccell); init = 1; }
    crow_dirty |= 1u << ccy;
    if (c == '\n') { ccx = 0; ccy++; }
    else if (c == '\r') ccx = 0;
    else if (c == '\b') { if (ccx) ccx--; }
    else if (c == '\t') { ccx = (ccx + 8) & ~7; }
    else if ((unsigned char)c >= 32) { ccell[ccy][ccx++] = c; }
    if (ccx >= CON_COLS) { ccx = 0; ccy++; }
    if (ccy >= CON_ROWS) {
        memmove(ccell[0], ccell[1], (CON_ROWS - 1) * CON_COLS); memset(ccell[CON_ROWS - 1], ' ', CON_COLS);
        ccy = CON_ROWS - 1; crow_dirty = 0xFFFFFFFFu;
    }
    crow_dirty |= 1u << ccy;
}
static void console_render(void) {
    if (!con) return;
    uint32_t d = crow_dirty; if (!d) return; crow_dirty = 0;
    for (int r = 0; r < CON_ROWS; r++) {
        if (!(d >> r & 1)) continue;
        for (int c = 0; c < CON_COLS; c++) {
            const unsigned char *g = qar_font8x8[(unsigned char)ccell[r][c] & 0x7F];
            int cur = (r == ccy && c == ccx);
            uint32_t fg = cur ? C_CON_BG : C_CON_FG, bg = cur ? C_CON_FG : C_CON_BG;
            for (int row = 0; row < 16; row++) {
                uint32_t *p = con->pix + (uint64_t)(r * 16 + row) * CON_W + c * 8; unsigned bits = g[row >> 1];
                for (int cx = 0; cx < 8; cx++) p[cx] = (bits >> cx & 1) ? fg : bg;
            }
        }
        damage(con->x, con->y + r * 16, con->x + CON_W, con->y + r * 16 + 16);
    }
}

/* ---------------------------------------------------------------- input */
static int mbtn, drag = -1, ddx, ddy, capture = -1;
static struct { int shift, ctrl, alt, caps; } mods;

static int win_at(int px, int py, int *part) {
    int zl[MAX_WINS], n; uint64_t f = irq_save(); n = nz; for (int i = 0; i < n; i++) zl[i] = zlist[i]; irq_restore(f);
    for (int i = n - 1; i >= 0; i--) {
        struct win *w = &wins[zl[i]]; if (!w->used || w->dead) continue;
        struct rect o; outer(w, &o);
        if (px < o.x0 || px >= o.x1 || py < o.y0 || py >= o.y1) continue;
        if (px >= w->x && px < w->x + w->w && py >= w->y && py < w->y + w->h) *part = 1;
        else if (px >= w->x && px < w->x + w->w && py >= w->y - TB && py < w->y) {
            *part = (!w->kernel && px >= w->x + w->w - 19 && px < w->x + w->w - 3 && py >= w->y - TB + 3 && py < w->y - TB + 19) ? 3 : 2;
        } else *part = 4;
        return zl[i];
    }
    return -1;
}
static void cursor_damage(void) { damage(mx, my, mx + 12, my + 19); }

static void on_mouse(const struct raw_ev *e) {
    cursor_damage();
    mx += e->dx; my -= e->dy;
    if (mx < 0) mx = 0; if (my < 0) my = 0; if (mx >= (int)fb.w) mx = fb.w - 1; if (my >= (int)fb.h) my = fb.h - 1;
    cursor_damage();
    int part = 0, slot = win_at(mx, my, &part);
    int pressed = e->buttons & ~mbtn, released = mbtn & ~e->buttons; mbtn = e->buttons;

    if (drag >= 0 && wins[drag].used && !wins[drag].dead && (mbtn & 1)) {          /* dragging a window by its title bar */
        struct win *w = &wins[drag]; struct rect o; outer(w, &o); damage(o.x0, o.y0, o.x1, o.y1);
        int nx = mx - ddx, ny = my - ddy;
        if (ny < TB + BD) ny = TB + BD; if (ny > (int)fb.h - 8) ny = fb.h - 8;
        if (nx < 40 - w->w) nx = 40 - w->w; if (nx > (int)fb.w - 40) nx = fb.w - 40;
        w->x = nx; w->y = ny; outer(w, &o); damage(o.x0, o.y0, o.x1, o.y1);
        return;
    }
    if (released & 1) drag = -1;
    for (int b = 1; b <= 4; b <<= 1) {
        if (pressed & b) {
            if (slot >= 0) {
                if (b == 1 && part >= 2) {
                    raise_win(slot); set_focus(slot);
                    if (part == 3) send(&wins[slot], QAR_EV_CLOSE, 0, 0, 0, 0);
                    else if (part == 2) { drag = slot; ddx = mx - wins[slot].x; ddy = my - wins[slot].y; }
                } else if (part == 1) {
                    if (b == 1) { raise_win(slot); set_focus(slot); }
                    capture = slot; send(&wins[slot], QAR_EV_MOUSE_DOWN, mx - wins[slot].x, my - wins[slot].y, b, 0);
                }
            }
        }
        if ((released & b) && capture >= 0 && wins[capture].used && !wins[capture].dead)
            send(&wins[capture], QAR_EV_MOUSE_UP, mx - wins[capture].x, my - wins[capture].y, b, 0);
    }
    if (!mbtn) capture = -1;
    if (e->dx || e->dy) {
        int t = capture >= 0 ? capture : (part == 1 ? slot : -1);
        if (t >= 0 && wins[t].used && !wins[t].dead) send(&wins[t], QAR_EV_MOUSE_MOVE, mx - wins[t].x, my - wins[t].y, mbtn, 0);
    }
    if (e->dz && slot >= 0 && part == 1) send(&wins[slot], QAR_EV_WHEEL, mx - wins[slot].x, my - wins[slot].y, (uint32_t)(int32_t)(-e->dz), 0);
}

static const char sc_lo[0x3A] = { 0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,'q','w','e','r','t','y','u','i','o','p','[',']',13,0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ' };
static const char sc_hi[0x3A] = { 0,27,'!','@','#','$','%','^','&','*','(',')','_','+',8,9,'Q','W','E','R','T','Y','U','I','O','P','{','}',13,0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ' };

static void on_key(const struct raw_ev *e) {
    int sc = e->scancode, rel = e->release, code = 0, ch = 0;
    if (e->ext && (sc == 0x2A || sc == 0x36)) return;                          /* fake shifts around PrintScreen/Insert */
    if (sc == 0x2A || sc == 0x36) { mods.shift = !rel; code = QAR_KEY_SHIFT; }
    else if (sc == 0x1D) { mods.ctrl = !rel; code = QAR_KEY_CTRL; }
    else if (sc == 0x38) { mods.alt = !rel; code = QAR_KEY_ALT; }
    else if (sc == 0x3A && !e->ext) { if (!rel) mods.caps ^= 1; code = QAR_KEY_CAPS; }
    else if (e->ext) {
        switch (sc) {
        case 0x48: code = QAR_KEY_UP; break;    case 0x50: code = QAR_KEY_DOWN; break;
        case 0x4B: code = QAR_KEY_LEFT; break;  case 0x4D: code = QAR_KEY_RIGHT; break;
        case 0x47: code = QAR_KEY_HOME; break;  case 0x4F: code = QAR_KEY_END; break;
        case 0x49: code = QAR_KEY_PGUP; break;  case 0x51: code = QAR_KEY_PGDN; break;
        case 0x52: code = QAR_KEY_INSERT; break; case 0x53: code = QAR_KEY_DELETE; break;
        case 0x1C: code = QAR_KEY_ENTER; ch = '\r'; break;
        case 0x35: code = '/'; ch = '/'; break;
        }
    } else if (sc >= 0x3B && sc <= 0x44) code = QAR_KEY_F1 + (sc - 0x3B);
    else if (sc == 0x57) code = QAR_KEY_F1 + 10; else if (sc == 0x58) code = QAR_KEY_F1 + 11;
    else if (sc < 0x3A && sc_lo[sc]) {
        code = (unsigned char)sc_lo[sc]; int up = mods.shift;
        if (code >= 'a' && code <= 'z' && mods.caps) up = !up;
        ch = (unsigned char)(up ? sc_hi[sc] : sc_lo[sc]);
    }
    if (!code) return;
    if (!rel && sc == 0x0F && !e->ext && mods.alt) {                             /* Alt+Tab: bring the bottom-most window to the front */
        if (nz > 1) { int s = zlist[0]; raise_win(s); set_focus(s); }
        return;
    }
    if (focus < 0 || !wins[focus].used || wins[focus].dead) return;
    int m = (mods.shift ? QAR_MOD_SHIFT : 0) | (mods.ctrl ? QAR_MOD_CTRL : 0) | (mods.alt ? QAR_MOD_ALT : 0) | (mods.caps ? QAR_MOD_CAPS : 0);
    if (mods.ctrl || mods.alt || rel) ch = 0;
    send(&wins[focus], rel ? QAR_EV_KEY_UP : QAR_EV_KEY_DOWN, m, 0, (uint32_t)code, (uint32_t)ch);
}

/* ---------------------------------------------------------------- thread + init */
void gui_thread(void) {
    for (;;) {
        struct raw_ev e;
        while (ps2_pop(&e)) { if (e.kind == RAW_MOUSE) on_mouse(&e); else on_key(&e); }
        reap_dead(); console_render();
        struct rect r; int have = 0;
        uint64_t f = irq_save(); if (have_dirty) { r = dirty; have_dirty = 0; have = 1; } irq_restore(f);
        if (have) { compose(r); fb_present(r.x0, r.y0, r.x1, r.y1); }
        task_sleep_ticks(1); sti();
    }
}

int gui_init(void) {
    if (fb_init()) return -1;
    ps2_init();
    mx = fb.w / 2; my = fb.h / 2;
    con = alloc_win(24, 48, CON_W, CON_H, "QAR-OS log", 0, 1);
    if (!con) { kprintf("[gui ] cannot create the log window\n"); fb.ok = 0; return -1; }
    gui_active = 1; crow_dirty = 0xFFFFFFFFu;
    damage(0, 0, fb.w, fb.h);
    kprintf("[gui ] window system ready (compositor thread, PS/2 input)\n");
    return 0;
}

/* ---------------------------------------------------------------- system calls */
static int user_str(uint64_t p, char *dst, int cap) {
    struct aspace *as = current_aspace();
    for (int i = 0; i < cap; i++) { if (!user_range_ok(as, p + i, 1, 0)) return -1; dst[i] = ((const char *)p)[i]; if (!dst[i]) return 0; }
    dst[cap - 1] = 0; return 0;
}
long gui_syscall(uint64_t n, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    if (!(current_caps() & CAP_GUI)) return -QAR_EPERM;
    if (!gui_active) return -QAR_ENODEV;
    struct aspace *as = current_aspace(); int pid = current_pid();
    switch (n) {
    case SYS_SCREEN_INFO: {
        if (!user_range_ok(as, a, sizeof(struct qar_screen), 1)) return -QAR_EFAULT;
        struct qar_screen *s = (struct qar_screen *)a; s->w = fb.w; s->h = fb.h; s->bpp = 32; s->pitch = fb.w * 4; return 0; }
    case SYS_WIN_CREATE: {
        if (!user_range_ok(as, a, sizeof(struct qar_win_create), 0)) return -QAR_EFAULT;
        struct qar_win_create rq = *(const struct qar_win_create *)a;
        if (rq.w < 64 || rq.h < 32 || rq.w > (int)fb.w || rq.h > (int)fb.h) return -QAR_EINVAL;
        rq.title[47] = 0;
        if (rq.x == -1) { int k = next_id % 8; rq.x = 80 + 34 * k; rq.y = 90 + 28 * k; }
        if (rq.y < TB + BD) rq.y = TB + BD;
        struct win *w = alloc_win(rq.x, rq.y, rq.w, rq.h, rq.title, pid, 0);
        if (!w) return -QAR_ENOMEM;
        kprintf("[gui ] window %d \"%s\" pid %d content (%d,%d) %dx%d\n", w->id, w->title, pid, w->x, w->y, w->w, w->h);
        return w->id; }
    case SYS_WIN_DESTROY: { struct win *w = find_win((int)a, pid); if (!w) return -QAR_EINVAL; kill_win(w); return 0; }
    case SYS_WIN_TITLE: {
        struct win *w = find_win((int)a, pid); if (!w) return -QAR_EINVAL;
        char t[48]; if (user_str(b, t, 48)) return -QAR_EFAULT;
        memcpy(w->title, t, 48); struct rect o; outer(w, &o); damage(o.x0, o.y0, o.x1, o.y0 + TB + 2 * BD); return 0; }
    case SYS_WIN_PRESENT: {
        struct win *w = find_win((int)a, pid); if (!w) return -QAR_EINVAL;
        struct qar_rect r = { 0, 0, w->w, w->h };
        if (d) { if (!user_range_ok(as, d, sizeof r, 0)) return -QAR_EFAULT; r = *(const struct qar_rect *)d; }
        if (r.x < 0) { r.w += r.x; r.x = 0; } if (r.y < 0) { r.h += r.y; r.y = 0; }
        if (r.x + r.w > w->w) r.w = w->w - r.x; if (r.y + r.h > w->h) r.h = w->h - r.y;
        if (r.w <= 0 || r.h <= 0) return 0;
        if (c < (uint64_t)w->w * 4 || (c & 3) || (b & 3)) return -QAR_EINVAL;
        for (int row = 0; row < r.h; row++) {
            uint64_t src = b + (uint64_t)(r.y + row) * c + (uint64_t)r.x * 4;
            if (!user_range_ok(as, src, (uint64_t)r.w * 4, 0)) return -QAR_EFAULT;
            fb_copy32(w->pix + (uint64_t)(r.y + row) * w->w + r.x, (const uint32_t *)src, r.w);
        }
        damage(w->x + r.x, w->y + r.y, w->x + r.x + r.w, w->y + r.y + r.h); return 0; }
    case SYS_WIN_POLL: {
        if (!user_range_ok(as, a, sizeof(struct qar_event), 1)) return -QAR_EFAULT;
        int64_t tmo = (int64_t)b; uint64_t deadline = tmo > 0 ? ticks + (uint64_t)tmo * TICK_HZ / 1000 + 1 : 0;
        for (;;) {
            int mine = 0;
            for (int i = 0; i < MAX_WINS; i++) {
                struct win *w = &wins[i]; if (!w->used || w->dead || w->owner != pid) continue;
                mine = 1;
                if (w->qh != w->qt) { *(struct qar_event *)a = w->q[w->qh]; w->qh = (w->qh + 1) % EVQ; return 1; }
            }
            if (!mine) return -QAR_ENODEV;
            if (tmo == 0 || (tmo > 0 && ticks >= deadline)) return 0;
            task_sleep_ticks(1);
        } }
    }
    return -QAR_ENOSYS;
}
