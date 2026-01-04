#ifndef PONE_RECT_H
#define PONE_RECT_H

#include "pone_vec2.h"

struct PoneRectF32 {
    Vec2 p_min;
    Vec2 p_max;
};

inline f32 pone_rect_f32_width(PoneRectF32 *rect) {
    return rect->p_max.x - rect->p_min.x;
}

inline f32 pone_rect_f32_height(PoneRectF32 *rect) {
    return rect->p_max.y - rect->p_min.y;
}

struct PoneRectU32 {
    u32 x_min;
    u32 y_min;
    u32 x_max;
    u32 y_max;
};

inline u32 pone_rect_u32_width(PoneRectU32 *rect) {
    return rect->x_max - rect->x_min;
}

inline u32 pone_rect_u32_height(PoneRectU32 *rect) {
    return rect->y_max - rect->y_min;
}

#endif
