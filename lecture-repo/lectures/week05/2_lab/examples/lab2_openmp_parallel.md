# Lab 2: OpenMP Parallel For and Reduction

## Description

Demonstrates OpenMP compiler directives for implicit threading. Three demos: basic parallel region, parallel for loop with speedup measurement, and reduction clause for parallel summation.

## Build & Run

```bash
gcc -Wall -fopenmp -o lab2_openmp_parallel lab2_openmp_parallel.c
./lab2_openmp_parallel
```

Note: The `-fopenmp` flag is required to enable OpenMP.

## Key Concepts

- `#pragma omp parallel` — create a team of threads
- `#pragma omp parallel for` — auto-split loop iterations
- `reduction(+:var)` — each thread gets a private copy, combined at end
- `omp_get_wtime()` for portable wall-clock timing

## Theory Connection

- Ch 4.5.3: OpenMP — overview, parallel regions, parallel for, reduction
