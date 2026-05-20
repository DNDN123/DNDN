# Week 4 Lab: Pthreads — Thread Creation, Data Parallelism, and Speedup

> In-class lab (~50 min)
> Textbook: Silberschatz Ch 4 (Threads & Concurrency)

## Learning Objectives

Through this lab, you will:

- Create and join threads using the **Pthreads** API (`pthread_create`, `pthread_join`)
- Apply **data parallelism** by splitting an array across multiple threads
- Understand a common **thread argument pitfall** (passing `&i` vs `&tids[i]`)
- Measure **speedup** with varying thread counts and relate it to **Amdahl's Law**

## Prerequisites

```bash
# Navigate to the lab directory
cd examples/

# Compile all examples at once
gcc -Wall -pthread -o lab1_hello_threads lab1_hello_threads.c
gcc -Wall -pthread -o lab2_parallel_sum lab2_parallel_sum.c
gcc -Wall -pthread -o lab3_arg_pitfall lab3_arg_pitfall.c
gcc -Wall -O2 -pthread -o lab4_speedup lab4_speedup.c
```

---

## Lab 1: Hello Threads (~10 min)

### Background

A **thread** is a unit of CPU utilization within a process. Threads in the same process share code, data, and files, but each has its own stack, registers, and program counter.

The Pthreads API provides two essential functions:

| Function | Description |
|----------|-------------|
| `pthread_create()` | Create a new thread |
| `pthread_join()` | Wait for a thread to terminate |

### Execution

```bash
# Default: 4 threads
./lab1_hello_threads

# Try different thread counts
./lab1_hello_threads 1
./lab1_hello_threads 8

# Run multiple times to observe different orderings
for i in 1 2 3; do ./lab1_hello_threads; echo "---"; done
```

### Checklist

- [ ] Did all threads print their greeting message?
- [ ] Did the output order change between runs? (non-deterministic scheduling)
- [ ] Does the program wait for all threads before printing "All threads finished"?

### Questions

1. What happens if you remove `pthread_join()`? (Hint: the main thread might exit first)
2. Why is the output order different each time?
3. What do all threads share? What does each thread own independently?

---

## Lab 2: Data Parallel Array Summation (~15 min)

### Background

**Data parallelism** distributes subsets of data across threads, each performing the same operation on its portion. This is the most common form of parallelism.

In this lab, an array of 1000 elements is split across N threads. Each thread computes a partial sum, and the main thread combines the results.

```
Array: [  0~249  |  250~499  |  500~749  |  750~999  ]
           |          |           |            |
       Thread 0   Thread 1   Thread 2     Thread 3
       partial[0] partial[1] partial[2]   partial[3]
           \          |           |          /
                     Total Sum = 500500
```

### Execution

```bash
# Default: 4 threads
./lab2_parallel_sum

# Try different thread counts
./lab2_parallel_sum 1
./lab2_parallel_sum 2
./lab2_parallel_sum 4
```

### Code Analysis

Open `lab2_parallel_sum.c` and trace the flow:

1. How is the array divided among threads? (`chunk`, `start`, `end`)
2. Why does each thread write to `partial_sum[id]` instead of a shared variable?
3. Why is the join loop separate from the create loop?

### Checklist

- [ ] Is the total always 500500 regardless of thread count?
- [ ] Can you explain how `start` and `end` are calculated for each thread?
- [ ] Do you understand why `partial_sum[id]` avoids conflicts?

### Questions

1. What would happen if all threads wrote to a single shared `total` variable instead?
2. Is this an example of data parallelism or task parallelism?
3. What happens if ARRAY_SIZE is not evenly divisible by the number of threads?

---

## Lab 3: Thread Argument Pitfall (~10 min)

### Background

A common mistake when creating threads is passing `&i` (the loop variable's address) directly to `pthread_create()`. Since all threads share the same pointer, the value may change before a thread reads it.

```c
// BUGGY: all threads share &i
for (int i = 0; i < N; i++)
    pthread_create(&threads[i], NULL, func, &i);   // danger!

// CORRECT: each thread gets its own copy
int tids[N];
for (int i = 0; i < N; i++) {
    tids[i] = i;
    pthread_create(&threads[i], NULL, func, &tids[i]);
}
```

### Execution

```bash
# Run multiple times to see the bug
./lab3_arg_pitfall
./lab3_arg_pitfall
./lab3_arg_pitfall
```

### Checklist

- [ ] In the buggy version, did you see duplicate or out-of-range IDs?
- [ ] In the correct version, did each thread get a unique ID (0 to 3)?
- [ ] Can you explain why `&i` causes the problem?

### Questions

1. Why doesn't the buggy version always produce wrong results?
2. Is this a race condition? Why or why not?
3. What other approaches could fix this? (Hint: casting `(void *)(intptr_t)i`)

---

## Lab 4: Measuring Speedup & Amdahl's Law (~15 min)

### Background

**Amdahl's Law** tells us the theoretical maximum speedup:

```
speedup <= 1 / (S + (1-S)/N)
```

- **S** = serial fraction of the program
- **N** = number of threads (cores)

Even with infinite cores, the speedup is limited to **1/S**.

This lab measures actual wall-clock time for a 50-million-element array sum using 1, 2, 4, and 8 threads, and compares the observed speedup with the theoretical prediction.

### Execution

```bash
# Run the speedup benchmark
./lab4_speedup

# Run multiple times for stable results
./lab4_speedup
./lab4_speedup
```

### Expected Output

```
=== Speedup Measurement Demo ===
Array size: 50000000 (400 MB)

  Threads: 1, Sum: 50000000, Time: 0.0XXX sec
  Threads: 2, Sum: 50000000, Time: 0.0XXX sec
  Threads: 4, Sum: 50000000, Time: 0.0XXX sec
  Threads: 8, Sum: 50000000, Time: 0.0XXX sec

--- Speedup Summary ---
Threads    Time (sec)   Speedup
1          0.0XXX       1.00x
2          0.0XXX       ~1.5-2.0x
4          0.0XXX       ~2.0-4.0x
8          0.0XXX       varies
```

### Checklist

- [ ] Does the speedup increase with more threads?
- [ ] Is the speedup less than the ideal (1x, 2x, 4x, 8x)?
- [ ] Can you identify what the serial fraction might be? (thread creation, joining, memory access overhead)

### Questions

1. Why is the actual speedup less than the ideal?
2. According to Amdahl's Law, if 10% of the program is serial, what is the max speedup with 8 threads?
3. Does adding more threads always help? When might it hurt?

---

## Summary and Key Takeaways

| Concept | Description | Theory Reference |
|---------|-------------|-----------------|
| Thread basics | `pthread_create()`, `pthread_join()` | Ch 4.4 — Pthreads API |
| Data parallelism | Split data across threads, same operation | Ch 4.2 — Types of Parallelism |
| Argument pitfall | Use `&tids[i]`, not `&i` | Ch 4.4 — Pthreads examples |
| Amdahl's Law | Speedup limited by serial fraction | Ch 4.2 — Multicore Programming |

### Key Pthreads Pattern

```c
pthread_t threads[N];
int tids[N];

/* create threads (they start running immediately) */
for (int i = 0; i < N; i++) {
    tids[i] = i;
    pthread_create(&threads[i], NULL, func, &tids[i]);
}

/* wait for all threads to finish */
for (int i = 0; i < N; i++)
    pthread_join(threads[i], NULL);
```
