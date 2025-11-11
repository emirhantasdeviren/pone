#ifndef PONE_RECT_H
#define PONE_RECT_H

#include "pone_vec2.h"

struct PoneRect {
    Vec2 p_min;
    Vec2 p_max;
};

inline f32 pone_rect_width(PoneRect *rect) {
    return rect->p_max.x - rect->p_min.x;
}

inline f32 pone_rect_height(PoneRect *rect) {
    return rect->p_max.y - rect->p_min.y;
}


#endif
