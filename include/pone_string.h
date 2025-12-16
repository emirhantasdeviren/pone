#ifndef PONE_STRING_H
#define PONE_STRING_H

#include "pone_types.h"

struct PoneString {
    u8 *buf;
    usize len;
};

void pone_string_from_cstr(const char *s, PoneString *pone_s);

b8 pone_string_eq(PoneString s1, PoneString s2);
b8 pone_string_eq_c_str(const PoneString *s1, const char *s2);

#endif

                                  

                              
