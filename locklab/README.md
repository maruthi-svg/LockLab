# LockLab

## Lock-Free Data Structures Laboratory

LockLab is a C11 concurrency project demonstrating practical lock-free programming, atomic CAS operations, memory reclamation, ABA protection, concurrent correctness testing, and performance benchmarking.

## Components

- Treiber lock-free stack
- Michael-Scott lock-free queue
- Hazard-pointer memory reclamation
- ABA problem demonstration
- Tagged-pointer ABA protection
- Concurrent stress testing
- Lock-free vs mutex benchmark
- AddressSanitizer validation
- ThreadSanitizer attempted under WSL2

## Build

    make clean
    make

## Run

    ./locklab
    ./stack_test
    ./queue_test
    ./aba_demo
    ./locklab_bench

## Correctness

The stack stress test uses:

    4 producers
    4 consumers
    4000 total items

The queue stress test uses:

    4 producers
    4 consumers
    4000 total items

Both tests verify that expected values are not lost or duplicated.

## ABA Protection

LockLab demonstrates the classic:

    A -> B -> A

ABA sequence.

The protected implementation uses a tagged pointer containing:

    pointer + version

Therefore returning to the same pointer does not hide an intermediate modification.

## Benchmark

The contention benchmark compares:

    Lock-Free Stack
            vs
    Mutex Stack

The measured result on the current system is reported directly rather than assuming lock-free will always be faster.

## Sanitizers

AddressSanitizer has been used to validate memory safety.

ThreadSanitizer was attempted under Ubuntu 24.04 on WSL2. The TSan runtime terminated with:

    FATAL: ThreadSanitizer: unexpected memory mapping

Therefore TSan cannot be considered a valid pass/fail result in this WSL environment.

## Memory Ordering

The implementation uses C11 atomic operations with acquire, release, relaxed, and compare-exchange semantics where appropriate.

The design document contains the detailed memory-ordering discussion.

## Project Structure

    locklab/
    ├── benchmark/
    ├── docs/
    ├── include/
    ├── src/
    ├── tests/
    ├── Makefile
    └── README.md

## Project Goal

LockLab demonstrates that correct lock-free programming requires more than replacing a mutex with CAS.

It also requires:

- Atomicity
- Memory ordering
- Safe pointer publication
- Memory reclamation
- ABA protection
- Concurrent correctness testing
- Performance measurement
