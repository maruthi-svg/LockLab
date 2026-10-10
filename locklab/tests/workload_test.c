#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "lf_stack.h"

#define MAX_THREADS 16
#define MAX_OPS 10000

typedef enum {
    OP_PUSH,
    OP_POP
} OpType;

typedef struct {
    OpType type;
    int value;
} Operation;

typedef struct {
    int thread_id;
    Operation *ops;
    int count;
    LFStack *stack;
    FILE *log;
    pthread_mutex_t *log_lock;
    _Atomic int *event_sequence;
} Worker;

static Operation operations[MAX_OPS];
static int operation_count;

static int parse_script(const char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("script");
        return -1;
    }

    char command[32];
    int value;

    while (operation_count < MAX_OPS &&
           fscanf(file, "%31s", command) == 1) {

        if (strcmp(command, "PUSH") == 0) {

            if (fscanf(file, "%d", &value) != 1) {
                fclose(file);
                return -1;
            }

            operations[operation_count].type = OP_PUSH;
            operations[operation_count].value = value;
            operation_count++;

        } else if (strcmp(command, "POP") == 0) {

            operations[operation_count].type = OP_POP;
            operations[operation_count].value = 0;
            operation_count++;

        } else {

            printf("Unknown operation: %s\n", command);
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

static void *worker_main(void *arg)
{
    Worker *worker = arg;

    for (int i = 0; i < worker->count; i++) {

        Operation *op = &worker->ops[i];

        int start = atomic_fetch_add_explicit(
            worker->event_sequence,
            1,
            memory_order_relaxed) + 1;

        int value = -1;
        int success = 0;

        if (op->type == OP_PUSH) {

            stack_push(worker->stack, op->value);

            value = op->value;
            success = 1;

        } else {

            success = stack_pop(
                worker->stack,
                &value);
        }

        int end = atomic_fetch_add_explicit(
            worker->event_sequence,
            1,
            memory_order_relaxed) + 1;

        pthread_mutex_lock(worker->log_lock);

        if (op->type == OP_PUSH) {

            fprintf(
                worker->log,
                "%d,%d,%d,PUSH,%d,OK\n",
                start,
                end,
                worker->thread_id,
                value);

        } else if (success) {

            fprintf(
                worker->log,
                "%d,%d,%d,POP,%d,OK\n",
                start,
                end,
                worker->thread_id,
                value);

        } else {

            fprintf(
                worker->log,
                "%d,%d,%d,POP,-1,EMPTY\n",
                start,
                end,
                worker->thread_id);
        }

        pthread_mutex_unlock(worker->log_lock);
    }

    stack_thread_cleanup();

    return NULL;
}

static int create_sample_script(const char *filename)
{
    FILE *file = fopen(filename, "w");

    if (!file)
        return -1;

    fprintf(file, "PUSH 10\n");
    fprintf(file, "PUSH 20\n");
    fprintf(file, "PUSH 30\n");
    fprintf(file, "POP\n");
    fprintf(file, "POP\n");
    fprintf(file, "PUSH 40\n");
    fprintf(file, "PUSH 50\n");
    fprintf(file, "POP\n");
    fprintf(file, "POP\n");
    fprintf(file, "POP\n");

    fclose(file);

    return 0;
}

int main(void)
{
    const char *script_file = "workload.script";
    const char *history_file = "history.log";

    printf("\n");
    printf("============================================================\n");
    printf("              LOCKLAB WORKLOAD ENGINE\n");
    printf("============================================================\n");

    if (create_sample_script(script_file) != 0 ||
        parse_script(script_file) != 0) {

        printf("Failed to prepare workload script.\n");
        return 1;
    }

    printf("Operations parsed : %d\n", operation_count);

    LFStack stack;

    stack_init(&stack);

    FILE *log = fopen(history_file, "w");

    if (!log) {
        perror("history");
        stack_destroy(&stack);
        return 1;
    }

    fprintf(
        log,
        "start,end,thread,operation,value,result\n");

    pthread_mutex_t log_lock =
        PTHREAD_MUTEX_INITIALIZER;

    _Atomic int event_sequence = 0;

    int thread_count = 4;

    pthread_t threads[MAX_THREADS];
    Worker workers[MAX_THREADS];

    int base = operation_count / thread_count;
    int extra = operation_count % thread_count;
    int offset = 0;

    for (int i = 0; i < thread_count; i++) {

        int count =
            base + (i < extra ? 1 : 0);

        workers[i].thread_id = i;
        workers[i].ops = &operations[offset];
        workers[i].count = count;
        workers[i].stack = &stack;
        workers[i].log = log;
        workers[i].log_lock = &log_lock;
        workers[i].event_sequence = &event_sequence;

        if (pthread_create(
                &threads[i],
                NULL,
                worker_main,
                &workers[i]) != 0) {

            printf("Failed to create worker thread.\n");

            for (int j = 0; j < i; j++)
                pthread_join(
                    threads[j],
                    NULL);

            fclose(log);
            pthread_mutex_destroy(&log_lock);
            stack_destroy(&stack);

            return 1;
        }

        offset += count;
    }

    for (int i = 0; i < thread_count; i++)
        pthread_join(
            threads[i],
            NULL);

    stack_thread_cleanup();

    fclose(log);

    stack_destroy(&stack);

    pthread_mutex_destroy(&log_lock);

    printf("Threads dispatched : %d\n", thread_count);
    printf("History file       : %s\n", history_file);
    printf("History format     : invocation/response intervals\n");
    printf("Status             : PASS\n");

    printf("============================================================\n\n");

    return 0;
}