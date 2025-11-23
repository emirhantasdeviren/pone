#ifndef PONE_MATH_H
#define PONE_MATH_H

#include "pone_types.h"

#define PONE_EPSILON 1.19209290e-07f
#define PONE_PI 3.14159265358979323846264338327950288f
#define PONE_F32_MAX  3.40282347e+38f
#define PONE_F32_MIN -3.40282347e+38f

#define PONE_MAX(x, y) (x) < (y) ? (y) : (x)
#define PONE_MIN(x, y) (x) < (y) ? (x) : (y)
#define PONE_CLAMP(v, lo, hi) (v)<(lo) ? (lo) : (v)>(hi) ? (hi) : (v)

b8 pone_f32_sign_bit(f32 x);
b8 pone_is_sign_negative(f32 x);
b8 pone_is_sign_positive(f32 x);
b8 pone_f32_is_nan(f32 x);

f32 pone_f32_copysign(f32 x, f32 y);
f32 pone_f32_signum(f32 x);

f32 pone_abs(f32 x);
f32 pone_ceil(f32 x);
f32 pone_floor(f32 x);

f32 pone_sqrt(f32 x);
f32 pone_cbrt(f32 x);

f32 pone_sin(f32 x);
f32 pone_cos(f32 x);
f32 pone_acos(f32 x);

#endif
