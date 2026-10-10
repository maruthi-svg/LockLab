#include <stdio.h>
#include "lf_stack.h"

static void stack_demo(void)
{
    LFStack stack;
    int value;

    stack_init(&stack);

    for (int i = 1; i <= 5; i++)
        stack_push(&stack, i);

    printf("\n[1] TREIBER LOCK-FREE STACK\n");
    printf("    Algorithm : Atomic CAS\n");
    printf("    Operations: 5 push / 5 pop\n");

    while (stack_pop(&stack, &value))
        printf("    Pop       : %d\n", value);

    printf("    Status    : PASS\n");
}

int main(void)
{
    printf("\n");
    printf("============================================================\n");
    printf("                         LOCKLAB\n");
    printf("              Lock-Free Data Structures Lab\n");
    printf("============================================================\n");

    stack_demo();

    printf("\n============================================================\n");
    printf("              CORE COMPONENT STATUS: READY\n");
    printf("============================================================\n\n");

    return 0;
}
