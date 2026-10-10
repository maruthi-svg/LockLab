#ifndef MS_QUEUE_H
#define MS_QUEUE_H

#include <stdatomic.h>

#define QUEUE_MAX_THREADS 16
#define QUEUE_RETIRED 256

typedef struct QNode {
    int value;
    _Atomic(struct QNode *) next;
} QNode;

typedef struct {
    _Atomic(QNode *) head;
    _Atomic(QNode *) tail;
} MSQueue;

void queue_init(MSQueue *q);
void queue_enqueue(MSQueue *q, int value);
int queue_dequeue(MSQueue *q, int *value);

#endif

void queue_thread_cleanup(void);
void queue_destroy(MSQueue *q);
