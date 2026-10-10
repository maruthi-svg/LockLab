#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>

static volatile sig_atomic_t snapshot_requested = 0;

static _Atomic unsigned long operations = 0;
static _Atomic unsigned long pushes = 0;
static _Atomic unsigned long pops = 0;

static void handle_snapshot(int signal_number)
{
    (void)signal_number;

    /*
     * Only async-signal-safe state is changed here.
     * The actual statistics printing happens outside
     * the signal handler.
     */
    snapshot_requested = 1;
}

static void print_snapshot(void)
{
    unsigned long op_count =
        atomic_load_explicit(
            &operations,
            memory_order_relaxed);

    unsigned long push_count =
        atomic_load_explicit(
            &pushes,
            memory_order_relaxed);

    unsigned long pop_count =
        atomic_load_explicit(
            &pops,
            memory_order_relaxed);

    printf(
        "STATS SNAPSHOT: operations=%lu pushes=%lu pops=%lu\n",
        op_count,
        push_count,
        pop_count);
}

int main(void)
{
    struct sigaction action;

    action.sa_handler = handle_snapshot;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(
            SIGUSR1,
            &action,
            NULL) != 0) {

        perror("sigaction");
        return 1;
    }

    printf("\n");
    printf("============================================================\n");
    printf("              LOCKLAB SIGNAL-SAFE STATS\n");
    printf("============================================================\n");

    printf("PID                : %d\n", getpid());
    printf("Send SIGUSR1 using : kill -USR1 %d\n", getpid());
    printf("Waiting for signal...\n");

    while (!snapshot_requested) {

        atomic_fetch_add_explicit(
            &operations,
            1,
            memory_order_relaxed);

        if ((atomic_load_explicit(
                 &operations,
                 memory_order_relaxed) % 2) == 0) {

            atomic_fetch_add_explicit(
                &pushes,
                1,
                memory_order_relaxed);
        } else {
            atomic_fetch_add_explicit(
                &pops,
                1,
                memory_order_relaxed);
        }

        struct timespec delay;
        delay.tv_sec = 0;
        delay.tv_nsec = 1000000L;

        nanosleep(&delay, NULL);
    }

    snapshot_requested = 0;

    print_snapshot();

    printf("Signal handling   : PASS\n");
    printf("============================================================\n\n");

    return 0;
}