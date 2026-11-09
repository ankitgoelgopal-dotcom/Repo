/* Host glue for the QuickJS engine on QAR-OS: console, timers, microtasks, error reporting.
 * Link with:  qar-cc app.c -lquickjs -o main     (the SDK also ships quickjs.h) */
#ifndef QAR_JS_H
#define QAR_JS_H
#include "quickjs.h"

/* Creates a runtime (memory limit and stack limit set for QAR-OS) and a context with the host globals:
 * print, console.{log,info,warn,error,debug}, setTimeout/setInterval/clearTimeout/clearInterval,
 * queueMicrotask, performance.now, globalThis. */
JSContext *qar_js_new(void);
void       qar_js_free(JSContext *ctx);
/* Evaluates a script. Returns 0 on success; on a JS exception prints "Uncaught ..." with the stack and returns -1. */
int        qar_js_eval(JSContext *ctx, const char *src, const char *filename);
/* Runs pending promise jobs and timers until nothing is left (or max_ms elapsed, 0 = no limit). Returns 0 if idle. */
int        qar_js_run_loop(JSContext *ctx, unsigned max_ms);
#endif
