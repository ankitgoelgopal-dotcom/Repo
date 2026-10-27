/* Deliberately dereferences kernel memory: the OS must kill this process and keep running. */
#include <stdio.h>
int main(void) {
    puts("crash app: about to touch kernel memory...");
    volatile int *p = (volatile int *)0x1000;
    *p = 1;
    puts("crash app: THIS LINE MUST NEVER PRINT");
    return 0;
}
