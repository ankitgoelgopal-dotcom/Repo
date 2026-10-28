/* Socket table and the kernel-side socket API used by system calls. */
#include "kernel.h"
#include "net.h"
#include "mem.h"
#include "os_abi.h"
#include "cpu.h"

static struct sock socks[MAX_SOCKS];
static uint16_t next_port = 49152;

uint16_t net_ephemeral_port(void) { if (next_port >= 65000) next_port = 49152; return next_port++; }
struct sock *sock_get(int id) { return (id >= 0 && id < MAX_SOCKS) ? &socks[id] : 0; }
int sock_id(struct sock *s) { return (int)(s - socks); }
struct sock *sock_alloc(int type) {
    for (int i = 0; i < MAX_SOCKS; i++) if (!socks[i].used) {
        memset(&socks[i], 0, sizeof socks[i]);
        socks[i].used = 1; socks[i].type = type; socks[i].lport = net_ephemeral_port();
        return &socks[i];
    }
    return 0;
}
void sock_free(struct sock *s) { s->used = 0; }

#define AF_INET 2
int net_socket(int domain, int type) {
    if (!nif.configured) return -QAR_ENETDOWN;
    if (domain != AF_INET || (type != SOCK_TCP && type != SOCK_UDP)) return -QAR_EINVAL;
    struct sock *s = sock_alloc(type);
    return s ? sock_id(s) : -QAR_EMFILE;
}
int net_connect(int id, uint32_t ip, uint16_t port) {
    struct sock *s = sock_get(id); if (!s || !s->used) return -QAR_EBADF;
    if (s->type == SOCK_UDP) { s->rip = ip; s->rport = port; return 0; }
    return tcp_connect(s, ip, port, TICK_HZ * 5);
}
long net_send(int id, const uint8_t *buf, size_t len) {
    struct sock *s = sock_get(id); if (!s || !s->used) return -QAR_EBADF;
    if (s->type == SOCK_UDP) {
        if (!s->rport) return -QAR_EINVAL;
        if (len > 1400) len = 1400;
        return udp_send(nif.ip, s->lport, s->rip, s->rport, buf, len) ? -QAR_EHOSTUNREACH : (long)len;
    }
    return tcp_send(s, buf, len);
}
static int dg_ready(void *p) { return ((struct sock *)p)->dlen > 0; }
long net_recv(int id, uint8_t *buf, size_t cap) {
    struct sock *s = sock_get(id); if (!s || !s->used) return -QAR_EBADF;
    if (s->type == SOCK_TCP) return tcp_recv(s, buf, cap, TICK_HZ * 10);
    if (!net_wait(dg_ready, s, TICK_HZ * 5)) return -QAR_ETIMEDOUT;
    size_t n = (size_t)s->dlen < cap ? (size_t)s->dlen : cap; memcpy(buf, s->dgram, n); s->dlen = 0; return (long)n;
}
int net_close(int id) {
    struct sock *s = sock_get(id); if (!s || !s->used) return -QAR_EBADF;
    if (s->type == SOCK_TCP) tcp_close(s);
    sock_free(s); return 0;
}
