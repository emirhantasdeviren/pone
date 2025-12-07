#include "pone_string.h"

#include "pone_assert.h"
#include "pone_memory.h"

u8 *pone_strcpy(PoneString dst, PoneString src) {
    PONE_ASSERT(dst.len >= src.len);
    return (u8 *)pone_memcpy((void *)dst.buf, (void *)src.buf, src.len);
}

b8 pone_string_eq(PoneString s1, PoneString s2) {
    if (s1.len != s2.len) {
        return 0;
    }

    for (usize i = 0; i < s1.len; i++) {
        if (s1.buf[i] != s2.buf[i]) {
            return 0;
        }
    }

    return 1;
}

b8 pone_string_eq_c_str(const PoneString *s1, const char *s2) {
    for (usize i = 0; i < s1->len; i++) {
        if (s1->buf[i] != (u8)s2[i]) {
            return 0;
        }
    }

    return 1;
}

void pone_string_from_cstr(const char *s, PoneString *pone_s) {
    pone_assert(s);

    pone_s->buf = (u8 *)s;
    pone_s->len = 0;
    while (*s) {
        pone_s->len++;
        s++;
    }
}
