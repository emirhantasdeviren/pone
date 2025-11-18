#ifndef PONE_MAT2_H
#define PONE_MAT2_H

struct Mat2 {
    f32 data[4];
};

inline f32 pone_mat2_get(Mat2 *m, usize i, usize j) {
    return m->data[j * 2 + i];
}

inline void pone_mat2_set(Mat2 *m, usize i, usize j, f32 v) {
    m->data[j * 2 + i] = v;
}

#endif
