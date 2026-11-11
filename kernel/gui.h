#ifndef QAR_GUI_H
#define QAR_GUI_H
#include <stdint.h>
extern int gui_active;
int  gui_init(void);                      /* framebuffer, PS/2 input, log window; 0 = ok */
void gui_thread(void);                    /* compositor + input loop, runs as a kernel thread */
void gui_console_putc(char c);            /* kernel log -> log window (safe in any context) */
void gui_close_owner(int pid);            /* process exited: remove its windows */
long gui_syscall(uint64_t n, uint64_t a, uint64_t b, uint64_t c, uint64_t d);
#endif
