/* Two copies run at once. Each does a long deterministic floating-point computation; if the kernel failed to
 * save/restore SSE/x87 state on task switches, the results would be corrupted. The host checks the exact bits. */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "os_api.h"
#ifndef TAG
#define TAG "?"
#endif
#ifndef X0
#define X0 1.5
#define Y0 0.5
#endif
int main(void) {
    uint64_t t0 = os_uptime_ms();
    double x = X0, y = Y0;
    for (long i = 1; i <= 1500000; i++) {
        x = x * 1.0000001 + 1e-9 * (double)i;
        y = y * 0.9999999 + sqrt(x) * 1e-6;
    }
    uint64_t t1 = os_uptime_ms(), bx, by; memcpy(&bx, &x, 8); memcpy(&by, &y, 8);
    printf("FPU[%s] x=%016llx y=%016llx start=%llu end=%llu\n", TAG, (unsigned long long)bx, (unsigned long long)by,
           (unsigned long long)t0, (unsigned long long)t1);
    return 0;
}
