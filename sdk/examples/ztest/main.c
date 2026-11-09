/* zlib on QAR-OS: round trips at several levels, gzip decoding (what web servers send), crc32/adler32 vectors. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#include "ztest_data.h"          /* generated at build time by Python's zlib: ref_data, ref_gzip, expected checksums */

static int pass, fail;
#define CHECK(c, name) do { if (c) pass++; else { fail++; printf("Z FAIL %s\n", name); } } while (0)

int main(void) {
    CHECK(crc32(0, (const Bytef *)"hello world", 11) == 0x0d4a1185UL, "crc32 vector");
    CHECK(adler32(1, (const Bytef *)"hello world", 11) == 0x1a0b045dUL, "adler32 vector");
    CHECK(crc32(0, ref_data, sizeof ref_data) == REF_CRC32, "crc32 of reference data");

    /* gzip -> raw bytes (windowBits 16 + MAX_WBITS selects the gzip wrapper) */
    uLongf outn = sizeof ref_data; Bytef *out = malloc(outn + 16);
    z_stream zs; memset(&zs, 0, sizeof zs);
    CHECK(inflateInit2(&zs, 16 + MAX_WBITS) == Z_OK, "inflateInit2 gzip");
    zs.next_in = (Bytef *)ref_gzip; zs.avail_in = sizeof ref_gzip; zs.next_out = out; zs.avail_out = outn + 16;
    int r = inflate(&zs, Z_FINISH);
    CHECK(r == Z_STREAM_END, "inflate gzip to end");
    CHECK(zs.total_out == sizeof ref_data && !memcmp(out, ref_data, sizeof ref_data), "gunzip matches original");
    inflateEnd(&zs);

    /* deflate at every level, then inflate and compare */
    for (int level = 0; level <= 9; level++) {
        uLongf cn = compressBound(sizeof ref_data); Bytef *c = malloc(cn);
        CHECK(compress2(c, &cn, ref_data, sizeof ref_data, level) == Z_OK, "compress2");
        uLongf dn = sizeof ref_data; Bytef *d = malloc(dn);
        CHECK(uncompress(d, &dn, c, cn) == Z_OK && dn == sizeof ref_data && !memcmp(d, ref_data, dn), "round trip");
        if (level == 0 || level == 6 || level == 9) printf("Z level %d: %lu -> %lu bytes\n", level, (unsigned long)sizeof ref_data, (unsigned long)cn);
        if (level == 6) CHECK(cn < sizeof ref_data / 2, "level 6 actually compresses");
        free(c); free(d);
    }
    /* corrupt input must be rejected, not crash */
    uLongf bad = sizeof ref_data; Bytef *junk = malloc(bad);
    uint8_t corrupt[64]; memcpy(corrupt, ref_gzip, sizeof corrupt); corrupt[20] ^= 0xFF;
    memset(&zs, 0, sizeof zs); inflateInit2(&zs, 16 + MAX_WBITS);
    zs.next_in = corrupt; zs.avail_in = sizeof corrupt; zs.next_out = junk; zs.avail_out = (uInt)bad;
    CHECK(inflate(&zs, Z_FINISH) != Z_STREAM_END, "corrupt gzip rejected");
    inflateEnd(&zs);
    printf("ZTEST DONE pass=%d fail=%d zlib=%s\n", pass, fail, zlibVersion());
    return fail ? 1 : 0;
}
