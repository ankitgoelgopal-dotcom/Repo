#ifndef QAR_TIME_H
#define QAR_TIME_H
typedef long time_t;
typedef long clock_t;
typedef int clockid_t;
#define CLOCKS_PER_SEC 1000000L
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
struct timespec { time_t tv_sec; long tv_nsec; };
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; long tm_gmtoff; const char *tm_zone; };
time_t time(time_t *t);
struct tm *gmtime_r(const time_t *t, struct tm *out);
struct tm *gmtime(const time_t *t);
struct tm *localtime_r(const time_t *t, struct tm *out);   /* QAR-OS has no time zones yet: local time is UTC */
struct tm *localtime(const time_t *t);
time_t timegm(struct tm *tm);
time_t mktime(struct tm *tm);
int clock_gettime(clockid_t id, struct timespec *ts);
clock_t clock(void);
int nanosleep(const struct timespec *req, struct timespec *rem);
#endif
