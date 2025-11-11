#include "pone_atomic.h"
#include "windows.h"

struct PoneSemaphore {
    volatile u32 val;
};

void pone_semaphore_init(PoneSemaphore *sem, u32 val) { sem->val = val; }

void pone_semaphore_signal(PoneSemaphore *sem) {
    _pone_atomic_fetch_add(&sem->val, 1, PONE_MEMORY_ORDERING_RELEASE);
    WakeByAddressSingle((void *)&sem->val);
}

void pone_semaphore_wait(PoneSemaphore *sem) {
    u32 curr_val = sem->val;

    for (;;) {
        if (curr_val) {
            if (_pone_atomic_compare_exchange_n(&sem->val, &curr_val,
                                                curr_val - 1, 0,
                                                PONE_MEMORY_ORDERING_ACQUIRE,
                                                PONE_MEMORY_ORDERING_RELAXED)) {
                break;
            }
        } else {
            WaitOnAddress((volatile void *)&sem->val, (void *)&curr_val,
                          sizeof(u32), INFINITE);
            curr_val = sem->val;
        }
    }
}

struct PoneMutex {
    volatile b8 flag;
};

void pone_mutex_init(PoneMutex *mutex) { mutex->flag = 0; }

void pone_mutex_lock(PoneMutex *mutex) {
    b8 curr_flag = mutex->flag;
    for (;;) {
        if (!curr_flag) {
            if (_pone_atomic_compare_exchange_n(&mutex->flag, &curr_flag, 1, 0,
                                                PONE_MEMORY_ORDERING_ACQUIRE,
                                                PONE_MEMORY_ORDERING_RELAXED)) {
                break;
            }
        } else {
            WaitOnAddress((volatile void *)&mutex->flag, (void *)&curr_flag,
                          sizeof(b8), INFINITE);
            curr_flag = mutex->flag;
        }
    }
}

void pone_mutex_unlock(PoneMutex *mutex) {
    _pone_atomic_clear(&mutex->flag, PONE_MEMORY_ORDERING_RELEASE);
    WakeByAddressSingle((void *)&mutex->flag);
}
