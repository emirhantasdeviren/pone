#include "pone_vec2.h"
#include "pone_math.h"

Vec2 pone_vec2_sub(Vec2 lhs, Vec2 rhs) {
    return (Vec2){
        .x = lhs.x - rhs.x,
        .y = lhs.y - rhs.y,
    };
}

Vec2 pone_vec2_add(Vec2 lhs, Vec2 rhs) {
    return (Vec2){
        .x = lhs.x + rhs.x,
        .y = lhs.y + rhs.y,
    };
}

Vec2 pone_vec2_mul_scalar(f32 n, Vec2 v) {
    return (Vec2){
        .x = v.x * n,
        .y = v.y * n,
    };
}

Vec2 pone_vec2_neg(Vec2 v) {
    return (Vec2){
        .x = -v.x,
        .y = -v.y,
    };
};

f32 pone_vec2_dot(Vec2 lhs, Vec2 rhs) {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y);
}

f32 pone_vec2_perp_dot(Vec2 lhs, Vec2 rhs) {
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

f32 pone_vec2_len_squared(Vec2 v) {
    return (v.x * v.x) + (v.y * v.y);
}

Vec2 pone_vec2_norm(Vec2 v) {
    f32 len = pone_sqrt(pone_vec2_len_squared(v));

    return pone_vec2_mul_scalar(1.0f / len, v);
}
