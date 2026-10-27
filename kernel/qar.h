#ifndef QAR_QAR_H
#define QAR_QAR_H
#include <stdint.h>
#include <stddef.h>

#define QAR_OK            0
#define QAR_E_FORMAT     -1   /* bad magic/size/bounds */
#define QAR_E_HASH       -2   /* content or file hash mismatch */
#define QAR_E_UNTRUSTED  -3   /* signer not in trust store */
#define QAR_E_SIGNATURE  -4
#define QAR_E_MANIFEST   -5

struct qar_info {
    char id[64], name[64], version[32], entry[64], caps[160];
    const uint8_t *entry_data; uint64_t entry_size;
};
/* Full verification (spec section "Verification order"). On success fills *out. */
int qar_verify(const uint8_t *blob, uint64_t len, const uint8_t (*trusted)[32], int ntrusted, struct qar_info *out);
const char *qar_strerror(int err);
#endif
