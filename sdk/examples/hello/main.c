#include <stdio.h>
#include <stdlib.h>
#include "os_api.h"
int main(void) {
    printf("Hello from a signed .qar app! pid=%d uptime=%lums\n", os_getpid(), (unsigned long)os_uptime_ms());
    char *p = malloc(100); snprintf(p, 100, "heap works: %d + %d = %d", 20, 22, 42); puts(p); free(p);
    long r = os_socket(2, 1, 0);
    printf("network socket syscall -> %ld (expected -38 until the network stack exists)\n", r);
    return 0;
}
