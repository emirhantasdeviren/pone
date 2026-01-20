#ifndef PONE_MEMORY_H
#define PONE_MEMORY_H

#include "pone_types.h"

#define KILOBYTES(n) (n << 10)
#define MEGABYTES(n) (KILOBYTES(n) << 10)
#define GIGABYTES(n) (MEGABYTES(n) << 10)
#define TERABYTES(n) (GIGABYTES(n) << 10)

#define pone_array_count(arr) sizeof((arr)) / sizeof((arr)[0])

void *pone_memcpy(void *dst, void *src, usize len);
void pone_memset(void *p, u8 c, usize n);

#endif
