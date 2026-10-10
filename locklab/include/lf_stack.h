#ifndef LF_STACK_H
#define LF_STACK_H

#include <stdatomic.h>

#define STACK_MAX_THREADS 16
#define STACK_RETIRED 256

typedef struct StackNode {
    int value;
    struct StackNode *next;
} StackNode;

typedef struct {
    _Atomic(StackNode *) head;
} LFStack;

void stack_init(LFStack *stack);
void stack_push(LFStack *stack, int value);
int stack_pop(LFStack *stack, int *value);

void stack_thread_cleanup(void);
void stack_cleanup(void);
void stack_destroy(LFStack *stack);

#endif