#include "ring_buffer.h"
#include <stdint.h>

void ring_init(MPMCRing *rb)
{
    for (size_t i = 0; i < RB_CAPACITY; i++)
        atomic_init(&rb->slots[i].sequence, i);

    atomic_init(&rb->enqueue_pos, 0);
    atomic_init(&rb->dequeue_pos, 0);
}

int ring_enqueue(MPMCRing *rb, int value)
{
    size_t pos = atomic_load_explicit(
        &rb->enqueue_pos,
        memory_order_relaxed);

    for (;;) {
        RBSlot *slot = &rb->slots[pos & (RB_CAPACITY - 1)];

        size_t seq = atomic_load_explicit(
            &slot->sequence,
            memory_order_acquire);

        intptr_t dif = (intptr_t)seq - (intptr_t)pos;

        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &rb->enqueue_pos,
                    &pos,
                    pos + 1,
                    memory_order_relaxed,
                    memory_order_relaxed))
                break;
        } else if (dif < 0) {
            return 0;
        } else {
            pos = atomic_load_explicit(
                &rb->enqueue_pos,
                memory_order_relaxed);
        }
    }

    RBSlot *slot = &rb->slots[pos & (RB_CAPACITY - 1)];

    slot->data = value;

    atomic_store_explicit(
        &slot->sequence,
        pos + 1,
        memory_order_release);

    return 1;
}

int ring_dequeue(MPMCRing *rb, int *value)
{
    size_t pos = atomic_load_explicit(
        &rb->dequeue_pos,
        memory_order_relaxed);

    for (;;) {
        RBSlot *slot = &rb->slots[pos & (RB_CAPACITY - 1)];

        size_t seq = atomic_load_explicit(
            &slot->sequence,
            memory_order_acquire);

        intptr_t dif =
            (intptr_t)seq - (intptr_t)(pos + 1);

        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &rb->dequeue_pos,
                    &pos,
                    pos + 1,
                    memory_order_relaxed,
                    memory_order_relaxed))
                break;
        } else if (dif < 0) {
            return 0;
        } else {
            pos = atomic_load_explicit(
                &rb->dequeue_pos,
                memory_order_relaxed);
        }
    }

    RBSlot *slot = &rb->slots[pos & (RB_CAPACITY - 1)];

    *value = slot->data;

    atomic_store_explicit(
        &slot->sequence,
        pos + RB_CAPACITY,
        memory_order_release);

    return 1;
}
