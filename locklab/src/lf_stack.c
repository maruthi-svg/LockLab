#include "lf_stack.h"

#include <pthread.h>
#include <stdlib.h>

/*
 * Hazard pointers.
 *
 * Each participating thread gets one hazard slot.
 * A thread publishes the node it is about to dereference.
 */
static _Atomic(StackNode *) hazards[STACK_MAX_THREADS];
static _Atomic int registered = 0;

/*
 * Each thread keeps a small local retired list.
 */
typedef struct {
    StackNode *nodes[STACK_RETIRED];
    int count;
} RetiredList;

static _Thread_local int hazard_id = -1;
static _Thread_local RetiredList retired = {0};

/*
 * Global retired list.
 *
 * This allows retired nodes to survive the lifetime of
 * the worker thread that originally removed them.
 */
#define STACK_GLOBAL_RETIRED \
    (STACK_MAX_THREADS * STACK_RETIRED)

static StackNode *global_retired[STACK_GLOBAL_RETIRED];
static int global_retired_count = 0;
static pthread_mutex_t retired_lock = PTHREAD_MUTEX_INITIALIZER;


/*
 * Register the current thread for hazard-pointer use.
 */
static int get_hazard_id(void)
{
    if (hazard_id >= 0)
        return hazard_id;

    int id = atomic_fetch_add_explicit(
        &registered,
        1,
        memory_order_relaxed);

    if (id >= STACK_MAX_THREADS)
        abort();

    hazard_id = id;

    atomic_store_explicit(
        &hazards[id],
        NULL,
        memory_order_release);

    return id;
}


/*
 * Publish a hazard pointer.
 */
static void protect(StackNode *node)
{
    int id = get_hazard_id();

    atomic_store_explicit(
        &hazards[id],
        node,
        memory_order_seq_cst);
}


/*
 * Clear the current thread's hazard pointer.
 */
static void clear_hazard(void)
{
    if (hazard_id >= 0) {
        atomic_store_explicit(
            &hazards[hazard_id],
            NULL,
            memory_order_seq_cst);
    }
}


/*
 * Check whether any thread currently protects node.
 */
static int is_hazard(StackNode *node)
{
    int count = atomic_load_explicit(
        &registered,
        memory_order_acquire);

    for (int i = 0; i < count; i++) {
        if (atomic_load_explicit(
                &hazards[i],
                memory_order_seq_cst) == node) {
            return 1;
        }
    }

    return 0;
}


/*
 * Scan the calling thread's retired list.
 */
static void scan_local_retired(void)
{
    int write = 0;

    for (int i = 0; i < retired.count; i++) {
        StackNode *node = retired.nodes[i];

        if (is_hazard(node)) {
            retired.nodes[write++] = node;
        } else {
            free(node);
        }
    }

    retired.count = write;
}


/*
 * Scan the global retired list.
 *
 * The caller must hold retired_lock.
 */
static void scan_global_retired_locked(void)
{
    int write = 0;

    for (int i = 0; i < global_retired_count; i++) {
        StackNode *node = global_retired[i];

        if (is_hazard(node)) {
            global_retired[write++] = node;
        } else {
            free(node);
        }
    }

    global_retired_count = write;
}


/*
 * Move a local retired node into the global retired list.
 */
static void transfer_retired_to_global(void)
{
    if (retired.count == 0)
        return;

    pthread_mutex_lock(&retired_lock);

    /*
     * Reclaim anything that is already safe.
     */
    scan_global_retired_locked();

    for (int i = 0; i < retired.count; i++) {

        /*
         * If the global list becomes full, leave the remaining
         * nodes in the local list. They will be transferred later.
         */
        if (global_retired_count >= STACK_GLOBAL_RETIRED)
            break;

        global_retired[global_retired_count++] =
            retired.nodes[i];
    }

    /*
     * Remove the nodes that were successfully transferred.
     */
    int transferred = 0;

    while (transferred < retired.count &&
           global_retired_count <= STACK_GLOBAL_RETIRED) {

        /*
         * Determine whether this node is already present
         * in the global prefix that was transferred.
         */
        int found = 0;

        for (int j = 0; j < global_retired_count; j++) {
            if (global_retired[j] ==
                retired.nodes[transferred]) {
                found = 1;
                break;
            }
        }

        if (!found)
            break;

        transferred++;
    }

    /*
     * Compact local list.
     */
    int remaining = 0;

    for (int i = transferred; i < retired.count; i++)
        retired.nodes[remaining++] = retired.nodes[i];

    retired.count = remaining;

    pthread_mutex_unlock(&retired_lock);
}


/*
 * Retire a node removed from the stack.
 */
static void retire_node(StackNode *node)
{
    if (retired.count >= STACK_RETIRED)
        scan_local_retired();

    /*
     * If the local list is still full, move its contents
     * to the global list.
     */
    if (retired.count >= STACK_RETIRED)
        transfer_retired_to_global();

    /*
     * If there is still room, retain locally.
     */
    if (retired.count < STACK_RETIRED) {
        retired.nodes[retired.count++] = node;
    } else {
        /*
         * Extremely defensive fallback:
         * transfer again and retain if possible.
         */
        transfer_retired_to_global();

        if (retired.count < STACK_RETIRED) {
            retired.nodes[retired.count++] = node;
        } else {
            /*
             * The global list is completely saturated.
             * Do not free an unprotected node blindly.
             * Keep it by aborting rather than risking UAF.
             */
            abort();
        }
    }

    /*
     * Periodically attempt reclamation.
     */
    if (retired.count >= 32)
        scan_local_retired();
}


/*
 * Called by a worker before it exits.
 */
void stack_thread_cleanup(void)
{
    /*
     * First reclaim locally safe nodes.
     */
    for (int pass = 0;
         pass < 8 && retired.count > 0;
         pass++) {

        scan_local_retired();

        if (retired.count == 0)
            break;
    }

    /*
     * Transfer anything still retired to the global list.
     */
    transfer_retired_to_global();

    /*
     * Clear the thread's hazard state so this pthread can
     * safely register again if it is reused.
     */
    clear_hazard();
    hazard_id = -1;
}


/*
 * Initialize the stack.
 */
void stack_init(LFStack *stack)
{
    atomic_init(&stack->head, NULL);

    for (int i = 0; i < STACK_MAX_THREADS; i++)
        atomic_store_explicit(
            &hazards[i],
            NULL,
            memory_order_relaxed);

    atomic_store_explicit(
        &registered,
        0,
        memory_order_relaxed);

    pthread_mutex_lock(&retired_lock);

    global_retired_count = 0;

    pthread_mutex_unlock(&retired_lock);
}


/*
 * Treiber stack push.
 */
void stack_push(LFStack *stack, int value)
{
    StackNode *node = malloc(sizeof(StackNode));

    if (!node)
        abort();

    node->value = value;

    StackNode *old;

    do {
        old = atomic_load_explicit(
            &stack->head,
            memory_order_acquire);

        node->next = old;

    } while (!atomic_compare_exchange_weak_explicit(
        &stack->head,
        &old,
        node,
        memory_order_release,
        memory_order_relaxed));
}


/*
 * Treiber stack pop with hazard protection.
 */
int stack_pop(LFStack *stack, int *value)
{
    StackNode *old;
    StackNode *next;

    while (1) {

        old = atomic_load_explicit(
            &stack->head,
            memory_order_acquire);

        if (old == NULL)
            return 0;

        /*
         * Protect the node before dereferencing it.
         */
        protect(old);

        /*
         * Revalidate the head after publishing the hazard.
         */
        if (old != atomic_load_explicit(
                &stack->head,
                memory_order_acquire)) {

            clear_hazard();
            continue;
        }

        next = old->next;

        /*
         * Remove the node using CAS.
         */
        if (atomic_compare_exchange_weak_explicit(
                &stack->head,
                &old,
                next,
                memory_order_acquire,
                memory_order_relaxed)) {

            *value = old->value;

            clear_hazard();

            retire_node(old);

            return 1;
        }

        clear_hazard();
    }
}


/*
 * Reclaim globally retired nodes.
 */
void stack_cleanup(void)
{
    pthread_mutex_lock(&retired_lock);

    scan_global_retired_locked();

    pthread_mutex_unlock(&retired_lock);
}


/*
 * Destroy the stack.
 *
 * Must only be called after all worker threads have stopped.
 */
void stack_destroy(LFStack *stack)
{
    /*
     * Reclaim anything belonging to the calling thread.
     */
    stack_thread_cleanup();

    /*
     * No worker may still be using the stack at this point.
     * Therefore every remaining node is safe to free.
     */
    StackNode *node = atomic_load_explicit(
        &stack->head,
        memory_order_relaxed);

    while (node != NULL) {
        StackNode *next = node->next;
        free(node);
        node = next;
    }

    atomic_store_explicit(
        &stack->head,
        NULL,
        memory_order_relaxed);

    /*
     * Reclaim global retired nodes.
     */
    pthread_mutex_lock(&retired_lock);

    for (int i = 0; i < global_retired_count; i++)
        free(global_retired[i]);

    global_retired_count = 0;

    pthread_mutex_unlock(&retired_lock);
}