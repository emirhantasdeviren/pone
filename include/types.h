#if !defined(PONE_TYPES_H)
#define PONE_TYPES_H

#include <stdint.h>

#include "assert.h"

typedef int8_t      i8;
typedef int16_t    i16;
typedef int32_t    i32;
typedef int64_t    i64;
typedef intptr_t isize;

typedef uint8_t      u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef uintptr_t usize;

typedef u8 b8;

typedef float  f32;
typedef double f64;

#define U32_MAX UINT32_MAX

inline u32
safeTruncateU32(u64 value) {
    assert(value <= U32_MAX);
    u32 result = (u32)value;
    return result;
}

inline u32
saturatingSubU32(u32 value, u32 rhs) {
    if (value > rhs) {
        return value - rhs;
    } else {
        return 0;
    }
}

#endif
