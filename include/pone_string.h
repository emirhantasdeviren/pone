#ifndef PONE_STRING_H
#define PONE_STRING_H

#include "pone_types.h"

struct PoneString {
    u8 *buf;
    usize len;
};

b8 pone_string_eq(PoneString s1, PoneString s2);

#endif
