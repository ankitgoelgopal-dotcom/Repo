#ifndef QAR_NET_H
#define QAR_NET_H
#include <stdint.h>
#include <stddef.h>

struct netif { uint8_t mac[6]; uint32_t ip, mask, gw, dns; int hw_up, configured; };
extern struct netif nif;
#define IP4(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

static inline uint16_t bswap16(uint16_t x) { return (uint16_t)(x << 8 | x >> 8); }
#define htons(x) bswap16(x)
#define ntohs(x) bswap16(x)
#define htonl(x) __builtin_bswap32(x)
#define ntohl(x) __builtin_bswap32(x)

/* driver (e1000.c) */
int  e1000_init(void);
int  e1000_tx(const void *frame, size_t len);
int  e1000_rx(void *buf, size_t cap);          /* returns frame length, 0 if none */

/* core (net.c) */
void net_init(void);
void net_poll(void);
int  net_wait(int (*cond)(void *), void *arg, unsigned timeout_ticks);
int  ip_send(uint32_t src, uint32_t dst, uint8_t proto, const void *payload, size_t len);
uint16_t ip_checksum(const void *data, size_t len, uint32_t seed);
int  udp_send(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport, const void *data, size_t len);
int  net_ping(uint32_t ip, unsigned timeout_ticks);       /* returns rtt in ticks+1, or 0 */
int  dns_resolve(const char *name, uint32_t *ip_out);     /* 0 ok, <0 error */

/* sockets (sock.c, tcp.c) */
#define SOCK_TCP 1
#define SOCK_UDP 2
#define MAX_SOCKS 16
#define SOCK_RXBUF 8192
enum { TCP_CLOSED, TCP_SYN_SENT, TCP_ESTABLISHED, TCP_FIN_SENT };
struct sock {
    int used, type, state, err, eof;
    uint16_t lport, rport; uint32_t rip;
    uint32_t snd_una, snd_nxt, rcv_nxt;
    uint8_t rx[SOCK_RXBUF]; uint32_t rpos, rlen;      /* tcp receive ring */
    uint8_t dgram[1500]; int dlen; uint32_t dsrc; uint16_t dsport;   /* udp: one pending datagram */
};
struct sock *sock_alloc(int type);
void sock_free(struct sock *s);
struct sock *sock_get(int id);
int  sock_id(struct sock *s);
uint16_t net_ephemeral_port(void);
void tcp_rx(uint32_t src, const uint8_t *seg, size_t len);
void udp_rx(uint32_t src, const uint8_t *dg, size_t len);
void dhcp_rx(const uint8_t *p, size_t len);

int  tcp_connect(struct sock *s, uint32_t ip, uint16_t port, unsigned timeout_ticks);
int  tcp_send(struct sock *s, const uint8_t *data, size_t len);
int  tcp_recv(struct sock *s, uint8_t *buf, size_t cap, unsigned timeout_ticks);
void tcp_close(struct sock *s);

/* socket API used by syscalls: all return >=0 or -errno */
int  net_socket(int domain, int type);
int  net_connect(int id, uint32_t ip, uint16_t port);
long net_send(int id, const uint8_t *buf, size_t len);
long net_recv(int id, uint8_t *buf, size_t cap);
int  net_close(int id);
#endif
