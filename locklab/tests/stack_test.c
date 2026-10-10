#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include "lf_stack.h"

#define THREADS 4
#define ITEMS 1000
#define TOTAL (THREADS * ITEMS)

LFStack stack;

_Atomic int seen[TOTAL];
_Atomic int popped = 0;

void *producer(void *arg)
{
    long id = (long)arg;

    for (int i = 0; i < ITEMS; i++) {
        int value = (int)(id * ITEMS + i);
        stack_push(&stack, value);
    }

    stack_thread_cleanup();

    return NULL;
}

void *consumer(void *arg)
{
    (void)arg;

    while (atomic_load(&popped) < TOTAL) {
        int value;

        if (stack_pop(&stack, &value)) {
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

    stack_thread_cleanup();

    return NULL;
}

int main(void)
{
    pthread_t producers[THREADS];
    pthread_t consumers[THREADS];

    stack_init(&stack);

    for (int i = 0; i < TOTAL; i++)
        atomic_init(&seen[i], 0);

    printf("\n");
    printf("============================================================\n");
    printf("                 TREIBER STACK STRESS TEST\n");
    printf("============================================================\n");
    printf("Producers       : %d\n", THREADS);
    printf("Consumers       : %d\n", THREADS);
    printf("Items/producer  : %d\n", ITEMS);
    printf("Expected items  : %d\n", TOTAL);
    printf("------------------------------------------------------------\n");

    for (long i = 0; i < THREADS; i++) {
        pthread_create(
            &producers[i],
            NULL,
            producer,
            (void *)i);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_create(
            &consumers[i],
            NULL,
            consumer,
            NULL);
    }

    for (int i = 0; i < THREADS; i++)
        pthread_join(producers[i], NULL);

    for (int i = 0; i < THREADS; i++)
        pthread_join(consumers[i], NULL);

    stack_thread_cleanup();
    stack_destroy(&stack);

    int errors = 0;

    for (int i = 0; i < TOTAL; i++) {
        if (atomic_load(&seen[i]) != 1)
            errors++;
    }

    printf("Popped items    : %d\n", atomic_load(&popped));
    printf("Missing items   : %d\n", errors);

    if (errors == 0 &&
        atomic_load(&popped) == TOTAL) {
        printf("Status          : PASS\n");
    } else {
        printf("Status          : FAIL\n");
    }

    printf("============================================================\n\n");

    return errors != 0;
}