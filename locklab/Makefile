CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -O2 -pthread -Iinclude

all: locklab stack_test queue_test hazard_test aba_demo ring_test workload_test history_check pipe_workload stats_test locklab_bench

locklab: src/main.c src/lf_stack.c
	$(CC) $(CFLAGS) src/main.c src/lf_stack.c -o locklab

stack_test: tests/stack_test.c src/lf_stack.c
	$(CC) $(CFLAGS) tests/stack_test.c src/lf_stack.c -o stack_test

queue_test: tests/queue_test.c src/ms_queue.c
	$(CC) $(CFLAGS) tests/queue_test.c src/ms_queue.c -o queue_test

ring_test: tests/ring_test.c src/ring_buffer.c
	$(CC) $(CFLAGS) tests/ring_test.c src/ring_buffer.c -o ring_test

hazard_test: tests/aba_demo.c src/hazard.c
	$(CC) $(CFLAGS) tests/aba_demo.c src/hazard.c -o hazard_test

aba_demo: tests/aba_demo.c
	$(CC) $(CFLAGS) tests/aba_demo.c -o aba_demo

workload_test: tests/workload_test.c src/lf_stack.c
	$(CC) $(CFLAGS) tests/workload_test.c src/lf_stack.c -o workload_test

history_check: tests/history_check.c
	$(CC) $(CFLAGS) tests/history_check.c -o history_check

pipe_workload: tests/pipe_workload.c src/lf_stack.c
	$(CC) $(CFLAGS) tests/pipe_workload.c src/lf_stack.c -o pipe_workload

stats_test: tests/stats_test.c
	$(CC) $(CFLAGS) tests/stats_test.c -o stats_test

locklab_bench: benchmark/benchmark.c src/lf_stack.c src/locked_stack.c
	$(CC) $(CFLAGS) benchmark/benchmark.c src/lf_stack.c src/locked_stack.c -o locklab_bench

clean:
	rm -f locklab stack_test queue_test ring_test hazard_test aba_demo \
	      workload_test history_check pipe_workload stats_test \
	      locklab_bench stack_asan queue_asan locklab_bench_asan

.PHONY: all clean