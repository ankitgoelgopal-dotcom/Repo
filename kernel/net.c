/* Ethernet, ARP, IPv4, ICMP, UDP, DHCP client and DNS client. Polled, single-CPU. */
#include "kernel.h"
#include "cpu.h"
#include "net.h"
#include "mem.h"
#include "rtc.h"

struct netif nif;
static int rx_depth;

/* ---------- helpers ---------- */
uint16_t ip_checksum(const void *data, size_t len, uint32_t sum) {
    const uint8_t *p = data;
    for (; len > 1; len -= 2, p += 2) sum += (uint32_t)p[0] << 8 | p[1];
    if (len) sum += (uint32_t)p[0] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}
static uint32_t rd32be(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint16_t rd16be(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static void wr16be(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = (uint8_t)v; }
static void wr32be(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = (uint8_t)v; }
static void put_ip(uint32_t ip) { kprintf("%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255); }

int net_wait(int (*cond)(void *), void *arg, unsigned timeout) {
    uint64_t deadline = ticks + timeout;
    for (;;) {
        for (int i = 0; i < 3000; i++) { net_poll(); if (cond(arg)) return 1; __asm__ volatile("pause"); }
        if (ticks >= deadline) return 0;
        sti(); __asm__ volatile("hlt"); cli();      /* let the timer tick; other tasks may run meanwhile */
    }
}

/* ---------- ARP ---------- */
struct arpe { uint32_t ip; uint8_t mac[6]; int valid; };
static struct arpe arp_cache[8]; static int arp_next;
static void arp_learn(uint32_t ip, const uint8_t *mac) {
    for (int i = 0; i < 8; i++) if (arp_cache[i].valid && arp_cache[i].ip == ip) { memcpy(arp_cache[i].mac, mac, 6); return; }
    arp_cache[arp_next] = (struct arpe){ ip, {0}, 1 }; memcpy(arp_cache[arp_next].mac, mac, 6); arp_next = (arp_next + 1) % 8;
}
static int arp_lookup(uint32_t ip, uint8_t *mac) {
    for (int i = 0; i < 8; i++) if (arp_cache[i].valid && arp_cache[i].ip == ip) { memcpy(mac, arp_cache[i].mac, 6); return 1; }
    return 0;
}
static void eth_send_arp(int op, const uint8_t *dmac, uint32_t tip, const uint8_t *tmac) {
    uint8_t f[60]; memset(f, 0, sizeof f);
    memcpy(f, dmac, 6); memcpy(f + 6, nif.mac, 6); wr16be(f + 12, 0x0806);
    wr16be(f + 14, 1); wr16be(f + 16, 0x0800); f[18] = 6; f[19] = 4; wr16be(f + 20, op);
    memcpy(f + 22, nif.mac, 6); wr32be(f + 28, nif.ip); memcpy(f + 32, tmac, 6); wr32be(f + 38, tip);
    e1000_tx(f, sizeof f);
}
static int arp_ready(void *a) { uint8_t m[6]; return arp_lookup(*(uint32_t *)a, m); }
static int arp_resolve(uint32_t ip, uint8_t *mac) {
    if (arp_lookup(ip, mac)) return 1;
    if (rx_depth) return 0;                              /* never block inside the receive path */
    static const uint8_t bc[6] = {255,255,255,255,255,255}, zero[6] = {0};
    for (int tries = 0; tries < 3; tries++) {
        eth_send_arp(1, bc, ip, zero);
        if (net_wait(arp_ready, &ip, TICK_HZ / 2)) return arp_lookup(ip, mac);
    }
    return 0;
}

/* ---------- IPv4 ---------- */
int ip_send(uint32_t src, uint32_t dst, uint8_t proto, const void *payload, size_t len) {
    static uint8_t f[1536]; static uint16_t id;
    if (len > 1480 || !nif.hw_up) return -1;
    uint8_t dmac[6];
    if (dst == 0xFFFFFFFFu) memset(dmac, 0xFF, 6);
    else {
        uint32_t nh = ((dst ^ nif.ip) & nif.mask) ? nif.gw : dst;
        if (!arp_resolve(nh, dmac)) return -1;
    }
    memcpy(f, dmac, 6); memcpy(f + 6, nif.mac, 6); wr16be(f + 12, 0x0800);
    uint8_t *ip = f + 14;
    ip[0] = 0x45; ip[1] = 0; wr16be(ip + 2, (uint16_t)(20 + len)); wr16be(ip + 4, ++id); wr16be(ip + 6, 0x4000);
    ip[8] = 64; ip[9] = proto; wr16be(ip + 10, 0); wr32be(ip + 12, src); wr32be(ip + 16, dst);
    wr16be(ip + 10, ip_checksum(ip, 20, 0));
    memcpy(ip + 20, payload, len);
    return e1000_tx(f, 14 + 20 + len);
}

/* ---------- ICMP ---------- */
static volatile int ping_got; static uint64_t ping_sent;
static int ping_ready(void *a) { (void)a; return ping_got; }
int net_ping(uint32_t ip, unsigned timeout) {
    uint8_t m[8 + 32]; memset(m, 0, sizeof m); m[0] = 8; wr16be(m + 4, 0x7172); wr16be(m + 6, 1);
    for (int i = 0; i < 32; i++) m[8 + i] = (uint8_t)i;
    wr16be(m + 2, ip_checksum(m, sizeof m, 0));
    ping_got = 0; ping_sent = ticks;
    if (ip_send(nif.ip, ip, 1, m, sizeof m)) return 0;
    if (!net_wait(ping_ready, 0, timeout)) return 0;
    return (int)(ticks - ping_sent) + 1;
}
static void icmp_rx(uint32_t src, const uint8_t *p, size_t len) {
    if (len < 8) return;
    if (p[0] == 8) {                                      /* echo request -> reply */
        uint8_t r[1480]; if (len > sizeof r) return;
        memcpy(r, p, len); r[0] = 0; wr16be(r + 2, 0); wr16be(r + 2, ip_checksum(r, len, 0));
        ip_send(nif.ip, src, 1, r, len);
    } else if (p[0] == 0 && rd16be(p + 4) == 0x7172) ping_got = 1;
}

/* ---------- UDP ---------- */
int udp_send(uint32_t src, uint16_t sport, uint32_t dst, uint16_t dport, const void *data, size_t len) {
    uint8_t d[1480]; if (len + 8 > sizeof d) return -1;
    wr16be(d, sport); wr16be(d + 2, dport); wr16be(d + 4, (uint16_t)(8 + len)); wr16be(d + 6, 0);
    memcpy(d + 8, data, len);
    uint8_t ph[12]; wr32be(ph, src); wr32be(ph + 4, dst); ph[8] = 0; ph[9] = 17; wr16be(ph + 10, (uint16_t)(8 + len));
    uint32_t seed = 0; for (int i = 0; i < 12; i += 2) seed += rd16be(ph + i);
    uint16_t c = ip_checksum(d, 8 + len, seed); wr16be(d + 6, c ? c : 0xFFFF);
    return ip_send(src, dst, 17, d, 8 + len);
}
void udp_rx(uint32_t src, const uint8_t *p, size_t len) {
    if (len < 8) return;
    uint16_t sport = rd16be(p), dport = rd16be(p + 2), ul = rd16be(p + 4);
    if (ul < 8 || ul > len) return;
    if (dport == 68) { dhcp_rx(p + 8, ul - 8); return; }
    for (int i = 0; i < MAX_SOCKS; i++) {
        struct sock *s = sock_get(i);
        if (s && s->used && s->type == SOCK_UDP && s->lport == dport && !s->dlen && (size_t)(ul - 8) <= sizeof s->dgram) {
            memcpy(s->dgram, p + 8, ul - 8); s->dlen = ul - 8; s->dsrc = src; s->dsport = sport; return;
        }
    }
}

/* ---------- DHCP ---------- */
static uint32_t dhcp_xid, dhcp_offer_ip, dhcp_server; static int dhcp_state;   /* 0 idle 1 discover 2 offered 3 requested 4 bound */
static uint32_t d_mask, d_gw, d_dns, d_ip;
static uint8_t *dhcp_build(uint8_t *b, int type) {
    memset(b, 0, 300);
    b[0] = 1; b[1] = 1; b[2] = 6; wr32be(b + 4, dhcp_xid); wr16be(b + 10, 0x8000);
    memcpy(b + 28, nif.mac, 6); wr32be(b + 236, 0x63825363);
    uint8_t *o = b + 240;
    *o++ = 53; *o++ = 1; *o++ = (uint8_t)type;
    if (type == 3) { *o++ = 50; *o++ = 4; wr32be(o, dhcp_offer_ip); o += 4; *o++ = 54; *o++ = 4; wr32be(o, dhcp_server); o += 4; }
    *o++ = 55; *o++ = 3; *o++ = 1; *o++ = 3; *o++ = 6; *o++ = 255;
    return o;
}
void dhcp_rx(const uint8_t *p, size_t len) {
    if (len < 241 || p[0] != 2 || rd32be(p + 4) != dhcp_xid || rd32be(p + 236) != 0x63825363) return;
    uint32_t yi = rd32be(p + 16), srv = 0, mask = 0, gw = 0, dns = 0; int type = 0;
    for (size_t i = 240; i + 1 < len && p[i] != 255;) {
        if (p[i] == 0) { i++; continue; }
        uint8_t t = p[i], l = p[i + 1]; if (i + 2 + l > len) break;
        if (t == 53 && l == 1) type = p[i + 2];
        else if (t == 54 && l == 4) srv = rd32be(p + i + 2);
        else if (t == 1 && l == 4) mask = rd32be(p + i + 2);
        else if (t == 3 && l >= 4) gw = rd32be(p + i + 2);
        else if (t == 6 && l >= 4) dns = rd32be(p + i + 2);
        i += 2 + l;
    }
    if (type == 2 && dhcp_state == 1) { dhcp_offer_ip = yi; dhcp_server = srv; dhcp_state = 2; }
    else if (type == 5 && dhcp_state == 3) { d_ip = yi; d_mask = mask ? mask : 0xFFFFFF00u; d_gw = gw; d_dns = dns; dhcp_state = 4; }
}
static int dhcp_is(void *want) { return dhcp_state == (int)(long)want; }
static int dhcp_run(void) {
    uint8_t b[320]; dhcp_xid = 0x51415200u ^ (uint32_t)ticks;
    for (int attempt = 0; attempt < 4; attempt++) {
        dhcp_state = 1; uint8_t *e = dhcp_build(b, 1);
        udp_send(0, 68, 0xFFFFFFFFu, 67, b, 300 > (size_t)(e - b) ? 300 : (size_t)(e - b));
        if (!net_wait(dhcp_is, (void *)2L, TICK_HZ)) continue;
        dhcp_state = 3; e = dhcp_build(b, 3);
        udp_send(0, 68, 0xFFFFFFFFu, 67, b, 300);
        if (net_wait(dhcp_is, (void *)4L, TICK_HZ)) {
            nif.ip = d_ip; nif.mask = d_mask; nif.gw = d_gw; nif.dns = d_dns; nif.configured = 1; return 0;
        }
    }
    return -1;
}

/* ---------- DNS (A records over UDP) ---------- */
static int dns_ready(void *s) { return ((struct sock *)s)->dlen > 0; }
int dns_resolve(const char *name, uint32_t *out) {
    if (!nif.configured || !nif.dns) return -101;
    uint8_t q[300]; size_t n = 12; memset(q, 0, 12);
    uint16_t id = (uint16_t)(0x5100 + (ticks & 0xFF)); wr16be(q, id); wr16be(q + 2, 0x0100); wr16be(q + 4, 1);
    const char *p = name;
    while (*p) {
        const char *dot = p; while (*dot && *dot != '.') dot++;
        size_t l = (size_t)(dot - p); if (!l || l > 63 || n + l + 6 > sizeof q) return -22;
        q[n++] = (uint8_t)l; memcpy(q + n, p, l); n += l; p = *dot ? dot + 1 : dot;
    }
    q[n++] = 0; wr16be(q + n, 1); wr16be(q + n + 2, 1); n += 4;
    struct sock *s = sock_alloc(SOCK_UDP); if (!s) return -24;
    int rc = -110;
    for (int t = 0; t < 3 && rc == -110; t++) {
        s->dlen = 0; udp_send(nif.ip, s->lport, nif.dns, 53, q, n);
        if (!net_wait(dns_ready, s, TICK_HZ * 2)) continue;
        const uint8_t *r = s->dgram; int rl = s->dlen;
        if (rl < 12 || rd16be(r) != id) { continue; }
        if ((r[3] & 0x0F) != 0) { rc = -2; break; }                        /* NXDOMAIN etc. */
        int pos = 12;
        for (int i = 0; i < rd16be(r + 4) && pos < rl; i++) {              /* skip question */
            while (pos < rl && r[pos]) pos += (r[pos] & 0xC0) == 0xC0 ? 1 : r[pos] + 1;
            pos += 5;
        }
        for (int i = 0; i < rd16be(r + 6) && pos + 12 <= rl; i++) {
            while (pos < rl && r[pos] && (r[pos] & 0xC0) != 0xC0) pos += r[pos] + 1;
            pos += (pos < rl && (r[pos] & 0xC0) == 0xC0) ? 2 : 1;
            if (pos + 10 > rl) break;
            uint16_t type = rd16be(r + pos), rdl = rd16be(r + pos + 8); pos += 10;
            if (type == 1 && rdl == 4 && pos + 4 <= rl) { *out = rd32be(r + pos); rc = 0; break; }
            pos += rdl;
        }
        if (rc == -110) rc = -2;
    }
    sock_free(s); return rc;
}

/* ---------- receive path ---------- */
static void ip_rx(const uint8_t *eth, size_t len) {
    const uint8_t *ip = eth + 14; size_t l = len - 14;
    if (l < 20 || (ip[0] >> 4) != 4) return;
    size_t ihl = (ip[0] & 15) * 4, tot = rd16be(ip + 2);
    if (ihl < 20 || tot < ihl || tot > l) return;
    if (rd16be(ip + 6) & 0x3FFF) return;                                 /* fragments: not supported, dropped */
    uint32_t src = rd32be(ip + 12), dst = rd32be(ip + 16);
    if (nif.configured && dst != nif.ip && dst != 0xFFFFFFFFu) return;
    if (nif.configured && !((src ^ nif.ip) & nif.mask)) arp_learn(src, eth + 6);
    else if (src == nif.gw && src) arp_learn(src, eth + 6);
    const uint8_t *pl = ip + ihl; size_t pll = tot - ihl;
    switch (ip[9]) { case 1: icmp_rx(src, pl, pll); break; case 6: tcp_rx(src, pl, pll); break; case 17: udp_rx(src, pl, pll); break; }
}
void net_poll(void) {
    static uint8_t f[2048]; int n;
    rx_depth++;
    while ((n = e1000_rx(f, sizeof f)) != 0) {
        if (n < 14) continue;
        entropy_event();
        uint16_t et = rd16be(f + 12);
        if (et == 0x0800) ip_rx(f, (size_t)n);
        else if (et == 0x0806 && n >= 42 && rd16be(f + 14 + 6) <= 2) {
            uint16_t op = rd16be(f + 20); uint32_t sip = rd32be(f + 28), tip = rd32be(f + 38);
            if (sip) arp_learn(sip, f + 22);
            if (op == 1 && nif.configured && tip == nif.ip) eth_send_arp(2, f + 22, sip, f + 22);
        }
    }
    rx_depth--;
}

void net_init(void) {
    if (e1000_init()) return;
    if (dhcp_run()) { kprintf("[net ] DHCP failed: no lease\n"); return; }
    kprintf("[net ] DHCP: ip "); put_ip(nif.ip); kprintf(" mask "); put_ip(nif.mask);
    kprintf(" gw "); put_ip(nif.gw); kprintf(" dns "); put_ip(nif.dns); kprintf("\n");
    int rtt = net_ping(nif.gw, TICK_HZ);
    uint8_t m[6];
    if (arp_lookup(nif.gw, m)) kprintf("[net ] ARP: gateway is at %02x:%02x:%02x:%02x:%02x:%02x\n", m[0], m[1], m[2], m[3], m[4], m[5]);
    if (rtt) kprintf("[net ] ICMP: ping gateway reply, rtt <= %d ms\n", rtt * 1000 / TICK_HZ);
    else kprintf("[net ] ICMP: ping gateway: no reply\n");
}
