/* In-OS .qar verifier. Portable: also compiled on the host by tests/host_test.sh. */
#include "qar.h"
#include "sha256.h"
#include "mem.h"
#include "../third_party/ed25519/src/ed25519.h"

#define HDR_SIZE 128
#define SIG_SIZE 128
#define HASH_OFF 96
static const uint8_t MAGIC[8] = {0x89,'Q','A','R','\r','\n',0x1a,'\n'};

static uint16_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (uint32_t)rd16(p+2) << 16; }
static uint64_t rd64(const uint8_t *p) { return rd32(p) | (uint64_t)rd32(p+4) << 32; }

/* tiny extractor for string values in the canonical JSON manifest */
static int json_str(const uint8_t *m, uint64_t n, const char *key, char *out, size_t cap) {
    size_t kl = 0; while (key[kl]) kl++;
    for (uint64_t i = 0; i + kl + 4 < n; i++) {
        if (m[i] == '"' && !memcmp(m + i + 1, key, kl) && m[i+1+kl] == '"' && m[i+2+kl] == ':' && m[i+3+kl] == '"') {
            uint64_t j = i + 4 + kl; size_t o = 0;
            while (j < n && m[j] != '"' && o + 1 < cap) out[o++] = m[j++];
            out[o] = 0; return 0;
        }
    }
    return -1;
}
static void json_caps(const uint8_t *m, uint64_t n, char *out, size_t cap) {
    static const char k[] = "\"capabilities\":[";
    out[0] = 0;
    for (uint64_t i = 0; i + sizeof k < n; i++) if (!memcmp(m + i, k, sizeof k - 1)) {
        uint64_t j = i + sizeof k - 1; size_t o = 0;
        while (j < n && m[j] != ']' && o + 1 < cap) out[o++] = m[j++];
        out[o] = 0; return;
    }
}

int qar_verify(const uint8_t *b, uint64_t len, const uint8_t (*trusted)[32], int nt, struct qar_info *out) {
    if (len < HDR_SIZE + SIG_SIZE || memcmp(b, MAGIC, 8)) return QAR_E_FORMAT;
    if (rd16(b + 8) != 1 || rd32(b + 16) != HDR_SIZE) return QAR_E_FORMAT;
    uint64_t m_off = rd64(b+20), m_len = rd64(b+28), t_off = rd64(b+36), t_len = rd64(b+44);
    uint64_t p_off = rd64(b+52), p_len = rd64(b+60), s_off = rd64(b+68), s_len = rd64(b+76), total = rd64(b+84);
    if (total != len || s_len != SIG_SIZE || s_off + s_len != total) return QAR_E_FORMAT;
    if (m_off < HDR_SIZE || m_off + m_len < m_off || m_off + m_len > t_off || t_off + t_len < t_off ||
        t_off + t_len > p_off || p_off + p_len < p_off || p_off + p_len > s_off) return QAR_E_FORMAT;

    /* content hash over [0, s_off) with the hash field zeroed */
    struct sha256 sh; uint8_t calc[32], zero[32] = {0};
    sha256_init(&sh); sha256_update(&sh, b, HASH_OFF); sha256_update(&sh, zero, 32);
    sha256_update(&sh, b + HDR_SIZE, s_off - HDR_SIZE); sha256_final(&sh, calc);
    if (memcmp(calc, b + HASH_OFF, 32)) return QAR_E_HASH;

    const uint8_t *kid = b + s_off, *pub = kid + 32, *sig = pub + 32;
    uint8_t kid_calc[32]; sha256_buf(pub, 32, kid_calc);
    if (memcmp(kid_calc, kid, 32)) return QAR_E_SIGNATURE;
    int trust = 0;
    for (int i = 0; i < nt; i++) if (!memcmp(trusted[i], pub, 32)) trust = 1;
    if (!trust) return QAR_E_UNTRUSTED;
    uint8_t msg[64]; memcpy(msg, calc, 32); memcpy(msg + 32, kid, 32);
    if (!ed25519_verify(sig, msg, 64, pub)) return QAR_E_SIGNATURE;

    /* manifest */
    const uint8_t *man = b + m_off;
    memset(out, 0, sizeof *out);
    if (json_str(man, m_len, "id", out->id, sizeof out->id) || json_str(man, m_len, "entry", out->entry, sizeof out->entry))
        return QAR_E_MANIFEST;
    json_str(man, m_len, "name", out->name, sizeof out->name);
    json_str(man, m_len, "version", out->version, sizeof out->version);
    json_caps(man, m_len, out->caps, sizeof out->caps);

    /* TOC: verify every file hash, find the exec entry */
    uint64_t pos = t_off, end = t_off + t_len; int found = 0;
    while (pos + 56 <= end) {
        uint16_t pl = rd16(b + pos); uint8_t kind = b[pos + 2];
        uint64_t off = rd64(b + pos + 8), size = rd64(b + pos + 16);
        if (!pl) break;
        if (pos + 56 + pl > end || off < p_off || off + size < off || off + size > p_off + p_len) return QAR_E_FORMAT;
        uint8_t fh[32]; sha256_buf(b + off, size, fh);
        if (memcmp(fh, b + pos + 24, 32)) return QAR_E_HASH;
        if (kind == 1 && pl < sizeof out->entry && !memcmp(b + pos + 56, out->entry, pl) && out->entry[pl] == 0) {
            out->entry_data = b + off; out->entry_size = size; found = 1;
        }
        pos += 56 + pl;
    }
    return found ? QAR_OK : QAR_E_MANIFEST;
}

const char *qar_strerror(int e) {
    switch (e) {
    case QAR_OK: return "ok"; case QAR_E_FORMAT: return "malformed package";
    case QAR_E_HASH: return "hash mismatch (modified)"; case QAR_E_UNTRUSTED: return "untrusted signer";
    case QAR_E_SIGNATURE: return "bad signature"; case QAR_E_MANIFEST: return "bad manifest";
    default: return "unknown error"; }
}
