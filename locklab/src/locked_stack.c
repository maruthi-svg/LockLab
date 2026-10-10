#include "locked_stack.h"
#include <stdlib.h>

void locked_stack_init(LockedStack *stack)
{
    stack->head = NULL;
    pthread_mutex_init(&stack->mutex, NULL);
}

void locked_stack_push(LockedStack *stack, int value)
{
    LockedNode *node = malloc(sizeof(LockedNode));

    if (!node)
        abort();

    node->value = value;

    pthread_mutex_lock(&stack->mutex);

    node->next = stack->head;
    stack->head = node;

    pthread_mutex_unlock(&stack->mutex);
}

int locked_stack_pop(LockedStack *stack, int *value)
{
    pthread_mutex_lock(&stack->mutex);

    LockedNode *node = stack->head;

    if (!node) {
        pthread_mutex_unlock(&stack->mutex);
        return 0;
    }

    stack->head = node->next;
    *value = node->value;

    pthread_mutex_unlock(&stack->mutex);

    free(node);
    return 1;
}

void locked_stack_destroy(LockedStack *stack)
{
    LockedNode *node = stack->head;

    while (node) {
        LockedNode *next = node->next;
        free(node);
        node = next;
    }

    pthread_mutex_destroy(&stack->mutex);
}
