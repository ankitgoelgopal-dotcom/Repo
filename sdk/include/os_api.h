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
#endif
