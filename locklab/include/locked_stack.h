#ifndef LOCKED_STACK_H
#define LOCKED_STACK_H

#include <pthread.h>

typedef struct LockedNode {
    int value;
    struct LockedNode *next;
} LockedNode;

typedef struct {
    LockedNode *head;
    pthread_mutex_t mutex;
} LockedStack;

void locked_stack_init(LockedStack *stack);
void locked_stack_push(LockedStack *stack, int value);
int locked_stack_pop(LockedStack *stack, int *value);
void locked_stack_destroy(LockedStack *stack);

#endif
