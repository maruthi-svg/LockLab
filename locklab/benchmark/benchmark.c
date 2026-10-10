#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include "lf_stack.h"
#include "locked_stack.h"

#define OPS 20000

static LFStack lf_stack;
static LockedStack mutex_stack;

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void *lf_worker(void *arg)
{
    long id = (long)arg;

    for (int i = 0; i < OPS; i++) {
        stack_push(&lf_stack, (int)(id * OPS + i));

        int value;
        while (!stack_pop(&lf_stack, &value))
            ;
    }

    return NULL;
}

static void *mutex_worker(void *arg)
{
    long id = (long)arg;

    for (int i = 0; i < OPS; i++) {
        locked_stack_push(
            &mutex_stack,
            (int)(id * OPS + i));

        int value;
        while (!locked_stack_pop(
            &mutex_stack, &value))
            ;
    }

    return NULL;
}

static double run_lf(int threads)
{
    pthread_t t[threads];

    stack_init(&lf_stack);

    double start = now();

    for (long i = 0; i < threads; i++)
        pthread_create(&t[i], NULL, lf_worker, (void *)i);

    for (int i = 0; i < threads; i++)
        pthread_join(t[i], NULL);

    return now() - start;
}

static double run_mutex(int threads)
{
    pthread_t t[threads];

    locked_stack_init(&mutex_stack);

    double start = now();

    for (long i = 0; i < threads; i++)
        pthread_create(&t[i], NULL, mutex_worker, (void *)i);

    for (int i = 0; i < threads; i++)
        pthread_join(t[i], NULL);

    double elapsed = now() - start;

    locked_stack_destroy(&mutex_stack);

    return elapsed;
}

int main(void)
{
    int thread_counts[] = {1, 2, 4, 8, 16};
    int cases = sizeof(thread_counts) /
                sizeof(thread_counts[0]);

    printf("\n");
    printf("===============================================================\n");
    printf("                LOCKLAB CONTENTION MATRIX\n");
    printf("===============================================================\n");
    printf("Operations/thread : %d push + %d pop\n", OPS, OPS);
    printf("---------------------------------------------------------------\n");
    printf("%-8s %-15s %-15s %-12s\n",
           "Threads",
           "Lock-Free",
           "Mutex",
           "Winner");
    printf("---------------------------------------------------------------\n");

    int lockfree_wins = 0;

    for (int i = 0; i < cases; i++) {
        int threads = thread_counts[i];

        double lf_time = run_lf(threads);
        double mutex_time = run_mutex(threads);

        int total = threads * OPS * 2;

        double lf_tp = total / lf_time;
        double mutex_tp = total / mutex_time;

        const char *winner;

        if (lf_tp > mutex_tp) {
            winner = "LOCK-FREE";
            lockfree_wins++;
        } else {
            winner = "MUTEX";
        }

        printf("%-8d %-15.2f %-15.2f %-12s\n",
               threads,
               lf_tp,
               mutex_tp,
               winner);
    }

    printf("---------------------------------------------------------------\n");
    printf("Lock-free winning regimes : %d / %d\n",
           lockfree_wins, cases);

    if (lockfree_wins > 0)
        printf("Acceptance condition       : PASS\n");
    else
        printf("Acceptance condition       : NOT MET ON THIS MACHINE\n");

    printf("===============================================================\n\n");

    return 0;
}
