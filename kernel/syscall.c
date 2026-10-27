#include "kernel.h"
#include "cpu.h"
#include "sched.h"
#include "os_abi.h"

long syscall_dispatch(struct regs *r) {
    uint64_t a = r->rdi, b = r->rsi, c = r->rdx;
    switch (r->rax) {
    case SYS_EXIT:      task_exit((int)a);
    case SYS_WRITE: {
        if (a != 1 && a != 2) return -QAR_EINVAL;
        if (c > 4096) c = 4096;
        if (!user_range_ok(current_aspace(), b, c, 0)) return -QAR_EFAULT;
        for (uint64_t i = 0; i < c; i++) kputc(((const char *)b)[i]);
        return (long)c; }
    case SYS_YIELD:     task_yield(); return 0;
    case SYS_GETPID:    return current_pid();
    case SYS_SLEEP_MS:  task_sleep_ticks((a * TICK_HZ + 999) / 1000); return 0;
    case SYS_UPTIME_MS: return (long)(ticks * 1000 / TICK_HZ);
    case SYS_SOCKET: case SYS_CONNECT: case SYS_SEND: case SYS_RECV: case SYS_CLOSE:
        return -QAR_ENOSYS;
    default:            return -QAR_ENOSYS;
    }
}
