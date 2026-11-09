/* Runs the embedded JavaScript conformance script (tests.js) on the QuickJS engine. */
#include <stdio.h>
#include "qar_js.h"
#include "jstest_script.h"
int main(void) {
    JSContext *ctx = qar_js_new();
    if (!ctx) { puts("JS FAIL: engine init"); return 1; }
    int r = qar_js_eval(ctx, tests_js, "tests.js");
    qar_js_run_loop(ctx, 20000);
    qar_js_free(ctx);
    return r ? 1 : 0;
}
