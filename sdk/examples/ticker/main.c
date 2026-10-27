/* Busy-loops for ~300 ms WITHOUT yielding: only timer preemption lets the other ticker run. */
#include <stdio.h>
#include "os_api.h"
#ifndef TAG
#define TAG "?"
#endif
int main(void) {
    uint64_t start = os_uptime_ms(), last = start; int n = 0;
    while (os_uptime_ms() - start < 300)
        if (os_uptime_ms() - last >= 50) { last = os_uptime_ms(); printf("[%s] tick %d\n", TAG, ++n); }
    return 0;
}
