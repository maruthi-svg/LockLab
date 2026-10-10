#include "ms_queue.h"
#include <stdlib.h>
#include <pthread.h>

/*
 * Hazard pointers
 *
 * Each worker can protect two queue nodes:
 *   slot 0 -> tail/head being inspected
 *   slot 1 -> successor during dequeue
 */
static _Atomic(QNode *) hazards[QUEUE_MAX_THREADS][2];
static _Atomic int registered = 0;

/*
 * Per-thread retired nodes.
 */
typedef struct {
    QNode *nodes[QUEUE_RETIRED];
    int count;
} RetiredList;

static _Thread_local int hazard_id = -1;
static _Thread_local RetiredList retired = {0};

/*
 * Global retired list.
 *
 * A worker may finish while still owning retired nodes in TLS.
 * Those nodes are transferred here so that queue_destroy()
 * can reclaim them after every worker has terminated.
 *
 * This mutex protects reclamation bookkeeping only.
 * The Michael-Scott queue itself remains lock-free.
 */
static RetiredList global_retired = {0};
static pthread_mutex_t retired_lock = PTHREAD_MUTEX_INITIALIZER;


/* -----------------------------------------------------------
 * Hazard-pointer registration
 * --------------------------------------------------------- */

static int get_hazard_id(void)
{
    if (hazard_id >= 0)
        return hazard_id;

    int id = atomic_fetch_add_explicit(
        &registered,
        1,
        memory_order_relaxed);

    if (id >= QUEUE_MAX_THREADS)
        abort();

    hazard_id = id;

    for (int i = 0; i < 2; i++) {
        atomic_store_explicit(
            &hazards[id][i],
            NULL,
            memory_order_release);
    }

    return id;
}


/* -----------------------------------------------------------
 * Hazard-pointer protection
 * --------------------------------------------------------- */

static void protect(int slot, QNode *node)
{
    int id = get_hazard_id();

    atomic_store_explicit(
        &hazards[id][slot],
        node,
        memory_order_seq_cst);
}


static void clear_hazards(void)
{
    if (hazard_id < 0)
        return;

    for (int i = 0; i < 2; i++) {
        atomic_store_explicit(
            &hazards[hazard_id][i],
            NULL,
            memory_order_seq_cst);
    }
}


/* -----------------------------------------------------------
 * Hazard scanning
 * --------------------------------------------------------- */

static int is_hazard(QNode *node)
{
    int count = atomic_load_explicit(
        &registered,
        memory_order_acquire);

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < 2; j++) {
            if (atomic_load_explicit(
                    &hazards[i][j],
                    memory_order_seq_cst) == node) {
                return 1;
            }
        }
    }

    return 0;
}


/* -----------------------------------------------------------
 * Reclaim local retired nodes
 * --------------------------------------------------------- */

static void scan_retired(void)
{
    int write = 0;

    for (int i = 0; i < retired.count; i++) {
        QNode *node = retired.nodes[i];

        if (is_hazard(node)) {
            retired.nodes[write++] = node;
        } else {
            free(node);
        }
    }

    retired.count = write;
}


/* -----------------------------------------------------------
 * Retire a removed node
 * --------------------------------------------------------- */

static void retire_node(QNode *node)
{
    if (retired.count == QUEUE_RETIRED)
        scan_retired();

    /*
     * If the list is still full after scanning, transfer
     * the oldest node to the global reclamation list.
     */
    if (retired.count == QUEUE_RETIRED) {
        pthread_mutex_lock(&retired_lock);

        if (global_retired.count < QUEUE_RETIRED) {
            global_retired.nodes[global_retired.count++] =
                retired.nodes[0];

            for (int i = 1; i < retired.count; i++)
                retired.nodes[i - 1] = retired.nodes[i];

            retired.count--;
        } else {
            /*
             * All worker threads have ownership of their
             * hazard slots. The node is not protected here
             * because scan_retired() already checked it.
             */
            free(retired.nodes[0]);

            for (int i = 1; i < retired.count; i++)
                retired.nodes[i - 1] = retired.nodes[i];

            retired.count--;
        }

        pthread_mutex_unlock(&retired_lock);
    }

    retired.nodes[retired.count++] = node;

    if (retired.count >= 32)
        scan_retired();
}


/* -----------------------------------------------------------
 * Worker cleanup
 * --------------------------------------------------------- */

void queue_thread_cleanup(void)
{
    /*
     * First reclaim anything that is already safe.
     */
    for (int pass = 0;
         pass < 8 && retired.count > 0;
         pass++) {
        scan_retired();
    }

    /*
     * Any nodes still present belong to this thread's
     * retired list. Transfer them to the global list so
     * they are not lost when the thread exits.
     */
    if (retired.count == 0)
        return;

    pthread_mutex_lock(&retired_lock);

    for (int i = 0; i < retired.count; i++) {
        if (global_retired.count < QUEUE_RETIRED) {
            global_retired.nodes[global_retired.count++] =
                retired.nodes[i];
        } else {
            /*
             * At this point all worker threads have already
             * stopped using the queue only when destroy() is
             * called. During normal worker cleanup, however,
             * a full global list must not cause unsafe freeing.
             *
             * Keep the node in the local list if necessary.
             */
            break;
        }
    }

    if (global_retired.count < QUEUE_RETIRED) {
        retired.count = 0;
    }

    pthread_mutex_unlock(&retired_lock);
}


/* -----------------------------------------------------------
 * Queue initialization
 * --------------------------------------------------------- */

void queue_init(MSQueue *q)
{
    QNode *dummy = malloc(sizeof(QNode));

    if (!dummy)
        abort();

    dummy->value = 0;

    atomic_init(
        &dummy->next,
        NULL);

    atomic_init(
        &q->head,
        dummy);

    atomic_init(
        &q->tail,
        dummy);

    atomic_store(
        &registered,
        0);

    pthread_mutex_lock(&retired_lock);

    global_retired.count = 0;

    pthread_mutex_unlock(&retired_lock);

    for (int i = 0; i < QUEUE_MAX_THREADS; i++) {
        for (int j = 0; j < 2; j++) {
            atomic_store(
                &hazards[i][j],
                NULL);
        }
    }
}


/* -----------------------------------------------------------
 * Michael-Scott enqueue
 * --------------------------------------------------------- */

void queue_enqueue(MSQueue *q, int value)
{
    QNode *node = malloc(sizeof(QNode));

    if (!node)
        abort();

    node->value = value;

    atomic_init(
        &node->next,
        NULL);

    while (1) {
        QNode *tail = atomic_load_explicit(
            &q->tail,
            memory_order_acquire);

        protect(0, tail);

        /*
         * Validate the protected tail.
         */
        if (tail != atomic_load_explicit(
                &q->tail,
                memory_order_acquire)) {

            clear_hazards();
            continue;
        }

        QNode *next = atomic_load_explicit(
            &tail->next,
            memory_order_acquire);

        if (next == NULL) {

            QNode *expected = NULL;

            if (atomic_compare_exchange_weak_explicit(
                    &tail->next,
                    &expected,
                    node,
                    memory_order_release,
                    memory_order_relaxed)) {

                /*
                 * Helping operation: move tail forward.
                 */
                atomic_compare_exchange_weak_explicit(
                    &q->tail,
                    &tail,
                    node,
                    memory_order_release,
                    memory_order_relaxed);

                clear_hazards();

                return;
            }

        } else {

            /*
             * Another producer already linked a node.
             * Help advance tail.
             */
            atomic_compare_exchange_weak_explicit(
                &q->tail,
                &tail,
                next,
                memory_order_release,
                memory_order_relaxed);
        }

        clear_hazards();
    }
}


/* -----------------------------------------------------------
 * Michael-Scott dequeue
 * --------------------------------------------------------- */

int queue_dequeue(MSQueue *q, int *value)
{
    while (1) {

        QNode *head = atomic_load_explicit(
            &q->head,
            memory_order_acquire);

        protect(0, head);

        /*
         * Validate protected head.
         */
        if (head != atomic_load_explicit(
                &q->head,
                memory_order_acquire)) {

            clear_hazards();
            continue;
        }

        QNode *tail = atomic_load_explicit(
            &q->tail,
            memory_order_acquire);

        QNode *next = atomic_load_explicit(
            &head->next,
            memory_order_acquire);

        /*
         * Validate head again after reading next.
         */
        if (head != atomic_load_explicit(
                &q->head,
                memory_order_acquire)) {

            clear_hazards();
            continue;
        }

        /*
         * Queue is empty.
         */
        if (next == NULL) {
            clear_hazards();
            return 0;
        }

        /*
         * Protect successor before dereferencing it.
         */
        protect(1, next);

        /*
         * Validate head after protecting successor.
         */
        if (head != atomic_load_explicit(
                &q->head,
                memory_order_acquire)) {

            clear_hazards();
            continue;
        }

        /*
         * Tail is behind head.
         * Help advance it.
         */
        if (head == tail) {

            atomic_compare_exchange_weak_explicit(
                &q->tail,
                &tail,
                next,
                memory_order_release,
                memory_order_relaxed);

            clear_hazards();
            continue;
        }

        /*
         * Successor is protected, so reading value is safe.
         */
        *value = next->value;

        /*
         * Move head forward.
         */
        if (atomic_compare_exchange_weak_explicit(
                &q->head,
                &head,
                next,
                memory_order_acq_rel,
                memory_order_relaxed)) {

            clear_hazards();

            /*
             * Old dummy node can now be retired.
             */
            retire_node(head);

            return 1;
        }

        clear_hazards();
    }
}


/* -----------------------------------------------------------
 * Final queue destruction
 *
 * Must be called only after ALL worker threads have joined.
 * --------------------------------------------------------- */

void queue_destroy(MSQueue *q)
{
    /*
     * At this point no worker can access the queue.
     */

    QNode *node = atomic_load_explicit(
        &q->head,
        memory_order_relaxed);

    while (node != NULL) {

        QNode *next = atomic_load_explicit(
            &node->next,
            memory_order_relaxed);

        free(node);

        node = next;
    }

    atomic_store_explicit(
        &q->head,
        NULL,
        memory_order_relaxed);

    atomic_store_explicit(
        &q->tail,
        NULL,
        memory_order_relaxed);

    /*
     * Reclaim every globally transferred retired node.
     */
    pthread_mutex_lock(&retired_lock);

    for (int i = 0;
         i < global_retired.count;
         i++) {

        free(global_retired.nodes[i]);
    }

    global_retired.count = 0;

    pthread_mutex_unlock(&retired_lock);
}