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
static inline long os_syscall4(long n, long a, long b, long c, long d) {
    long r; register long r10 __asm__("r10") = d;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10) : "memory", "rcx", "r11");
    return r;
}
static inline __attribute__((noreturn)) void os_exit(int code) { os_syscall(SYS_EXIT, code, 0, 0); for (;;) ; }
static inline long os_write(int fd, const void *buf, size_t n) { return os_syscall(SYS_WRITE, fd, (long)buf, (long)n); }
static inline void os_yield(void)           { os_syscall(SYS_YIELD, 0, 0, 0); }
static inline int  os_getpid(void)          { return (int)os_syscall(SYS_GETPID, 0, 0, 0); }
static inline void os_sleep_ms(unsigned ms) { os_syscall(SYS_SLEEP_MS, ms, 0, 0); }
static inline uint64_t os_uptime_ms(void)   { return (uint64_t)os_syscall(SYS_UPTIME_MS, 0, 0, 0); }

static inline long os_sbrk_raw(long inc) { return os_syscall(SYS_SBRK, inc, 0, 0); }   /* grows the heap; returns old break or -errno */
static inline long os_getrandom(void *buf, size_t n) { return os_syscall(SYS_GETRANDOM, (long)buf, (long)n, 0); }
static inline uint64_t os_time_unix(void)     { return (uint64_t)os_syscall(SYS_TIME_UNIX, 0, 0, 0); }

/* ---- Networking (manifest must list the "net.connect" capability, otherwise calls return -QAR_EPERM) ---- */
#define AF_INET     2
#define SOCK_STREAM 1   /* TCP */
#define SOCK_DGRAM  2   /* UDP */
#define IP4(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

static inline int  os_socket(int domain, int type, int proto) { return (int)os_syscall(SYS_SOCKET, domain, type, proto); }
static inline int  os_connect(int fd, uint32_t ip, uint16_t port) { return (int)os_syscall(SYS_CONNECT, fd, (long)ip, port); }
static inline long os_send(int fd, const void *buf, size_t n) { return os_syscall(SYS_SEND, fd, (long)buf, (long)n); }
static inline long os_recv(int fd, void *buf, size_t n)       { return os_syscall(SYS_RECV, fd, (long)buf, (long)n); }
static inline int  os_close(int fd)                           { return (int)os_syscall(SYS_CLOSE, fd, 0, 0); }
/* DNS lookup of an IPv4 address. Returns 0 on success. */
static inline int  os_resolve(const char *name, uint32_t *ip) { return (int)os_syscall(SYS_RESOLVE, (long)name, (long)ip, 0); }
/* out = {ip, netmask, gateway, dns}; returns 0 if the network is configured. */
static inline int  os_netinfo(uint32_t out[4])                { return (int)os_syscall(SYS_NETINFO, (long)out, 0, 0); }

/* ---- Window system (manifest must list the "gui.window" capability; see docs/WINDOW-SYSTEM.md) ---- */
static inline int os_screen_info(struct qar_screen *s)                { return (int)os_syscall(SYS_SCREEN_INFO, (long)s, 0, 0); }
static inline int os_win_create(const struct qar_win_create *c)       { return (int)os_syscall(SYS_WIN_CREATE, (long)c, 0, 0); }
static inline int os_win_destroy(int id)                              { return (int)os_syscall(SYS_WIN_DESTROY, id, 0, 0); }
static inline int os_win_title(int id, const char *t)                 { return (int)os_syscall(SYS_WIN_TITLE, id, (long)t, 0); }
/* copy pixels (xRGB8888, rows `stride_bytes` apart; (0,0) of the buffer = window content (0,0)) into the window; r = NULL: all */
static inline int os_win_present(int id, const void *px, int stride_bytes, const struct qar_rect *r) { return (int)os_syscall4(SYS_WIN_PRESENT, id, (long)px, stride_bytes, (long)r); }
/* wait for the next event of any of this process's windows: 1 = event stored, 0 = timeout, <0 = error/no windows */
static inline int os_win_poll(struct qar_event *e, int timeout_ms)    { return (int)os_syscall(SYS_WIN_POLL, (long)e, timeout_ms, 0); }
#endif
