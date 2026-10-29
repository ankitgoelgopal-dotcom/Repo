/* HTTPS demo.
 *  1. TLS handshake with the local test server (10.0.2.2:8443), trusting only the embedded test CA.
 *  2. Same server again with the NORMAL root store: must be REJECTED (unknown CA) - proves verification is on.
 *  3. Real internet HTTPS to example.com (only if DNS/internet work). */
#include <stdio.h>
#include <string.h>
#include "os_api.h"
#include "os_tls.h"
#include "test_ca.h"

static int https_get(uint32_t ip, const char *host, const char *ca, unsigned flags, const char *label) {
    int fd = os_socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return -1;
    int rc = os_connect(fd, ip, label[0] == 'i' ? 443 : 8443);
    if (rc) { printf("tlsdemo[%s]: TCP connect failed (%d)\n", label, rc); os_close(fd); return -2; }
    int err = 0; struct os_tls *t = os_tls_connect(fd, host, ca, flags, &err);
    if (!t) { printf("tlsdemo[%s]: handshake REJECTED: %s\n", label, os_tls_strerror(err)); os_close(fd); return -3; }
    printf("tlsdemo[%s]: handshake OK, %s\n", label, os_tls_info(t));
    char req[160]; snprintf(req, sizeof req, "GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    os_tls_write(t, req, strlen(req));
    static char buf[2048]; long n, total = 0; char line[64] = "";
    while ((n = os_tls_read(t, buf, sizeof buf - 1)) > 0) {
        if (!total) { size_t i = 0; while (i < (size_t)n && i < 63 && buf[i] != '\r' && buf[i] != '\n') { line[i] = buf[i]; i++; } line[i] = 0; }
        total += n;
    }
    printf("tlsdemo[%s]: HTTPS status line: %s (%ld bytes)\n", label, line, total);
    os_tls_close(t); os_close(fd); return 0;
}

int main(void) {
    printf("tlsdemo: clock says unix time %lu\n", (unsigned long)os_time_unix());
    https_get(IP4(10, 0, 2, 2), "qar.test", test_ca_pem, OS_TLS_NO_DEFAULT_ROOTS, "local-trusted");
    https_get(IP4(10, 0, 2, 2), "qar.test", 0, 0, "local-untrusted");
    uint32_t ip;
    if (os_resolve("example.com", &ip) == 0) https_get(ip, "example.com", 0, 0, "internet");
    else puts("tlsdemo[internet]: skipped (no DNS/internet)");
    puts("tlsdemo: done");
    return 0;
}
