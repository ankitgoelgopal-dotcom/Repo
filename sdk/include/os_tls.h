/* HTTPS / TLS 1.2 client for QAR-OS apps (Mbed TLS 3.6). Certificates are always verified:
 * chain to a trusted root, validity dates (needs a correct RTC), and the host name.
 *
 *   int fd = os_socket(AF_INET, SOCK_STREAM, 0); os_connect(fd, ip, 443);
 *   int err; struct os_tls *t = os_tls_connect(fd, "example.com", NULL, 0, &err);
 *   if (!t) printf("TLS failed: %s\n", os_tls_strerror(err));
 *   os_tls_write(t, req, len); n = os_tls_read(t, buf, sizeof buf); os_tls_close(t); os_close(fd);
 *
 * Not supported: TLS 1.3 (servers that only offer 1.3 will fail), client certificates, session resumption.
 * Needs the "net.connect" capability (it uses sockets) and ~1 MiB of heap during the handshake. */
#ifndef QAR_OS_TLS_H
#define QAR_OS_TLS_H
#include <stddef.h>

struct os_tls;
#define OS_TLS_NO_DEFAULT_ROOTS 1   /* trust ONLY extra_ca_pem (private PKI / tests) */

/* extra_ca_pem: optional NUL-terminated PEM with additional trusted CA certificate(s). Returns NULL on failure (*err set). */
struct os_tls *os_tls_connect(int fd, const char *host, const char *extra_ca_pem, unsigned flags, int *err);
long os_tls_write(struct os_tls *t, const void *buf, size_t n);   /* all bytes or -1 */
long os_tls_read(struct os_tls *t, void *buf, size_t n);          /* bytes, 0 = closed by peer, -1 = error */
void os_tls_close(struct os_tls *t);                              /* sends close_notify and frees; does not close the socket */
const char *os_tls_strerror(int err);
const char *os_tls_info(struct os_tls *t);                        /* e.g. "TLSv1.2 TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256" */
#endif
