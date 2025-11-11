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
