# Week 5 Lab: Implicit Threading and Threading Issues

> In-class lab (~50 min)
> Textbook: Silberschatz Ch 4 (Sections 4.5–4.7)

## Learning Objectives

Through this lab, you will:

- Implement a simple **thread pool** with a work queue and worker threads
- Use **OpenMP** directives to parallelize loops and perform reductions
- Observe what happens when **fork()** is called in a multithreaded program
- Use **Thread-Local Storage (TLS)** to give each thread private global state

## Prerequisites

```bash
# Navigate to the lab directory
cd examples/

# Compile all examples
gcc -Wall -pthread -o lab1_thread_pool lab1_thread_pool.c
gcc -Wall -fopenmp -o lab2_openmp_parallel lab2_openmp_parallel.c
gcc -Wall -pthread -o lab3_fork_threads lab3_fork_threads.c
gcc -Wall -pthread -o lab4_tls lab4_tls.c
```

> Note: Lab 2 requires OpenMP support (`-fopenmp`). GCC includes this by default.

---

## Lab 1: Thread Pool (~15 min)

### Background

A **thread pool** pre-creates a fixed number of worker threads that wait for tasks in a shared queue. This avoids the overhead of creating and destroying threads for each task.

The three benefits of thread pools (from the textbook):
1. **Faster response** — reuse existing threads instead of creating new ones
2. **Bounded concurrency** — limit the number of threads in the system
3. **Task/execution separation** — decouple what to do from how to schedule it

### Architecture

```
Main Thread                     Worker Threads
    |                          [Worker 0] ─┐
    |── submit(task1) ──→ [Queue] ──→ [Worker 1] ──→ execute task
    |── submit(task2) ──→ [Queue] ──→ [Worker 2] ──→ execute task
    |── submit(task3) ──→ [Queue] ──→ [Worker 3] ─┘
    |
    |── shutdown() ──→ wake all → workers exit
```

### Execution

```bash
./lab1_thread_pool
```

### Checklist

- [ ] Do 4 workers process 12 tasks (3 tasks per worker on average)?
- [ ] Can you identify the producer-consumer pattern in the code?
- [ ] Does graceful shutdown wait for all tasks to finish?

### Questions

1. What happens if you submit tasks faster than workers can process them?
2. Why do workers use `while (count == 0)` instead of `if (count == 0)` before `cond_wait`?
3. How is this similar to Java's `ExecutorService.newFixedThreadPool()`?

---

## Lab 2: OpenMP Parallel For & Reduction (~12 min)

### Background

**OpenMP** is a set of compiler directives for implicit threading. Instead of manually creating threads, you add `#pragma` annotations and the compiler handles the rest.

Key directives:
| Directive | Effect |
|-----------|--------|
| `#pragma omp parallel` | Create a team of threads |
| `#pragma omp parallel for` | Split loop iterations across threads |
| `reduction(+:var)` | Each thread gets a private copy, combined at end |

### Execution

```bash
./lab2_openmp_parallel

# Try different thread counts
OMP_NUM_THREADS=1 ./lab2_openmp_parallel
OMP_NUM_THREADS=2 ./lab2_openmp_parallel
OMP_NUM_THREADS=8 ./lab2_openmp_parallel
```

### Checklist

- [ ] Did Demo 1 show multiple threads printing their IDs?
- [ ] Is the parallel for loop faster than sequential?
- [ ] Does the reduction produce the correct sum (50000000)?

### Questions

1. What would happen if you used `sum += array[i]` without the `reduction` clause?
2. How does OpenMP decide how many iterations each thread gets?
3. Compare OpenMP with manual Pthreads — what are the trade-offs?

---

## Lab 3: fork() in Multithreaded Programs (~13 min)

### Background

When a multithreaded process calls `fork()`, POSIX specifies that **only the calling thread** is duplicated in the child process. All other threads disappear.

This creates two problems:
1. **Lost threads** — background work stops in the child
2. **Orphaned locks** — if a disappeared thread held a mutex, the child may deadlock

The safe pattern: call `exec()` immediately after `fork()`, replacing the entire address space.

### Execution

```bash
./lab3_fork_threads
```

### Expected Behavior

```
[Parent] Thread 0 started
[Parent] Thread 1 started
[Parent] Thread 2 started
[Parent] counter = 7 (3 threads running)

[Child] counter = 7
[Child] counter after 500ms = 7      ← did NOT change!
[Parent] counter = 10 (still incrementing)
```

In the child, `counter` stays at 7 because the background threads were not copied.

### Checklist

- [ ] Did the child's counter remain unchanged?
- [ ] Did the parent's counter continue incrementing?
- [ ] Did the fork+exec demo work correctly?

### Questions

1. Why does POSIX fork() only copy the calling thread?
2. What could go wrong if a disappeared thread held a mutex?
3. When is it safe to NOT call exec() after fork()?

---

## Lab 4: Thread-Local Storage (TLS) (~10 min)

### Background

**Thread-Local Storage** gives each thread its own private copy of a global variable. Without TLS, global variables are shared and require locks. With TLS, each thread has an independent copy — no synchronization needed.

| Approach | Declaration | Shared? |
|----------|------------|---------|
| Global | `int var;` | Yes — all threads see the same value |
| Thread-local | `__thread int var;` | No — each thread has its own copy |

Real-world use: `errno` is implemented as TLS so each thread gets its own error code.

### Execution

```bash
./lab4_tls
```

### Expected Behavior

```
--- Demo 1: Shared Global vs Thread-Local ---
[Thread 0] tls_var = 100000 (expected 100000)    ← always correct
[Thread 1] tls_var = 100000 (expected 100000)
...
shared_var = 287453 (expected 400000) RACE CONDITION!
Each thread's tls_var was 100000 (always correct)
```

### Checklist

- [ ] Is `shared_var` less than expected? (race condition)
- [ ] Is each thread's `tls_var` exactly 100000?
- [ ] In Demo 2, does each thread maintain its own error code?

### Questions

1. Why doesn't `__thread` need a mutex?
2. How is `__thread` different from a local variable on the stack?
3. What are the limitations of TLS? (Hint: dynamic allocation, thread creation)

---

## Summary and Key Takeaways

| Concept | Description | Theory Reference |
|---------|-------------|-----------------|
| Thread Pool | Pre-created workers + task queue | Ch 4.5.1 — Thread Pools |
| OpenMP | Compiler directives for implicit threading | Ch 4.5.3 — OpenMP |
| fork() with threads | Only calling thread is copied | Ch 4.6.1 — fork/exec Semantics |
| Thread-Local Storage | Per-thread private global variables | Ch 4.6.4 — TLS |
