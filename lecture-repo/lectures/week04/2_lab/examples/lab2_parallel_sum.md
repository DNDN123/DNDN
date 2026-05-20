# Lab 2: Data Parallel Array Sum

## Description

Demonstrates **data parallelism** by splitting a 1000-element array across N threads. Each thread computes a partial sum of its chunk, and the main thread combines the results. The total should always be 500500 (= 1+2+...+1000).

## Build & Run

```bash
gcc -Wall -pthread -o lab2_parallel_sum lab2_parallel_sum.c
./lab2_parallel_sum
```

Optional argument: `./lab2_parallel_sum [num_threads]`

```bash
./lab2_parallel_sum 1
./lab2_parallel_sum 2
./lab2_parallel_sum 4
```

## Key Concepts

- Data parallelism: same operation on different data subsets
- Each thread writes to `partial_sum[id]` — a separate index per thread, avoiding conflicts
- The create loop and join loop are separated so all threads run in parallel

## Theory Connection

- Ch 4.2: Types of Parallelism — Data Parallelism vs Task Parallelism
- Ch 4.4: Pthreads — multiple thread creation and join pattern
