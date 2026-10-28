/* TCP client: active open, in-order receive, stop-and-wait send with retransmit, active close.
 * Not implemented yet: listen/accept, out-of-order reassembly, congestion control, window scaling, SACK. */
#include "kernel.h"
#include "cpu.h"
#include "net.h"
#include "mem.h"
#include "os_abi.h"

#define F_FIN 1
#define F_SYN 2
#define F_RST 4
#define F_PSH 8
#define F_ACK 16

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static void wr16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = (uint8_t)v; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = (uint8_t)v; }

static void xmit(struct sock *s, uint8_t flags, uint32_t seq, const uint8_t *data, size_t len) {
    uint8_t seg[1480]; size_t hl = (flags & F_SYN) ? 24 : 20;
    if (hl + len > sizeof seg) return;
    wr16(seg, s->lport); wr16(seg + 2, s->rport); wr32(seg + 4, seq); wr32(seg + 8, (flags & F_ACK) ? s->rcv_nxt : 0);
    seg[12] = (uint8_t)(hl / 4 << 4); seg[13] = flags;
    wr16(seg + 14, (uint16_t)(SOCK_RXBUF - s->rlen)); wr16(seg + 16, 0); wr16(seg + 18, 0);
    if (flags & F_SYN) { seg[20] = 2; seg[21] = 4; wr16(seg + 22, 1460); }          /* MSS option */
    if (len) memcpy(seg + hl, data, len);
    uint8_t ph[12]; wr32(ph, nif.ip); wr32(ph + 4, s->rip); ph[8] = 0; ph[9] = 6; wr16(ph + 10, (uint16_t)(hl + len));
    uint32_t seed = 0; for (int i = 0; i < 12; i += 2) seed += rd16(ph + i);
    wr16(seg + 16, ip_checksum(seg, hl + len, seed));
    ip_send(nif.ip, s->rip, 6, seg, hl + len);
}

void tcp_rx(uint32_t src, const uint8_t *p, size_t len) {
    if (len < 20) return;
    uint16_t sport = rd16(p), dport = rd16(p + 2); uint32_t seq = rd32(p + 4), ack = rd32(p + 8);
    size_t hl = (p[12] >> 4) * 4; uint8_t fl = p[13];
    if (hl < 20 || hl > len) return;
    struct sock *s = 0;
    for (int i = 0; i < MAX_SOCKS; i++) {
        struct sock *c = sock_get(i);
        if (c->used && c->type == SOCK_TCP && c->state != TCP_CLOSED && c->lport == dport && c->rport == sport && c->rip == src) { s = c; break; }
    }
    if (!s) return;
    const uint8_t *data = p + hl; size_t dl = len - hl;
    if (fl & F_RST) { s->state = TCP_CLOSED; s->err = QAR_ECONNRESET; s->eof = 1; return; }
    if (s->state == TCP_SYN_SENT) {
        if ((fl & (F_SYN | F_ACK)) == (F_SYN | F_ACK) && ack == s->snd_nxt) {
            s->snd_una = ack; s->rcv_nxt = seq + 1; s->state = TCP_ESTABLISHED; xmit(s, F_ACK, s->snd_nxt, 0, 0);
        }
        return;
    }
    if ((fl & F_ACK) && (int32_t)(ack - s->snd_una) > 0 && (int32_t)(ack - s->snd_nxt) <= 0) s->snd_una = ack;
    int need_ack = 0;
    if (dl) {
        if (seq == s->rcv_nxt) {
            size_t room = SOCK_RXBUF - s->rlen, take = dl < room ? dl : room;
            for (size_t i = 0; i < take; i++) s->rx[(s->rpos + s->rlen + i) % SOCK_RXBUF] = data[i];
            s->rlen += (uint32_t)take; s->rcv_nxt += (uint32_t)take;
            if (take < dl) fl &= ~F_FIN;                      /* did not take everything: peer must resend */
        }
        need_ack = 1;
    }
    if ((fl & F_FIN) && seq + dl == s->rcv_nxt) { s->rcv_nxt++; s->eof = 1; need_ack = 1; }
    if (need_ack) xmit(s, F_ACK, s->snd_nxt, 0, 0);
}

static int est_or_fail(void *a) { struct sock *s = a; return s->state != TCP_SYN_SENT; }
int tcp_connect(struct sock *s, uint32_t ip, uint16_t port, unsigned timeout) {
    if (!nif.configured) return -QAR_ENETDOWN;
    uint32_t iss = (uint32_t)(ticks * 64000 + 1);
    s->rip = ip; s->rport = port; s->snd_una = iss; s->snd_nxt = iss + 1;        /* SYN consumes one sequence number */
    s->state = TCP_SYN_SENT;
    for (int t = 0; t < 3; t++) {
        xmit(s, F_SYN, iss, 0, 0);
        if (net_wait(est_or_fail, s, timeout / 3 ? timeout / 3 : 1)) break;
    }
    if (s->state == TCP_ESTABLISHED) return 0;
    int err = s->state == TCP_CLOSED ? -QAR_ECONNREFUSED : -QAR_ETIMEDOUT;
    s->state = TCP_CLOSED; return err;
}

static int acked(void *a) { struct sock *s = a; return s->snd_una == s->snd_nxt || s->state == TCP_CLOSED; }
int tcp_send(struct sock *s, const uint8_t *data, size_t len) {
    if (s->state != TCP_ESTABLISHED) return -QAR_ENOTCONN;
    size_t sent = 0;
    while (sent < len) {
        size_t n = len - sent; if (n > 1400) n = 1400;
        uint32_t seq = s->snd_nxt; s->snd_nxt += (uint32_t)n;
        int ok = 0;
        for (int t = 0; t < 6 && !ok; t++) {
            xmit(s, F_ACK | F_PSH, seq, data + sent, n);
            ok = net_wait(acked, s, TICK_HZ);
            if (s->state == TCP_CLOSED) return -QAR_ECONNRESET;
        }
        if (!ok) { s->state = TCP_CLOSED; return -QAR_ETIMEDOUT; }
        sent += n;
    }
    return (int)sent;
}

static int has_data(void *a) { struct sock *s = a; return s->rlen > 0 || s->eof || s->state == TCP_CLOSED; }
int tcp_recv(struct sock *s, uint8_t *buf, size_t cap, unsigned timeout) {
    if (!net_wait(has_data, s, timeout)) return -QAR_ETIMEDOUT;
    if (!s->rlen) return s->err ? -s->err : 0;                                     /* 0 = orderly EOF */
    size_t n = s->rlen < cap ? s->rlen : cap;
    for (size_t i = 0; i < n; i++) buf[i] = s->rx[(s->rpos + i) % SOCK_RXBUF];
    s->rpos = (s->rpos + (uint32_t)n) % SOCK_RXBUF; s->rlen -= (uint32_t)n;
    return (int)n;
}

void tcp_close(struct sock *s) {
    if (s->state == TCP_ESTABLISHED) {
        uint32_t seq = s->snd_nxt; s->snd_nxt++;
        xmit(s, F_FIN | F_ACK, seq, 0, 0); s->state = TCP_FIN_SENT;
        net_wait(acked, s, TICK_HZ);
    }
    s->state = TCP_CLOSED;
}
