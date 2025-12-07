#include "pone_platform.h"

#include <unistd.h>
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
