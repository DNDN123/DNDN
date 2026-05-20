# Lab 4: Measuring Speedup & Amdahl's Law

## Description

Measures wall-clock time for a 50-million-element array sum using 1, 2, 4, and 8 threads. Displays the speedup relative to the single-threaded baseline. The observed speedup is always less than the ideal due to serial overhead (thread creation, memory bus contention), which illustrates Amdahl's Law.

## Build & Run

```bash
gcc -Wall -O2 -pthread -o lab4_speedup lab4_speedup.c
./lab4_speedup
```

Run multiple times for stable results. The `-O2` flag enables compiler optimizations for more realistic timing.

## Key Concepts

- Amdahl's Law: `speedup <= 1 / (S + (1-S)/N)` where S = serial fraction, N = threads
- Serial overhead: thread creation/join, memory bandwidth contention
- More threads does not always mean proportionally better performance

## Theory Connection

- Ch 4.2: Multicore Programming — Amdahl's Law, Concurrency vs Parallelism
- Ch 4.2: Five Challenges of Multicore Programming
