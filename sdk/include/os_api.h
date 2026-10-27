/* QAR-OS application API. Include this (or the libc headers) from your app. */
#ifndef QAR_OS_API_H
#define QAR_OS_API_H
#include <stdint.h>
#include <stddef.h>
#include "os_abi.h"

static inline long os_syscall(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c) : "memory", "rcx", "r11");
    return r;
}
static inline __attribute__((noreturn)) void os_exit(int code) { os_syscall(SYS_EXIT, code, 0, 0); for (;;) ; }
static inline long os_write(int fd, const void *buf, size_t n) { return os_syscall(SYS_WRITE, fd, (long)buf, (long)n); }
static inline void os_yield(void)           { os_syscall(SYS_YIELD, 0, 0, 0); }
static inline int  os_getpid(void)          { return (int)os_syscall(SYS_GETPID, 0, 0, 0); }
static inline void os_sleep_ms(unsigned ms) { os_syscall(SYS_SLEEP_MS, ms, 0, 0); }
static inline uint64_t os_uptime_ms(void)   { return (uint64_t)os_syscall(SYS_UPTIME_MS, 0, 0, 0); }

/* Sockets: ABI reserved, returns -QAR_ENOSYS until the network stack lands (roadmap phase 7). */
static inline int os_socket(int domain, int type, int proto) { return (int)os_syscall(SYS_SOCKET, domain, type, proto); }
#endif
