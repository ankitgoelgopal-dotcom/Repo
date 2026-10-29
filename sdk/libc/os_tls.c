#include <stdlib.h>
#include <string.h>
#include "os_api.h"
#include "os_tls.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

extern const unsigned char qar_ca_bundle_pem[];   /* generated at SDK build time from the build host's CA bundle */
extern const unsigned qar_ca_bundle_len;

struct os_tls {
    int fd;
    mbedtls_ssl_context ssl; mbedtls_ssl_config conf;
    mbedtls_entropy_context ent; mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt ca;
};

static int bio_send(void *ctx, const unsigned char *buf, size_t len) {
    long n = os_send(*(int *)ctx, buf, len); return n > 0 ? (int)n : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
static int bio_recv(void *ctx, unsigned char *buf, size_t len) {
    long n = os_recv(*(int *)ctx, buf, len); return n >= 0 ? (int)n : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

void os_tls_close(struct os_tls *t) {
    if (!t) return;
    mbedtls_ssl_close_notify(&t->ssl);
    mbedtls_ssl_free(&t->ssl); mbedtls_ssl_config_free(&t->conf); mbedtls_ctr_drbg_free(&t->drbg);
    mbedtls_entropy_free(&t->ent); mbedtls_x509_crt_free(&t->ca); free(t);
}

struct os_tls *os_tls_connect(int fd, const char *host, const char *extra_ca_pem, unsigned flags, int *err) {
    int e = 0; if (!err) err = &e;
    struct os_tls *t = calloc(1, sizeof *t);
    if (!t) { *err = MBEDTLS_ERR_SSL_ALLOC_FAILED; return 0; }
    t->fd = fd;
    mbedtls_ssl_init(&t->ssl); mbedtls_ssl_config_init(&t->conf); mbedtls_ctr_drbg_init(&t->drbg);
    mbedtls_entropy_init(&t->ent); mbedtls_x509_crt_init(&t->ca);
    int r;
    if ((r = mbedtls_ctr_drbg_seed(&t->drbg, mbedtls_entropy_func, &t->ent, (const unsigned char *)"qar-tls", 7)) != 0) goto fail;
    if ((r = mbedtls_ssl_config_defaults(&t->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) goto fail;
    mbedtls_ssl_conf_min_tls_version(&t->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_authmode(&t->conf, MBEDTLS_SSL_VERIFY_REQUIRED);          /* never skip certificate verification */
    if (!(flags & OS_TLS_NO_DEFAULT_ROOTS))
        mbedtls_x509_crt_parse(&t->ca, qar_ca_bundle_pem, qar_ca_bundle_len);  /* >0 = some roots use unsupported algorithms: fine */
    if (extra_ca_pem && (r = mbedtls_x509_crt_parse(&t->ca, (const unsigned char *)extra_ca_pem, strlen(extra_ca_pem) + 1)) < 0) goto fail;
    mbedtls_ssl_conf_ca_chain(&t->conf, &t->ca, 0);
    mbedtls_ssl_conf_rng(&t->conf, mbedtls_ctr_drbg_random, &t->drbg);
    if ((r = mbedtls_ssl_setup(&t->ssl, &t->conf)) != 0) goto fail;
    if (host && (r = mbedtls_ssl_set_hostname(&t->ssl, host)) != 0) goto fail;   /* SNI + certificate name check */
    mbedtls_ssl_set_bio(&t->ssl, &t->fd, bio_send, bio_recv, 0);
    while ((r = mbedtls_ssl_handshake(&t->ssl)) != 0)
        if (r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) goto fail;
    return t;
fail:
    *err = r; os_tls_close(t); return 0;
}

long os_tls_write(struct os_tls *t, const void *buf, size_t n) {
    const unsigned char *p = buf; size_t done = 0;
    while (done < n) {
        int r = mbedtls_ssl_write(&t->ssl, p + done, n - done);
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (r <= 0) return -1;
        done += (size_t)r;
    }
    return (long)n;
}
long os_tls_read(struct os_tls *t, void *buf, size_t n) {
    for (;;) {
        int r = mbedtls_ssl_read(&t->ssl, buf, n);
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == 0) return 0;
        return r < 0 ? -1 : r;
    }
}
const char *os_tls_strerror(int err) {
    static char buf[160]; mbedtls_strerror(err, buf, sizeof buf); return buf;
}
const char *os_tls_info(struct os_tls *t) {
    static char buf[128]; const char *v = mbedtls_ssl_get_version(&t->ssl), *c = mbedtls_ssl_get_ciphersuite(&t->ssl);
    size_t i = 0; while (*v && i < 20) buf[i++] = *v++; buf[i++] = ' ';
    while (*c && i < sizeof buf - 1) buf[i++] = *c++; buf[i] = 0; return buf;
}
