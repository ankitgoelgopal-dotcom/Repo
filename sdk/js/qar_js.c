/* Portable (also compiled on the host for tests): uses only stdio/time/stdlib. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "qar_js.h"

static double now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6; }

static void print_val(JSContext *ctx, JSValueConst v, FILE *out) {
    const char *s;
    if (JS_IsObject(v) && !JS_IsFunction(ctx, v)) {                      /* objects/arrays: show JSON, like a console would */
        JSValue g = JS_GetGlobalObject(ctx), json = JS_GetPropertyStr(ctx, g, "JSON");
        JSValue fn = JS_GetPropertyStr(ctx, json, "stringify");
        JSValue r = JS_Call(ctx, fn, json, 1, &v);
        JS_FreeValue(ctx, fn); JS_FreeValue(ctx, json); JS_FreeValue(ctx, g);
        if (!JS_IsException(r) && JS_IsString(r)) { s = JS_ToCString(ctx, r); fputs(s ? s : "", out); JS_FreeCString(ctx, s); JS_FreeValue(ctx, r); return; }
        JS_FreeValue(ctx, r); JS_FreeValue(ctx, JS_GetException(ctx));
    }
    s = JS_ToCString(ctx, v);
    if (s) { fputs(s, out); JS_FreeCString(ctx, s); } else { JS_FreeValue(ctx, JS_GetException(ctx)); fputs("[unprintable]", out); }
}
static JSValue js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic) {
    (void)this_val; FILE *out = magic ? stderr : stdout;
    for (int i = 0; i < argc; i++) { if (i) fputc(' ', out); print_val(ctx, argv[i], out); }
    fputc('\n', out); return JS_UNDEFINED;
}
static JSValue js_perf_now(JSContext *ctx, JSValueConst t, int argc, JSValueConst *argv) { (void)t; (void)argc; (void)argv; return JS_NewFloat64(ctx, now_ms()); }
/* ---- timers ---- */
struct timer { int id; double due, interval; JSValue fn; int active; };
#define MAX_TIMERS 256
struct host { struct timer t[MAX_TIMERS]; int next_id; };
static struct host *H(JSContext *ctx) { return (struct host *)JS_GetContextOpaque(ctx); }

static JSValue add_timer(JSContext *ctx, int argc, JSValueConst *argv, int repeat) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) return JS_ThrowTypeError(ctx, "timer callback must be a function");
    double ms = 0; if (argc > 1) JS_ToFloat64(ctx, &ms, argv[1]); if (!(ms >= 1)) ms = repeat ? 1 : 0;
    struct host *h = H(ctx);
    for (int i = 0; i < MAX_TIMERS; i++) if (!h->t[i].active) {
        h->t[i] = (struct timer){ ++h->next_id, now_ms() + ms, repeat ? ms : 0, JS_DupValue(ctx, argv[0]), 1 };
        return JS_NewInt32(ctx, h->t[i].id);
    }
    return JS_ThrowInternalError(ctx, "too many timers");
}
static JSValue js_set_timeout(JSContext *c, JSValueConst t, int n, JSValueConst *a) { (void)t; return add_timer(c, n, a, 0); }
static JSValue js_set_interval(JSContext *c, JSValueConst t, int n, JSValueConst *a) { (void)t; return add_timer(c, n, a, 1); }
static JSValue js_clear_timer(JSContext *ctx, JSValueConst t, int argc, JSValueConst *argv) {
    (void)t; int id = 0; if (argc > 0) JS_ToInt32(ctx, &id, argv[0]);
    struct host *h = H(ctx);
    for (int i = 0; i < MAX_TIMERS; i++) if (h->t[i].active && h->t[i].id == id) { h->t[i].active = 0; JS_FreeValue(ctx, h->t[i].fn); }
    return JS_UNDEFINED;
}

/* queueMicrotask is implemented in JS on top of Promise so ordering matches browsers */
static const char bootstrap[] = "globalThis.queueMicrotask = function (f) { Promise.resolve().then(f); };";

JSContext *qar_js_new(void) {
    JSRuntime *rt = JS_NewRuntime(); if (!rt) return 0;
    JS_SetMemoryLimit(rt, 96u << 20);
    JS_SetMaxStackSize(rt, 768u << 10);
    JSContext *ctx = JS_NewContext(rt); if (!ctx) return 0;
    struct host *h = calloc(1, sizeof *h); JS_SetContextOpaque(ctx, h);
    JSValue g = JS_GetGlobalObject(ctx), con = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, g, "print", JS_NewCFunctionMagic(ctx, js_log, "print", 0, JS_CFUNC_generic_magic, 0));
    static const char *names[] = { "log", "info", "debug", "warn", "error" };
    for (int i = 0; i < 5; i++) JS_SetPropertyStr(ctx, con, names[i], JS_NewCFunctionMagic(ctx, js_log, names[i], 0, JS_CFUNC_generic_magic, i >= 3));
    JS_SetPropertyStr(ctx, g, "console", con);
    JS_SetPropertyStr(ctx, g, "setTimeout", JS_NewCFunction(ctx, js_set_timeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, g, "setInterval", JS_NewCFunction(ctx, js_set_interval, "setInterval", 2));
    JS_SetPropertyStr(ctx, g, "clearTimeout", JS_NewCFunction(ctx, js_clear_timer, "clearTimeout", 1));
    JS_SetPropertyStr(ctx, g, "clearInterval", JS_NewCFunction(ctx, js_clear_timer, "clearInterval", 1));
    JSValue perf = JS_NewObject(ctx); JS_SetPropertyStr(ctx, perf, "now", JS_NewCFunction(ctx, js_perf_now, "now", 0));
    JS_SetPropertyStr(ctx, g, "performance", perf);
    JS_FreeValue(ctx, g);
    JSValue r = JS_Eval(ctx, bootstrap, sizeof bootstrap - 1, "<bootstrap>", JS_EVAL_TYPE_GLOBAL); JS_FreeValue(ctx, r);
    return ctx;
}
void qar_js_free(JSContext *ctx) {
    JSRuntime *rt = JS_GetRuntime(ctx); struct host *h = H(ctx);
    for (int i = 0; i < MAX_TIMERS; i++) if (h->t[i].active) JS_FreeValue(ctx, h->t[i].fn);
    free(h); JS_FreeContext(ctx); JS_FreeRuntime(rt);
}
static void report(JSContext *ctx) {
    JSValue ex = JS_GetException(ctx); const char *s = JS_ToCString(ctx, ex);
    fprintf(stderr, "Uncaught %s\n", s ? s : "exception"); JS_FreeCString(ctx, s);
    JSValue st = JS_GetPropertyStr(ctx, ex, "stack");
    if (!JS_IsUndefined(st)) {            /* first 12 lines of the stack only: a runaway recursion can have thousands */
        const char *t = JS_ToCString(ctx, st);
        if (t) { int lines = 0; for (const char *p = t; *p; p++) { fputc(*p, stderr); if (*p == '\n' && ++lines == 12) { fputs("    ...\n", stderr); break; } } fputc('\n', stderr); }
        JS_FreeCString(ctx, t);
    }
    JS_FreeValue(ctx, st); JS_FreeValue(ctx, ex);
}
int qar_js_eval(JSContext *ctx, const char *src, const char *filename) {
    JSValue v = JS_Eval(ctx, src, strlen(src), filename, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) { report(ctx); return -1; }
    JS_FreeValue(ctx, v); return 0;
}
int qar_js_run_loop(JSContext *ctx, unsigned max_ms) {
    JSRuntime *rt = JS_GetRuntime(ctx); struct host *h = H(ctx); double start = now_ms();
    for (;;) {
        JSContext *c1; int e;
        while ((e = JS_ExecutePendingJob(rt, &c1)) > 0) ;
        if (e < 0) report(c1);
        int best = -1;
        for (int i = 0; i < MAX_TIMERS; i++) if (h->t[i].active && (best < 0 || h->t[i].due < h->t[best].due)) best = i;
        if (best < 0) return 0;
        double t = now_ms();
        if (max_ms && t - start > max_ms) return 1;
        if (h->t[best].due > t) { struct timespec d = { 0, 1000000 }; nanosleep(&d, 0); continue; }
        struct timer tm = h->t[best];
        if (tm.interval > 0) h->t[best].due = t + tm.interval; else h->t[best].active = 0;
        JSValue r = JS_Call(ctx, tm.fn, JS_UNDEFINED, 0, 0);
        if (JS_IsException(r)) report(ctx);
        JS_FreeValue(ctx, r);
        if (tm.interval <= 0) JS_FreeValue(ctx, tm.fn);
    }
}
