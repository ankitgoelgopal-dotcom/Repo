#ifndef QAR_RTC_H
#define QAR_RTC_H
#include <stdint.h>
#include <stddef.h>
uint64_t rtc_unix_time(void);        /* seconds since 1970-01-01 UTC, from the CMOS clock */
void entropy_init(void);
void entropy_add(const void *data, size_t n);
void entropy_event(void);
void entropy_read(uint8_t *out, size_t n);
#endif
