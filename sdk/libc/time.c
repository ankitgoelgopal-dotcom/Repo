#include <time.h>
#include "os_api.h"
time_t time(time_t *t) { time_t v = (time_t)os_time_unix(); if (t) *t = v; return v; }
struct tm *gmtime_r(const time_t *tt, struct tm *o) {
    long long z = *tt / 86400, rem = *tt % 86400; if (rem < 0) { rem += 86400; z--; }
    o->tm_hour = (int)(rem / 3600); o->tm_min = (int)(rem % 3600 / 60); o->tm_sec = (int)(rem % 60);
    o->tm_wday = (int)((z + 4) % 7); if (o->tm_wday < 0) o->tm_wday += 7;
    z += 719468; long long era = (z >= 0 ? z : z - 146096) / 146097; unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; long long y = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1; unsigned m = mp < 10 ? mp + 3 : mp - 9; if (m <= 2) y++;
    o->tm_year = (int)(y - 1900); o->tm_mon = (int)m - 1; o->tm_mday = (int)d; o->tm_isdst = 0;
    static const int cum[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)); o->tm_yday = cum[m - 1] + (int)d - 1 + (leap && m > 2);
    return o;
}
