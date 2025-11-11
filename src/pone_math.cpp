#include "pone_math.h"
#include <math.h>

b8 pone_f32_sign_bit(f32 x) {
    union {
        f32 f;
        u32 u;
    } value;
    value.f = x;

    return (b8)(value.u >> 31);
}

b8 pone_is_sign_negative(f32 x) {
    union {
        f32 f;
        u32 u;
    } value;
    value.f = x;
    return (value.u & 0x80000000) != 0;
}

b8 pone_is_sign_positive(f32 x) {
    return !pone_is_sign_negative(x);
}

b8 pone_f32_is_nan(f32 x) {
    return x != x;
}

f32 pone_f32_copysign(f32 x, f32 y) {
    union {
        f32 f;
        u32 u;
    } value_x;
    union {
        f32 f;
        u32 u;
    } value_y;

    value_x.f = x;
    value_y.f = y;

    value_x.u = (value_x.u & 0x7FFFFFFF) | (value_y.u & 0x80000000);
    
    return value_x.f;
}

f32 pone_f32_signum(f32 x) {
    if (pone_f32_is_nan(x)) {
        return x;
    } else {
        return pone_f32_copysign(1.0f, x);
    }
}

f32 pone_abs(f32 x) {
    union {
        f32 f;
        u32 u;
    } value;

    value.f = x;
    value.u &= 0x7FFFFFFF;

    return value.f;
}

f32 pone_ceil(f32 x) {
    return ceilf(x);
}

f32 pone_floor(f32 x) {
    return floorf(x);
}

f32 pone_sqrt(f32 x) {
    return sqrtf(x);
}

f32 pone_cbrt(f32 x) {
    return cbrtf(x);
}

f32 pone_sin(f32 x) {
    return sinf(x);
}

f32 pone_cos(f32 x) {
    return cosf(x);
}

f32 pone_acos(f32 x) {
    return acosf(x);
}
