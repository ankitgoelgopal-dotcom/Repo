/* CMOS real-time clock -> Unix time (needed to check TLS certificate validity dates). */
#include "kernel.h"
#include "rtc.h"
static uint8_t cmos(uint8_t r) { outb(0x70, r); return inb(0x71); }
static uint8_t bcd(uint8_t v) { return (uint8_t)((v & 0x0F) + (v >> 4) * 10); }
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {   /* Howard Hinnant's algorithm */
    y -= m <= 2; int64_t era = (y >= 0 ? y : y - 399) / 400; unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}
uint64_t rtc_unix_time(void) {
    #define RET(x) return (uint64_t)(x)
    uint8_t s, mi, h, d, mo, y, c, s2, mi2, h2, d2, mo2, y2;
    do {                                                   /* re-read until two reads agree (update-in-progress safe) */
        while (cmos(0x0A) & 0x80) ;
        s = cmos(0); mi = cmos(2); h = cmos(4); d = cmos(7); mo = cmos(8); y = cmos(9);
        while (cmos(0x0A) & 0x80) ;
        s2 = cmos(0); mi2 = cmos(2); h2 = cmos(4); d2 = cmos(7); mo2 = cmos(8); y2 = cmos(9);
    } while (s != s2 || mi != mi2 || h != h2 || d != d2 || mo != mo2 || y != y2);
    uint8_t b = cmos(0x0B); c = cmos(0x32);
    int pm = h & 0x80; h &= 0x7F;
    if (!(b & 4)) { s = bcd(s); mi = bcd(mi); h = bcd(h); d = bcd(d); mo = bcd(mo); y = bcd(y); if (c) c = bcd(c); }
    if (!(b & 2) && pm) h = (uint8_t)((h + 12) % 24);
    int year = (c ? c * 100 : 2000) + y;
    RET(days_from_civil(year, mo, d) * 86400 + h * 3600 + mi * 60 + s);
}
