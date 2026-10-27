/* Host harness: ./host_verify pkg.qar trusted.pub  -> prints result of the in-OS verifier code */
#include <stdio.h>
#include <stdlib.h>
#include "../kernel/qar.h"
#include "../kernel/sha256.h"
static uint8_t *slurp(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); if (!f) exit(2); fseek(f,0,SEEK_END); *n = ftell(f); rewind(f);
    uint8_t *b = malloc(*n); if (fread(b,1,*n,f) != *n) exit(2); fclose(f); return b; }
int main(int argc, char **argv) {
    if (argc == 2 && argv[1][0] == '-') { /* sha256 of stdin-free vectors */
        uint8_t o[32]; sha256_buf("abc", 3, o); for (int i=0;i<32;i++) printf("%02x", o[i]); printf("\n"); return 0; }
    size_t n, kn; uint8_t *pkg = slurp(argv[1], &n); uint8_t *k = slurp(argv[2], &kn);
    uint8_t keys[1][32]; for (int i=0;i<32;i++) keys[0][i] = k[i];
    struct qar_info info; int r = qar_verify(pkg, n, (const uint8_t (*)[32])keys, 1, &info);
    if (r) { printf("REJECT: %s\n", qar_strerror(r)); return 1; }
    printf("ACCEPT: id=%s entry=%s size=%lu caps=%s\n", info.id, info.entry, (unsigned long)info.entry_size, info.caps);
    return 0;
}
