#!/bin/bash

set -e

echo
echo "============================================================"
echo "                    LOCKLAB FINAL DEMO"
echo "============================================================"

echo
echo "[1/10] Building project..."
make clean
make -j"$(nproc)"

echo
echo "[2/10] Treiber Stack..."
./stack_test

echo
echo "[3/10] Michael-Scott Queue..."
./queue_test

echo
echo "[4/10] Bounded MPMC Ring Buffer..."
./ring_test

echo
echo "[5/10] ABA Demonstration..."
./aba_demo

echo
echo "[6/10] Workload Parser + Parallel Dispatch..."
./workload_test

echo
echo "[7/10] Offline History Check..."
./history_check

echo
echo "[8/10] Pipe Workload Generator..."
./pipe_workload

echo
echo "[9/10] Signal-Safe Statistics..."
./stats_test &
STATS_PID=$!

sleep 1

kill -USR1 "$STATS_PID"
wait "$STATS_PID"

echo
echo "[10/10] Lock-Free vs Mutex Benchmark..."
./locklab_bench

echo
echo "============================================================"
echo "                 LOCKLAB DEMO COMPLETE"
echo "============================================================"
echo "All LockLab components executed successfully."
echo