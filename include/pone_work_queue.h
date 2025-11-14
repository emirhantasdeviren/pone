#ifndef PONE_WORK_QUEUE_H
#define PONE_WORK_QUEUE_H

#include "pone_types.h"
#include "pone_arena.h"

typedef void (*PoneWorkFn)(void *); 

struct PoneWorkQueueData {
    PoneWorkFn work;
    void *user_data;
};

struct PoneWorkQueueCell {
    volatile usize sequence;
    PoneWorkQueueData data;
};

struct PoneWorkQueue {
    PoneWorkQueueCell *buffer;
    usize buffer_mask;
    volatile usize enqueue_pos;
    volatile usize dequeue_pos;
};


void pone_work_queue_init(PoneWorkQueue *queue, Arena *arena);
b8 pone_work_queue_enqueue(PoneWorkQueue *queue, PoneWorkQueueData *data);
b8 pone_work_queue_dequeue(PoneWorkQueue *queue, PoneWorkQueueData *data);
usize pone_work_queue_length(PoneWorkQueue *queue);

#endif
