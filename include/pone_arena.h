#ifndef PONE_ARENA_H
#define PONE_ARENA_H

#include "pone_types.h"
#include <stdarg.h>

struct Arena {
    void *base;
    usize offset;
    usize capacity;
};

struct AllocationHeader {
    usize size;
};

usize get_page_size();

void arena_init(Arena *arena, void *base, usize capacity);
Arena *arena_create(usize capacity);
void *arena_alloc(Arena *arena, usize size);
void *arena_realloc(Arena *arena, void *p, usize size);
void arena_clear(Arena *arena);
void arena_destroy(Arena **arena);
i32 arena_vsprintf(Arena *arena, char **s, const char *fmt, va_list args);
i32 arena_sprintf(Arena *arena, char **s, const char *fmt, ...);
void pone_arena_create_sub_arena(Arena *arena, usize capacity,
                                 Arena *sub_arena);

#define arena_alloc_array(arena, count, type)                                  \
    (type *)arena_alloc(arena, (count) * sizeof(type))

#endif
