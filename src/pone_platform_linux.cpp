#include "pone_arena.h"
#include "pone_assert.h"
#include "pone_platform.h"
#include "pone_memory.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

void pone_platform_get_system_info(PonePlatformSystemInfo *info) {
    info->page_size = (usize)getpagesize();
}

void *pone_platform_allocate_memory(void *addr, usize size) {
    void *p = mmap(addr, size, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (p == MAP_FAILED) {
        return 0;
    } else {
        return p;
    }
}

void pone_platform_deallocate_memory(void *p) {}

u64 pone_platform_get_time(void) {
    struct timespec ts;
    int ret = clock_gettime(CLOCK_MONOTONIC, &ts);
    pone_assert(ret == 0);

    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}

void pone_platform_read_file(PoneString *path, usize *size, void *data,
                             Arena *arena) {
    PoneArenaTmp *tmp_arena = pone_arena_tmp_begin(arena);
    char *path_c_str = arena_alloc_array(tmp_arena->arena, path->len + 1, char);
    pone_memcpy((void *)path_c_str, (void *)path->buf, path->len);
    path_c_str[path->len] = '\0';

    if (!data) {
        struct stat statbuf;
        int ret = stat(path_c_str, &statbuf);
        pone_assert(ret == 0);

        *size = statbuf.st_size;

        pone_arena_tmp_end(tmp_arena);
        return;
    }

    int fd = open(path_c_str, O_RDONLY);
    pone_assert(fd != -1);

    ssize_t n = read(fd, data, *size);
    pone_assert(n == *size);

    pone_arena_tmp_end(tmp_arena);
}
