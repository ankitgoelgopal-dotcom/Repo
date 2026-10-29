/* Kernel random number source. Pool is hashed with SHA-256; outputs are SHA-256(pool || counter || fresh data).
 * Inputs: RDRAND (if the CPU has it), TSC, tick count, RTC, NIC MAC, and the arrival time of every network frame and timer tick.
 * HONEST LIMIT: without RDRAND (typical in emulators, e.g. Limbo/QEMU "qemu64" CPU) the only unpredictability is timing
 * jitter. That is fine for development and tests but not for protecting real secrets. Real hardware with RDRAND is OK. */
#include "kernel.h"
#include "sha256.h"
#include "mem.h"
#include "rtc.h"
#include "net.h"

static uint8_t pool[32]; static uint64_t counter; static int have_rdrand;

static inline uint64_t rdtsc(void) { uint32_t lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (uint64_t)hi << 32 | lo; }
static int rdrand64(uint64_t *v) {
    for (int i = 0; i < 10; i++) { unsigned char ok; uint64_t x; __asm__ volatile("rdrand %0; setc %1" : "=r"(x), "=qm"(ok)); if (ok) { *v = x; return 1; } }
    return 0;
}
void entropy_add(const void *data, size_t n) {
    struct sha256 s; uint64_t t = rdtsc(); sha256_init(&s); sha256_update(&s, pool, 32);
    sha256_update(&s, &t, 8); sha256_update(&s, data, n); sha256_final(&s, pool);
}
void entropy_event(void) { uint64_t t = rdtsc(); entropy_add(&t, 8); }

void entropy_init(void) {
    uint32_t a = 1, b, c = 0, d; __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    have_rdrand = (c >> 30) & 1;
    uint64_t now = rtc_unix_time(); entropy_add(&now, 8);
    entropy_add(nif.mac, 6);
    for (int i = 0; i < 64; i++) entropy_event();
    kprintf("[rand] entropy pool seeded (%s)\n", have_rdrand ? "RDRAND available" : "no RDRAND: timing jitter only, weak in emulators");
}
void entropy_read(uint8_t *out, size_t n) {
    while (n) {
        struct sha256 s; uint8_t blk[32]; uint64_t t = rdtsc(), r = 0; int hr = have_rdrand && rdrand64(&r);
        counter++;
        sha256_init(&s); sha256_update(&s, pool, 32); sha256_update(&s, &counter, 8); sha256_update(&s, &t, 8);
        if (hr) sha256_update(&s, &r, 8);
        sha256_final(&s, blk);
        entropy_add(blk, 8);                                  /* ratchet the pool so outputs can't be replayed from it */
        size_t k = n < 32 ? n : 32; memcpy(out, blk, k); out += k; n -= k;
    }
}
