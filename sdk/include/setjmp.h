#ifndef QAR_SETJMP_H
#define QAR_SETJMP_H
typedef long jmp_buf[8];     /* rbx rbp r12-r15 rsp rip */
int  setjmp(jmp_buf env) __attribute__((returns_twice));
int  _setjmp(jmp_buf env) __attribute__((returns_twice));
void longjmp(jmp_buf env, int val) __attribute__((noreturn));
void _longjmp(jmp_buf env, int val) __attribute__((noreturn));
#define setjmp(e) setjmp(e)
#endif
