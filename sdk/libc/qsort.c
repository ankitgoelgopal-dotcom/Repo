#include <stdlib.h>
#include <string.h>
/* In-place heapsort: O(n log n) worst case, no recursion, no allocation. */
static void swap(char *a, char *b, size_t n) { while (n--) { char t = *a; *a++ = *b; *b++ = t; } }
static void sift(char *base, size_t n, size_t sz, size_t i, int (*cmp)(const void *, const void *)) {
    for (;;) {
        size_t l = 2 * i + 1, big = i;
        if (l < n && cmp(base + l * sz, base + big * sz) > 0) big = l;
        if (l + 1 < n && cmp(base + (l + 1) * sz, base + big * sz) > 0) big = l + 1;
        if (big == i) return;
        swap(base + i * sz, base + big * sz, sz); i = big;
    }
}
void qsort(void *b, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {
    char *base = b; if (n < 2 || !sz) return;
    for (size_t i = n / 2; i-- > 0;) sift(base, n, sz, i, cmp);
    for (size_t e = n - 1; e > 0; e--) { swap(base, base + e * sz, sz); sift(base, e, sz, 0, cmp); }
}
void *bsearch(const void *key, const void *b, size_t n, size_t sz, int (*cmp)(const void *, const void *)) {
    const char *base = b; size_t lo = 0, hi = n;
    while (lo < hi) { size_t mid = lo + (hi - lo) / 2; int c = cmp(key, base + mid * sz); if (!c) return (void *)(base + mid * sz); if (c < 0) hi = mid; else lo = mid + 1; }
    return 0;
}
