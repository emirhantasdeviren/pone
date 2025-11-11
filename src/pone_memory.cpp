#include "pone_memory.h"

void *pone_memcpy(void *dst, void *src, usize len) {
    u8 *d = (u8 *)dst;
    u8 *s = (u8 *)src;
    for (; len; len--) {
        *d++ = *s++;
    }

    return dst;
}

void pone_memset(void *p, u8 c, usize n) {
    for (usize i = 0; i < n; ++i) {
        u8 *b = (u8 *)p + i;
        *b = c;
    }
}
