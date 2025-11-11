#include "pone_arena.h"

#include "pone_assert.h"
#include "pone_memory.h"
#include <stdio.h>

static usize get_page_size() {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    return (usize)system_info.dwPageSize;
}

void arena_init(Arena *arena, void *base, usize capacity) {
    arena->base = base;
    arena->offset = 0;
    arena->capacity = capacity;
}

Arena *arena_create(usize capacity) {
    usize page_size = get_page_size();
    capacity += sizeof(Arena);
    capacity += (page_size - 1) & -capacity;

    Arena *arena = (Arena *)VirtualAlloc(0, capacity, MEM_RESERVE | MEM_COMMIT,
                                         PAGE_READWRITE);
    pone_assert(arena);
    usize align = alignof(max_align_t);
    usize base = (usize)arena + sizeof(Arena);
    base += (align - 1) & -base;
    usize end = (usize)arena + capacity;

    arena->base = (void *)base;
    arena->offset = 0;
    arena->capacity = end - base;
    return arena;
}

static void *arena_alloc_align(Arena *arena, usize size, usize align) {
    PONE_ASSERT(arena);
    if (size == 0) {
        return 0;
    }

    usize header_addr = (usize)arena->base + arena->offset;
    header_addr += (align - 1) & -header_addr;
    usize addr = header_addr + sizeof(AllocationHeader);
    addr += (align - 1) & -addr;
    usize next_offset = addr + size - (usize)arena->base;

    PONE_ASSERT(next_offset <= arena->capacity);
    *(AllocationHeader *)header_addr = {
        .size = size,
    };
    arena->offset = next_offset;

    return (void *)addr;
}

void *arena_alloc(Arena *arena, usize size) {
    return arena_alloc_align(arena, size, alignof(max_align_t));
}

static void *arena_realloc_align(Arena *arena, void *p, usize size,
                                 usize align) {
    if (size == 0) {
        return 0;
    }

    if (!p) {
        return arena_alloc(arena, size);
    }

    usize header_addr = (usize)p - sizeof(AllocationHeader);
    header_addr &= ~(align - 1);
    AllocationHeader *header = (AllocationHeader *)header_addr;
    usize old_size = header->size;

    if ((usize)p + old_size == (usize)arena->base + arena->offset) {
        header->size = size;
        arena->offset = (usize)p + size - (usize)arena->base;

        return p;
    }

    if (old_size >= size) {
        return p;
    }

    void *p_new = arena_alloc(arena, size);
    if (p_new) {
        pone_memcpy(p_new, p, old_size);
    }

    return p_new;
}

void *arena_realloc(Arena *arena, void *p, usize size) {
    return arena_realloc_align(arena, p, size, alignof(max_align_t));
}

void arena_clear(Arena *arena) { arena->offset = 0; }

void arena_destroy(Arena **arena) {
    if (arena) {
        VirtualFree(*arena, 0, MEM_RELEASE);
        *arena = 0;
    }
}

i32 arena_vsprintf(Arena *arena, char **s, const char *fmt, va_list args) {
    PONE_ASSERT(arena && s && fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int n = vsnprintf(0, 0, fmt, args_copy);
    va_end(args_copy);

    if (n < 0) {
        return n;
    }
    *s = (char *)arena_alloc(arena, n + 1);
    n = vsnprintf(*s, n + 1, fmt, args);

    return n;
}

i32 arena_sprintf(Arena *arena, char **s, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    i32 n = arena_vsprintf(arena, s, fmt, args);
    va_end(args);

    return n;
}
