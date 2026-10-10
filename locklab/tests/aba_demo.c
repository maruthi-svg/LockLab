#include <stdio.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct Node {
    int value;
} Node;

static _Atomic(Node *) naive_head;
static _Atomic(uintptr_t) tagged_head;

static uintptr_t pack(Node *ptr, unsigned version)
{
    return ((uintptr_t)ptr & ~7ULL) | (version & 7U);
}

static unsigned unpack_version(uintptr_t value)
{
    return (unsigned)(value & 7U);
}

static void naive_demo(void)
{
    Node a = {1};
    Node b = {2};

    atomic_store(&naive_head, &a);

    Node *observed = atomic_load(&naive_head);

    printf("\nNAIVE ABA DEMO\n");
    printf("[Thread A] Read head = A\n");

    atomic_store(&naive_head, &b);
    printf("[Thread B] Changed A -> B\n");

    atomic_store(&naive_head, &a);
    printf("[Thread B] Changed B -> A\n");

    Node *expected = observed;

    if (atomic_compare_exchange_strong(
            &naive_head,
            &expected,
            &b)) {

        printf("[Thread A] CAS succeeded after ABA!\n");
        printf("ABA PROBLEM DEMONSTRATED\n");
    }
}

static void tagged_demo(void)
{
    Node a = {1};
    Node b = {2};

    atomic_store(&tagged_head, pack(&a, 0));

    uintptr_t observed = atomic_load(&tagged_head);

    printf("\nTAGGED POINTER ABA FIX\n");
    printf("[Thread A] Read head = A\n");
    printf("[Thread A] Version = %u\n",
           unpack_version(observed));

    atomic_store(&tagged_head, pack(&b, 1));
    printf("[Thread B] Changed A -> B\n");

    atomic_store(&tagged_head, pack(&a, 2));
    printf("[Thread B] Changed B -> A\n");
    printf("[Thread B] Version is now 2\n");

    uintptr_t expected = observed;

    if (!atomic_compare_exchange_strong(
            &tagged_head,
            &expected,
            pack(&b, 3))) {

        printf("[Thread A] CAS rejected!\n");
        printf("[Thread A] Pointer returned to A, but version changed.\n");
        printf("[Thread A] ABA DETECTED\n");
        printf("ABA PROTECTION ACTIVE\n");
    }
}

int main(void)
{
    printf("\n============================================================\n");
    printf("                    ABA DEMONSTRATION\n");
    printf("============================================================\n");

    naive_demo();
    tagged_demo();

    printf("\n============================================================\n\n");

    return 0;
}