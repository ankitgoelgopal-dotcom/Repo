#include "kernel.h"
#include "cpu.h"
#include "sched.h"
#include "os_abi.h"
#include "net.h"
#include "mem.h"

static int fd_to_sock(uint64_t fd) {
    int *t = current_fds();
    return (fd < 8) ? t[fd] : -1;
}
static long net_syscall(uint64_t n, uint64_t a, uint64_t b, uint64_t c) {
    if (!(current_caps() & CAP_NET) && n != SYS_NETINFO) return -QAR_EPERM;      /* capability check */
    struct aspace *as = current_aspace();
    switch (n) {
    case SYS_SOCKET: {
        int *t = current_fds(); int slot = -1;
        for (int i = 0; i < 8; i++) if (t[i] < 0) { slot = i; break; }
        if (slot < 0) return -QAR_EMFILE;
        int id = net_socket((int)a, (int)b); if (id < 0) return id;
        t[slot] = id; return slot; }
    case SYS_CONNECT: { int id = fd_to_sock(a); return id < 0 ? -QAR_EBADF : net_connect(id, (uint32_t)b, (uint16_t)c); }
    case SYS_SEND: {
        int id = fd_to_sock(a); if (id < 0) return -QAR_EBADF;
        if (c > 65536) c = 65536;
        if (!user_range_ok(as, b, c, 0)) return -QAR_EFAULT;
        return net_send(id, (const uint8_t *)b, c); }
    case SYS_RECV: {
        int id = fd_to_sock(a); if (id < 0) return -QAR_EBADF;
        if (c > 65536) c = 65536;
        if (!user_range_ok(as, b, c, 1)) return -QAR_EFAULT;
        return net_recv(id, (uint8_t *)b, c); }
    case SYS_CLOSE: { int id = fd_to_sock(a); if (id < 0) return -QAR_EBADF; current_fds()[a] = -1; return net_close(id); }
    case SYS_RESOLVE: {
        char name[256]; size_t i = 0;
        for (; i < 255; i++) {
            if (!user_range_ok(as, a + i, 1, 0)) return -QAR_EFAULT;
            name[i] = ((const char *)a)[i]; if (!name[i]) break;
        }
        if (i == 255) return -QAR_EINVAL;
        if (!user_range_ok(as, b, 4, 1)) return -QAR_EFAULT;
        uint32_t ip; int rc = dns_resolve(name, &ip); if (rc) return rc;
        *(uint32_t *)b = ip; return 0; }
    case SYS_NETINFO: {
        if (!user_range_ok(as, a, 16, 1)) return -QAR_EFAULT;
        uint32_t *o = (uint32_t *)a; o[0] = nif.ip; o[1] = nif.mask; o[2] = nif.gw; o[3] = nif.dns; return nif.configured ? 0 : -QAR_ENETDOWN; }
    }
    return -QAR_ENOSYS;
}

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
    case SYS_RESOLVE: case SYS_NETINFO:
        return net_syscall(r->rax, a, b, c);
    default:            return -QAR_ENOSYS;
    }
}
