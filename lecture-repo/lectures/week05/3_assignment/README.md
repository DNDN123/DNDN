# Week 5 Assignment: Implicit Threading

> Textbook reference: Silberschatz Ch 4 (Sections 4.5–4.7)
>
> Deadline: Before the next class
>
> **Environment**: Linux or macOS with gcc (OpenMP requires `-fopenmp`)

---

## Assignment 1: Fork-Join Parallel Merge Sort (Required)

> Based on textbook Ch 4.5.2 — Fork-Join Parallelism

### Background

**Fork-Join** is a parallel pattern where a task is divided into subtasks
(fork), each subtask runs in parallel, and the results are combined (join).
This maps naturally to divide-and-conquer algorithms like merge sort.

```
            [8, 3, 1, 5, 4, 7, 2, 6]
                    fork
           /                    \
    [8, 3, 1, 5]          [4, 7, 2, 6]
        fork                   fork
       /    \                 /    \
   [8,3]  [1,5]          [4,7]  [2,6]
     |      |               |      |
   [3,8]  [1,5]          [4,7]  [2,6]
       \    /                 \    /
    [1, 3, 5, 8]          [2, 4, 6, 7]
           \                    /
                    join
            [1, 2, 3, 4, 5, 6, 7, 8]
```

In a sequential merge sort, each division is processed one at a time.
In a fork-join merge sort, the two halves are sorted **in parallel** using threads.

### Goal

Implement a parallel merge sort using Pthreads that:
1. **Forks** a new thread to sort one half while the current thread sorts the other
2. **Joins** the thread and merges the two sorted halves
3. Uses a **depth threshold** to avoid creating too many threads (falls back to sequential below the threshold)
4. Compares performance against the sequential version

### Implementation Requirements

#### 1. `merge()` — Merge two sorted halves

Standard merge operation. This is already provided in the skeleton.

#### 2. `merge_sort_sequential()` — Sequential merge sort

Standard recursive merge sort (no threads). Used as the baseline for comparison
and as a fallback when the depth threshold is reached.

#### 3. `merge_sort_parallel()` — Parallel fork-join merge sort

```c
struct sort_arg {
    int *array;
    int *temp;
    int left;
    int right;
    int depth;      /* current recursion depth */
};

void *merge_sort_parallel(void *arg)
{
    // If array size <= 1, return
    // If depth >= MAX_DEPTH, fall back to sequential
    //
    // Otherwise:
    //   1. Fork: create a thread for the left half
    //   2. Sort the right half in the current thread (recursive call)
    //   3. Join: wait for the left-half thread
    //   4. Merge the two sorted halves
}
```

**Key design decisions:**
- Only fork one thread per level (not two) — the current thread handles the other half
- `MAX_DEPTH` limits the number of threads to 2^MAX_DEPTH
- Below `MAX_DEPTH`, fall back to `merge_sort_sequential()`

#### 4. `main()` — Performance comparison

- Generate a random array of N integers
- Sort with sequential merge sort and measure time
- Sort with parallel merge sort and measure time
- Print speedup ratio
- Verify both produce the same sorted result

### Getting Started

```bash
cd skeleton/
gcc -Wall -pthread -O2 -o mergesort mergesort.c
./mergesort              # default: 10,000,000 elements, depth 3
./mergesort 5000000 4    # custom: 5M elements, depth 4
```

Find the `TODO` comments in `mergesort.c` and complete the code.

### Expected Output

```
=== Fork-Join Parallel Merge Sort ===
Array size: 10000000, Max thread depth: 3 (up to 8 threads)

Sequential: 1.2345 sec
Parallel:   0.4567 sec
Speedup:    2.70x
Verified:   CORRECT
```

### Questions to Consider

1. What happens if `MAX_DEPTH` is too large? (Hint: thread creation overhead)
2. Why do we fork only one thread instead of two at each level?
3. How does this relate to Java's `ForkJoinPool` discussed in the textbook?
4. Compare the speedup with Amdahl's Law — what is the serial fraction?

### Grading Criteria

| Item | Weight |
|------|--------|
| Correct `merge_sort_parallel()` with thread fork/join | 30% |
| Proper depth threshold to limit thread count | 20% |
| Correct merge producing sorted output | 20% |
| Performance comparison (sequential vs parallel) | 20% |
| Code quality and answers to questions | 10% |

---

## Assignment 2: OpenMP Matrix Multiplication (Bonus, +20 points)

> Based on textbook Ch 4.5.3 — OpenMP

### Background

Matrix multiplication is a classic data-parallel workload. Each element
of the result matrix C can be computed independently:

```
C[i][j] = sum(A[i][k] * B[k][j]) for k = 0..N-1
```

This makes it ideal for OpenMP's `#pragma omp parallel for`.

### Goal

Implement matrix multiplication in three versions and compare performance:
1. **Sequential** — no parallelism
2. **OpenMP basic** — `#pragma omp parallel for` on the outer loop
3. **OpenMP optimized** — with `collapse(2)` and/or loop reordering (i-k-j)

### Implementation Requirements

#### 1. `matmul_sequential()` — Baseline

Standard triple-nested loop (i-j-k order).

#### 2. `matmul_openmp_basic()` — Basic OpenMP

Add `#pragma omp parallel for` to the outermost loop.

#### 3. `matmul_openmp_optimized()` — Optimized OpenMP

Apply one or both optimizations:
- **`collapse(2)`**: Parallelize across both i and j loops for better load balancing
- **Loop reordering (i-k-j)**: Better cache locality since B is accessed sequentially

### Getting Started

```bash
cd skeleton/
gcc -Wall -fopenmp -O2 -o matmul matmul.c
./matmul              # default: 1024x1024
./matmul 512          # custom: 512x512
```

### Expected Output

```
=== OpenMP Matrix Multiplication ===
Matrix size: 1024 x 1024

Sequential (i-j-k):     3.4567 sec
OpenMP basic:            1.2345 sec  (2.80x)
OpenMP optimized (i-k-j): 0.5678 sec  (6.09x)
Verified: CORRECT
```

### Grading Criteria

| Item | Weight |
|------|--------|
| Correct sequential matrix multiplication | 20% |
| Correct OpenMP basic version | 30% |
| Optimized version with measurable improvement | 30% |
| Performance comparison and analysis | 20% |

---

## Deliverables

```
week05/3_assignment/
├── skeleton/
│   ├── mergesort.c        <- Complete the TODOs and submit (required)
│   └── matmul.c           <- Complete the TODOs and submit (bonus)
└── report.md              <- Brief implementation description (optional)
```

### report.md Writing Guide (Optional)

Bonus points will be awarded for answering the following questions:

1. How does `MAX_DEPTH` affect performance? What is the optimal value on your machine?
2. Why does loop reordering (i-k-j) improve cache performance?
3. Compare the fork-join pattern with OpenMP — when would you use each?

---

## References

- Silberschatz Ch 4.5.2: Fork-Join Parallelism
- Silberschatz Ch 4.5.3: OpenMP
- Java ForkJoinPool documentation (conceptual comparison)
