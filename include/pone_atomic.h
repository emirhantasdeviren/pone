#ifndef PONE_ATOMIC_H
#define PONE_ATOMIC_H

#include "pone_types.h"

#define _pone_atomic_fetch_add(ptr, val, mem_order)                            \
    __atomic_fetch_add(ptr, val, (int)mem_order)
#define _pone_atomic_fetch_sub(ptr, val, mem_order)                            \
    __atomic_fetch_sub(ptr, val, (int)mem_order)
#define _pone_atomic_load(ptr, ret, mem_order)                                 \
    __atomic_load(ptr, ret, (int)mem_order)
#define _pone_atomic_load_n(ptr, mem_order) __atomic_load_n(ptr, (int)mem_order)
#define _pone_atomic_store(ptr, val, mem_order)                                \
    __atomic_store(ptr, val, (int)mem_order)
#define _pone_atomic_store_n(ptr, val, mem_order)                              \
    __atomic_store_n(ptr, val, (int)mem_order)
#define _pone_atomic_compare_exchange_n(ptr, expected, desired, weak,          \
                                        success_memorder, failure_memorder)    \
    __atomic_compare_exchange_n(ptr, expected, desired, weak,                  \
                                (int)success_memorder, (int)failure_memorder)
#define _pone_atomic_test_and_set(ptr, memorder)                               \
    __atomic_test_and_set(ptr, memorder)
#define _pone_atomic_clear(ptr, memorder) __atomic_clear(ptr, (int)memorder)

enum PoneMemoryOrdering {
    PONE_MEMORY_ORDERING_RELAXED,
    PONE_MEMORY_ORDERING_CONSUME,
    PONE_MEMORY_ORDERING_ACQUIRE,
    PONE_MEMORY_ORDERING_RELEASE,
    PONE_MEMORY_ORDERING_ACQ_REL,
    PONE_MEMORY_ORDERING_SEQ_CST,
};

struct PoneSemaphore;

void pone_semaphore_init(PoneSemaphore *sem, u32 val);
void pone_semaphore_signal(PoneSemaphore *sem);
void pone_semaphore_wait(PoneSemaphore *sem);

struct PoneMutex;

void pone_mutex_init(PoneMutex *mutex);
void pone_mutex_lock(PoneMutex *mutex);
void pone_mutex_unlock(PoneMutex *mutex);

#endif
