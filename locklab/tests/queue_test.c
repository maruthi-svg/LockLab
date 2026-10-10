#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include "ms_queue.h"

#define THREADS 4
#define ITEMS 1000
#define TOTAL (THREADS * ITEMS)

MSQueue queue;
_Atomic int seen[TOTAL];
_Atomic int popped = 0;

void *producer(void *arg)
{
    long id = (long)arg;

    for (int i = 0; i < ITEMS; i++) {
        int value = (int)(id * ITEMS + i);
        queue_enqueue(&queue, value);
    }

    queue_thread_cleanup();
    return NULL;
}

void *consumer(void *arg)
{
    (void)arg;

    while (atomic_load(&popped) < TOTAL) {
        int value;

        if (queue_dequeue(&queue, &value)) {
            if (value < 0 || value >= TOTAL)
                continue;

            int old = atomic_exchange(&seen[value], 1);

            if (old != 0) {
                printf("ERROR: duplicate value %d\n", value);
                continue;
            }

            atomic_fetch_add(&popped, 1);
        }
    }

    queue_thread_cleanup();
    return NULL;
}

int main(void)
{
    pthread_t producers[THREADS];
    pthread_t consumers[THREADS];

    queue_init(&queue);

    for (int i = 0; i < TOTAL; i++)
        atomic_init(&seen[i], 0);

    printf("\n");
    printf("============================================================\n");
    printf("              MICHAEL-SCOTT QUEUE STRESS TEST\n");
    printf("============================================================\n");
    printf("Producers       : %d\n", THREADS);
    printf("Consumers       : %d\n", THREADS);
    printf("Items/producer  : %d\n", ITEMS);
    printf("Expected items  : %d\n", TOTAL);
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

    queue_thread_cleanup();
    queue_destroy(&queue);

    int errors = 0;

    for (int i = 0; i < TOTAL; i++) {
        if (atomic_load(&seen[i]) != 1)
            errors++;
    }

    printf("Items dequeued  : %d\n", atomic_load(&popped));
    printf("Missing/invalid : %d\n", errors);

    if (errors == 0 &&
        atomic_load(&popped) == TOTAL)
        printf("Status          : PASS\n");
    else
        printf("Status          : FAIL\n");

    printf("============================================================\n\n");

    return errors != 0;
}