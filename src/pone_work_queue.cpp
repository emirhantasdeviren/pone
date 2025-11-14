#include "pone_work_queue.h"
#include "pone_atomic.h"

#define PONE_WORK_QUEUE_CAPACITY 256

void pone_work_queue_init(PoneWorkQueue *queue, Arena *arena) {
    queue->buffer =
        arena_alloc_array(arena, PONE_WORK_QUEUE_CAPACITY, PoneWorkQueueCell);
    queue->buffer_mask = PONE_WORK_QUEUE_CAPACITY - 1;
    for (usize i = 0; i < PONE_WORK_QUEUE_CAPACITY; ++i) {
        _pone_atomic_store_n(&queue->buffer[i].sequence, i,
                             PONE_MEMORY_ORDERING_RELAXED);
    }
    _pone_atomic_store_n(&queue->enqueue_pos, 0, PONE_MEMORY_ORDERING_RELAXED);
    _pone_atomic_store_n(&queue->dequeue_pos, 0, PONE_MEMORY_ORDERING_RELAXED);
}

b8 pone_work_queue_enqueue(PoneWorkQueue *queue, PoneWorkQueueData *data) {
    PoneWorkQueueCell *cell;
    usize pos =
        _pone_atomic_load_n(&queue->enqueue_pos, PONE_MEMORY_ORDERING_RELAXED);

    for (;;) {
        cell = &queue->buffer[pos & queue->buffer_mask];
        usize seq =
            _pone_atomic_load_n(&cell->sequence, PONE_MEMORY_ORDERING_ACQUIRE);
        isize diff = (isize)seq - (isize)pos;

        if (diff == 0) {
            if (_pone_atomic_compare_exchange_n(&queue->enqueue_pos, &pos,
                                                pos + 1, 0,
                                                PONE_MEMORY_ORDERING_RELAXED,
                                                PONE_MEMORY_ORDERING_RELAXED)) {
                break;
            }
        } else if (diff < 0) {
            return 0;
        } else {
            pos = _pone_atomic_load_n(&queue->enqueue_pos,
                                      PONE_MEMORY_ORDERING_RELAXED);
        }
    }

    cell->data = *data;
    _pone_atomic_store_n(&cell->sequence, pos + 1,
                         PONE_MEMORY_ORDERING_RELEASE);

    return 1;
}

b8 pone_work_queue_dequeue(PoneWorkQueue *queue, PoneWorkQueueData *data) {
    PoneWorkQueueCell *cell;
    usize pos =
        _pone_atomic_load_n(&queue->dequeue_pos, PONE_MEMORY_ORDERING_RELAXED);

    for (;;) {
        cell = &queue->buffer[pos & queue->buffer_mask];
        usize seq =
            _pone_atomic_load_n(&cell->sequence, PONE_MEMORY_ORDERING_ACQUIRE);
        isize diff = (isize)seq - (isize)(pos + 1);

        if (diff == 0) {
            if (_pone_atomic_compare_exchange_n(&queue->dequeue_pos, &pos,
                                                pos + 1, 0,
                                                PONE_MEMORY_ORDERING_RELAXED,
                                                PONE_MEMORY_ORDERING_RELAXED)) {
                break;
            }
        } else if (diff < 0) {
            return 0;
        } else {
            pos = _pone_atomic_load_n(&queue->dequeue_pos,
                                      PONE_MEMORY_ORDERING_RELAXED);
        }
    }

    *data = cell->data;
    _pone_atomic_store_n(&cell->sequence, pos + queue->buffer_mask + 1,
                         PONE_MEMORY_ORDERING_RELEASE);

    return 1;
}

usize pone_work_queue_length(PoneWorkQueue *queue) {
    return _pone_atomic_load_n(&queue->enqueue_pos,
                               PONE_MEMORY_ORDERING_RELAXED) -
           _pone_atomic_load_n(&queue->dequeue_pos,
                               PONE_MEMORY_ORDERING_RELAXED);
}
