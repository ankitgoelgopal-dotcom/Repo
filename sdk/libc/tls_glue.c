/* Platform glue that Mbed TLS needs on QAR-OS. */
#include <string.h>
#include <time.h>
#include "os_api.h"
#include "mbedtls/entropy.h"
#include "mbedtls/platform_time.h"

int mbedtls_hardware_poll(void *data, unsigned char *out, size_t len, size_t *olen) {
    (void)data; size_t done = 0;
    while (done < len) {
        size_t k = len - done > 256 ? 256 : len - done;
        if (os_getrandom(out + done, k) != (long)k) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        done += k;
    }
    *olen = len; return 0;
}
struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt, struct tm *buf) { return gmtime_r(tt, buf); }
mbedtls_ms_time_t mbedtls_ms_time(void) { return (mbedtls_ms_time_t)os_uptime_ms(); }

/* x509 host-name matching asks for inet_pton(); only IPv4 is supported here. */
int inet_pton(int af, const char *s, void *dst) {
    if (af != 2) return 0;
    unsigned char *o = dst; int part = 0; unsigned v = 0, digits = 0;
    for (;; s++) {
        if (*s >= '0' && *s <= '9') { v = v * 10 + (unsigned)(*s - '0'); if (++digits > 3 || v > 255) return 0; }
        else if (*s == '.' || *s == 0) { if (!digits || part > 3) return 0; o[part++] = (unsigned char)v; v = digits = 0; if (!*s) break; }
        else return 0;
    }
    return part == 4;
}
