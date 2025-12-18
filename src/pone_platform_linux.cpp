#include "pone_platform.h"
#include "pone_assert.h"

#include <unistd.h>
#include <time.h>
#include <sys/mman.h>

void pone_platform_get_system_info(PonePlatformSystemInfo *info) {
    info->page_size = (usize)getpagesize();
}

void *pone_platform_allocate_memory(void *addr, usize size) {
    void *p = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

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
