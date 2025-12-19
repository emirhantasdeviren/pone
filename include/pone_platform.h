#ifndef PONE_PLATFORM_H
#define PONE_PLATFORM_H

#if defined(_WIN64)
#define PONE_PLATFORM_WIN64
#elif defined(__linux__) && defined(__x86_64__)
#define PONE_PLATFORM_LINUX
#endif

#include "pone_string.h"
#include "pone_arena.h"
#include "pone_types.h"

struct PonePlatformSystemInfo {
    usize page_size;
};

void pone_platform_get_system_info(PonePlatformSystemInfo *info);
void *pone_platform_allocate_memory(void *addr, usize size);
void pone_platform_deallocate_memory(void *p);
u64 pone_platform_get_time(void);
void pone_platform_read_file(PoneString *path, usize *size, void *data,
                             Arena *arena);

#endif
