# Week 4 Assignment: Pthreads Data Parallelism

> Textbook: Silberschatz Ch 4 (Threads & Concurrency)
>
> Deadline: Before the next class
>
> **Environment**: Linux or macOS with gcc and pthreads

---

## Assignment: Parallel Histogram (Required)

### Background

A **histogram** counts how often each value appears in a dataset.
For example, given exam scores 0–99 for 10 million students,
a histogram counts how many students got each score.

This is a classic **data parallelism** problem: the dataset is split
among N threads, each builds a local histogram over its chunk, and
the main thread merges the results.

```
Data: [72, 85, 72, 91, 85, 60, ...]   (10,000,000 values)

Thread 0: [72, 85, 72, ...]  → local_hist0[72]=2, [85]=1, ...
Thread 1: [91, 85, 60, ...]  → local_hist1[91]=1, [85]=1, [60]=1, ...
Thread 2: ...                 → local_hist2[...]
Thread 3: ...                 → local_hist3[...]

Merge: global_hist[i] = sum of local_hist*[i] for all threads
```

Each thread works on its own local histogram (separate array), so
no synchronization is needed during the counting phase. This is the
same pattern as `partial_sum[id]` from Lab 2.

### Learning Objectives

- Apply **data parallelism** with Pthreads (`pthread_create`, `pthread_join`)
- Use **separate per-thread arrays** to avoid sharing conflicts
- Measure **speedup** with different thread counts and relate to **Amdahl's Law**
- Practice the create-loop / join-loop pattern from the textbook

### Implementation Requirements

#### 1. Data Generation

Generate an array of `N` random integers in range `[0, NUM_BINS)`:

```c
#define NUM_BINS 100        /* histogram bins (e.g., exam scores 0–99) */
#define DEFAULT_SIZE 10000000
```

#### 2. `histogram_sequential()` — Baseline

```c
void histogram_sequential(int *data, int n, int *hist)
{
    memset(hist, 0, NUM_BINS * sizeof(int));
    for (int i = 0; i < n; i++)
        hist[data[i]]++;
}
```

#### 3. `histogram_parallel()` — Parallel version

Each thread:
1. Receives its chunk range (`start`, `end`) and a **local histogram array**
2. Counts values in `data[start..end)` into its local histogram
3. Returns (no shared state modified)

Main thread:
1. Creates `nthreads` threads with separate local histograms
2. Joins all threads
3. Merges local histograms into the global result:
   `global_hist[i] = sum of local_hist[t][i]` for all threads t

#### 4. `main()` — Performance comparison

- Run sequential and parallel (1, 2, 4, 8 threads)
- Print elapsed time and speedup for each
- Verify parallel results match sequential

### Getting Started

```bash
cd skeleton/
gcc -Wall -pthread -O2 -o histogram histogram.c
./histogram                    # default: 10M values, 4 threads
./histogram 20000000 8         # custom: 20M values, 8 threads
```

Find the `TODO` comments in `histogram.c` and complete the code.

### Expected Output

```
=== Parallel Histogram ===
Data size: 10000000, Bins: 100

Sequential:          0.0312 sec

Parallel (1 thread): 0.0315 sec  Speedup: 0.99x
Parallel (2 threads): 0.0178 sec  Speedup: 1.75x
Parallel (4 threads): 0.0098 sec  Speedup: 3.18x
Parallel (8 threads): 0.0067 sec  Speedup: 4.66x

Verification: ALL CORRECT

--- Top 5 most frequent values ---
Value 42: 100,234 occurrences
Value 17: 100,198 occurrences
...
```

### Questions (include in report)

1. Why does each thread use a **local histogram** instead of a shared one?
   What would happen if all threads incremented `global_hist[data[i]]++` directly?

2. Plot or describe how speedup changes with 1, 2, 4, 8 threads.
   Does it match the prediction from Amdahl's Law? What is the serial fraction?

3. What is the overhead of creating/joining threads? Try with a very
   small dataset (e.g., 1000 elements) — is parallel still faster?

4. Compare this pattern with Lab 2 (parallel array sum). What is similar?
   What is different?

### Grading Criteria

| Item | Weight |
|------|--------|
| Correct thread creation with chunk splitting | 25% |
| Local histogram per thread (no sharing conflicts) | 25% |
| Correct merge of local histograms | 20% |
| Performance measurement across thread counts | 20% |
| Report answers | 10% |

---

## Deliverables

```
week04/3_assignment/
├── skeleton/
│   └── histogram.c     <- Complete the TODOs and submit
└── report.md           <- Performance results and question answers
```

---

## References

- Silberschatz Ch 4.2: Multicore Programming, Amdahl's Law
- Silberschatz Ch 4.4: Pthreads — create, join, data sharing
- Lab 2: Data Parallel Array Sum (same pattern, simpler scale)
