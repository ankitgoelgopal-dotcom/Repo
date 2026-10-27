#ifndef QAR_SHA256_H
#define QAR_SHA256_H
#include <stdint.h>
#include <stddef.h>
struct sha256 { uint32_t h[8]; uint8_t buf[64]; uint64_t len; size_t fill; };
void sha256_init(struct sha256 *s);
void sha256_update(struct sha256 *s, const void *data, size_t n);
void sha256_final(struct sha256 *s, uint8_t out[32]);
void sha256_buf(const void *data, size_t n, uint8_t out[32]);
#endif
