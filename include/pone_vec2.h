#ifndef PONE_VEC2_H
#define PONE_VEC2_H

#include "pone_types.h"

struct Vec2 {
    f32 x;
    f32 y;
};

Vec2 pone_vec2_sub(Vec2 lhs, Vec2 rhs);
Vec2 pone_vec2_add(Vec2 lhs, Vec2 rhs);
Vec2 pone_vec2_mul_scalar(f32 n, Vec2 v);
Vec2 pone_vec2_neg(Vec2 v);

f32 pone_vec2_dot(Vec2 lhs, Vec2 rhs);
f32 pone_vec2_perp_dot(Vec2 lhs, Vec2 rhs);

f32 pone_vec2_len_squared(Vec2 v);
Vec2 pone_vec2_norm(Vec2 v);

#endif
