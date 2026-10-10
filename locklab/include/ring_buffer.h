#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdatomic.h>
#include <stddef.h>

#define RB_CAPACITY 1024

typedef struct {
    _Atomic size_t sequence;
    int data;
} RBSlot;

typedef struct {
    RBSlot slots[RB_CAPACITY];
    _Atomic size_t enqueue_pos;
    _Atomic size_t dequeue_pos;
} MPMCRing;

void ring_init(MPMCRing *rb);
int ring_enqueue(MPMCRing *rb, int value);
int ring_dequeue(MPMCRing *rb, int *value);

#endif
