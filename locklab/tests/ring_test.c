#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include "ring_buffer.h"

#define THREADS 4
#define ITEMS 1000
#define TOTAL (THREADS * ITEMS)

MPMCRing ring;
_Atomic int seen[TOTAL];
_Atomic int consumed = 0;

void *producer(void *arg)
{
    long id = (long)arg;

    for (int i = 0; i < ITEMS; i++) {
        int value = (int)(id * ITEMS + i);

        while (!ring_enqueue(&ring, value))
            ;
    }

    return NULL;
}

void *consumer(void *arg)
{
    (void)arg;

    while (atomic_load_explicit(
               &consumed,
               memory_order_acquire) < TOTAL) {

        int value;

        if (!ring_dequeue(&ring, &value))
            continue;

        if (value >= 0 && value < TOTAL) {
            int old = atomic_exchange_explicit(
                &seen[value],
                1,
                memory_order_acq_rel);

            if (old == 0)
                atomic_fetch_add_explicit(
                    &consumed,
                    1,
                    memory_order_acq_rel);
        }
    }

    return NULL;
}

int main(void)
{
    pthread_t producers[THREADS];
    pthread_t consumers[THREADS];

    ring_init(&ring);

    for (int i = 0; i < TOTAL; i++)
        atomic_init(&seen[i], 0);

    printf("\n");
    printf("============================================================\n");
    printf("             LOCK-FREE BOUNDED MPMC RING BUFFER\n");
    printf("============================================================\n");
    printf("Producers       : %d\n", THREADS);
    printf("Consumers       : %d\n", THREADS);
    printf("Items/producer  : %d\n", ITEMS);
    printf("Expected items  : %d\n", TOTAL);
    printf("Capacity        : %d\n", RB_CAPACITY);
    printf("------------------------------------------------------------\n");

    for (long i = 0; i < THREADS; i++)
        pthread_create(
            &producers[i],
            NULL,
            producer,
            (void *)i);

    for (int i = 0; i < THREADS; i++)
        pthread_create(
            &consumers[i],
            NULL,
            consumer,
            NULL);

    for (int i = 0; i < THREADS; i++)
        pthread_join(producers[i], NULL);

    for (int i = 0; i < THREADS; i++)
        pthread_join(consumers[i], NULL);

    int errors = 0;

    for (int i = 0; i < TOTAL; i++)
        if (atomic_load(&seen[i]) != 1)
            errors++;

    printf("Consumed        : %d\n",
           atomic_load(&consumed));
    printf("Missing         : %d\n", errors);

    if (errors == 0 &&
        atomic_load(&consumed) == TOTAL)
        printf("Status          : PASS\n");
    else
        printf("Status          : FAIL\n");

    printf("============================================================\n\n");

    return errors != 0;
}
