---
theme: default
title: "Week 5 — Thread & Concurrency 2"
info: "Operating Systems Ch 4"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Thread & Concurrency 2

Operating Systems Ch 4 (Sections 4.5 - 4.7)

Week 5

---

# Today's Schedule

| Hour | Content |
|------|---------|
| **1st** | **Quiz (Beginning)** → Theory Lecture (Part 1) |
| **2nd** | Theory Lecture (Part 2) |
| **3rd** | Hands-on Lab |

---
layout: section
---

# Part 1
## Implicit Threading

---

# Implicit Threading Overview

<div class="text-left text-lg leading-10">

**Definition**: A strategy that delegates thread creation and management to the **compiler and runtime library**

</div>

<div class="text-left text-base leading-8">

**Why is it needed?**
- Applications with hundreds to thousands of threads have emerged
- Guaranteeing **correctness** of multithreaded programming is extremely difficult
- Direct thread management by programmers leads to exploding complexity

**Core idea**: Developers only **identify parallelizable tasks** → library handles thread mapping

**Five main techniques**:
1. Thread Pools
2. Fork-Join Parallelism
3. OpenMP
4. Grand Central Dispatch (GCD)
5. Intel Thread Building Blocks (TBB)

</div>

---

# Implicit Threading: Task vs Thread

<div class="text-left text-lg leading-10">

**Task-based approach**

- Tasks are typically written as **functions**
- The runtime library maps tasks to separate threads
- Generally utilizes the **Many-to-Many model**
- Developers do not need to worry about thread count, creation timing, or scheduling

</div>

```text
┌──────────────────────────────────────┐
│          Application Code            │
│    (identify parallel tasks)         │
├──────────────────────────────────────┤
│     Compiler / Runtime Library       │
│  (create threads, assign tasks,      │
│   manage scheduling)                 │
├──────────────────────────────────────┤
│         Operating System             │
│    (kernel thread management)        │
└──────────────────────────────────────┘
```

---

# Thread Pools — Concept

<div class="text-left text-base leading-8">

**Problem scenario** (web server example):
- What if a new thread is created for every request?
  - Thread creation takes time → destroyed after completion (wasteful)
  - **No limit** on the number of concurrent threads → risk of system resource exhaustion

**Thread Pool solution**:
- **Pre-create a fixed number of threads** at startup and keep them waiting in the pool
- When a request arrives, **wake an idle thread** from the pool to handle the task
- If no idle threads are available → task **waits in a queue**
- After task completion, the thread is **returned** to the pool for reuse

</div>

```text
Request ──→ [ Task Queue ] ──→ ┌─ Thread 1 (busy) ─┐
Request ──→ [ Task Queue ] ──→ │  Thread 2 (idle)   │ ──→ Execute
Request ──→ [ Task Queue ] ──→ │  Thread 3 (busy)   │
                               │  Thread 4 (idle)   │
                               └─ Thread Pool ──────┘
```

---

# Thread Pools — Benefits

<div class="text-left text-lg leading-10">

**Three textbook benefits** (Silberschatz, p.178):

1. **Speed**: Reusing existing threads → eliminates wait time for new thread creation
2. **Resource bound**: **Bounds** the number of concurrent threads → prevents system overload
3. **Separation of execution and creation**: Separates task execution from creation mechanism
   - Enables various strategies such as delayed execution, periodic execution, etc.

**Determining pool size**:
- Set **heuristically** based on CPU count, physical memory, expected concurrent requests
- Can be dynamically adjusted (e.g., GCD automatically adjusts based on system load)

</div>

---

# Thread Pools — Windows API

<div class="text-left text-base leading-8">

Windows provides a thread pool API, used similarly to Thread_Create()

</div>

```c
// Define a function to be executed as a separate thread
DWORD WINAPI PoolFunction(PVOID Param) {
    /* this function runs as a separate thread. */
}

// Submit work to the thread pool
QueueUserWorkItem(&PoolFunction, NULL, 0);
```

<div class="text-left text-base leading-8">

**Three parameters of QueueUserWorkItem()**:
1. `LPTHREAD_START_ROUTINE Function` — function pointer to execute as a separate thread
2. `PVOID Param` — parameter to pass to the function
3. `ULONG Flags` — flags specifying how the thread pool creates/manages threads

Additional utilities: **periodic execution**, **callback on asynchronous I/O completion**, etc.

</div>

---

# Thread Pools — Java ExecutorService

<div class="text-left text-base leading-8">

Three thread pool architectures provided by the `java.util.concurrent` package:

| Factory Method | Description |
|---------------|-------------|
| `newSingleThreadExecutor()` | Creates a pool of size 1 |
| `newFixedThreadPool(int size)` | Creates a fixed-size pool |
| `newCachedThreadPool()` | Unbounded pool, reuses idle threads |

</div>

```java
import java.util.concurrent.*;

public class ThreadPoolExample {
    public static void main(String[] args) {
        int numTasks = Integer.parseInt(args[0].trim());

        /* Create the thread pool */
        ExecutorService pool = Executors.newCachedThreadPool();

        /* Run each task using a thread in the pool */
        for (int i = 0; i < numTasks; i++)
            pool.execute(new Task());

        /* Shut down the pool once all threads have completed */
        pool.shutdown();
    }
}
```

<div class="text-left text-sm">

Figure 4.15 — Creating a thread pool in Java (textbook p.180)

</div>

---

# Java ExecutorService — Details

<div class="text-left text-base leading-8">

**ExecutorService interface structure**:

```text
      Executor (interface)
          │
          ▼
    ExecutorService (interface)
          │
    ┌─────┴─────┐
    │            │
execute()    submit()    shutdown()
(Runnable)   (Callable)  (shutdown request)
```

**execute() vs submit()**:
- `execute(Runnable)` — executes a task with no return value
- `submit(Callable\<T\>)` — returns a `Future\<T\>`, result can be retrieved later

**When shutdown() is called**:
- Rejects additional task submissions
- Waits for all currently running tasks to complete before terminating

**Note — Android AIDL**: Provides a thread pool to remote services, handling multiple concurrent requests with individual threads from the pool

</div>

---
layout: section
---

# Fork-Join Parallelism

---

# Fork-Join — Concept

<div class="text-left text-base leading-8">

**Fork-Join model**:
- The main (parent) thread **forks** (creates) one or more child tasks
- Child tasks execute in parallel
- Once all children complete, they **join** (merge) and results are combined

**From the implicit threading perspective**:
- Instead of creating threads directly, **parallel tasks are specified**
- The library determines the number of threads and assigns tasks to them
- A variant of a synchronous thread pool

</div>

<img src="./images/figures/fig4_16.png" class="h-40 mx-auto" />
<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 4.16 — Fork-join parallelism</p>

---

# Fork-Join — Divide-and-Conquer Algorithm

<div class="text-left text-base leading-8">

Fork-Join is particularly well-suited for **recursive divide-and-conquer** algorithms

</div>

```text
Task(problem):
    if problem is small enough:
        solve the problem directly
    else:
        subtask1 = fork(new Task(subset of problem))
        subtask2 = fork(new Task(subset of problem))
        result1 = join(subtask1)
        result2 = join(subtask2)
        return combined results
```

<img src="./images/figures/fig4_17.png" class="h-64 mx-auto" />
<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 4.17 — Fork-join in Java</p>

---

# Fork-Join in Java — ForkJoinPool

<div class="text-left text-base leading-8">

Fork-Join framework introduced in Java 1.7:
- **ForkJoinPool**: worker thread pool
- **ForkJoinTask\<V\>**: abstract base class

</div>

```java
ForkJoinPool pool = new ForkJoinPool();
// array contains the integers to be summed
int[] array = new int[SIZE];
SumTask task = new SumTask(0, SIZE - 1, array);
int sum = pool.invoke(task);
```

<div class="text-left text-base leading-8">

**Class hierarchy** (Figure 4.19):

</div>

<img src="./images/figures/fig4_19.png" class="h-44 mx-auto" />
<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 4.19 — UML class diagram for Fork-Join</p>

---

# Fork-Join in Java — SumTask Implementation

<div class="text-left text-sm">

Figure 4.18 — Fork-join calculation using the Java API (textbook p.183)

</div>

```java
import java.util.concurrent.*;

public class SumTask extends RecursiveTask<Integer> {
    static final int THRESHOLD = 1000;
    private int begin;
    private int end;
    private int[] array;

    public SumTask(int begin, int end, int[] array) {
        this.begin = begin;
        this.end = end;
        this.array = array;
    }

    protected Integer compute() {
        if (end - begin < THRESHOLD) {
            int sum = 0;
            for (int i = begin; i <= end; i++)
                sum += array[i];
            return sum;
        } else {
            int mid = (begin + end) / 2;
            SumTask leftTask = new SumTask(begin, mid, array);
            SumTask rightTask = new SumTask(mid + 1, end, array);
            leftTask.fork();
            rightTask.fork();
            return rightTask.join() + leftTask.join();
        }
    }
}
```

---

# Fork-Join — Work Stealing

<div class="text-left text-base leading-8">

**Work Stealing algorithm**:
- Each thread maintains its own **task queue**
- When its own queue is empty → **steals tasks from another thread's queue**
- Efficiently handles thousands of tasks with just a few worker threads

**Determining THRESHOLD**:
- The criterion for deciding whether a problem is "small enough"
- In the textbook example, THRESHOLD = 1000
- In practice, the optimal value should be determined through **timing experiments**

</div>

```text
Thread 1: [Task A] [Task B] [Task C]
Thread 2: [Task D]  (empty)  (empty) ← steals Task C from Thread 1
Thread 3: [Task E] [Task F]  (empty)
Thread 4:  (empty)  (empty)  (empty) ← steals Task F from Thread 3
```

---
layout: section
---

# OpenMP

---

# OpenMP — Overview

<div class="text-left text-base leading-8">

**OpenMP (Open Multi-Processing)**:
- A parallel programming API based on **compiler directives** for C, C++, and Fortran
- Supports parallel programming in **shared-memory** environments
- Developers mark **parallel regions** with directives
- The OpenMP runtime library executes those regions in parallel

**Basic usage**:

</div>

```c
#include <omp.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    /* sequential code */

    #pragma omp parallel
    {
        printf("I am a parallel region.");
    }

    /* sequential code */
    return 0;
}
```

<div class="text-left text-base leading-8">

`#pragma omp parallel` → **automatically creates as many threads as there are processing cores**
- Dual-core → 2 threads, Quad-core → 4 threads

</div>

---

# OpenMP — parallel for

<div class="text-left text-base leading-8">

**Automatic parallelization of for loops**: `#pragma omp parallel for`

</div>

```c
#pragma omp parallel for
for (i = 0; i < N; i++) {
    c[i] = a[i] + b[i];
}
```

<div class="text-left text-base leading-8">

- **Automatically partitions** the operation of adding elements of arrays a and b into c
- OpenMP divides the iteration space among as many threads as available
- Example: N=1000, 4 threads → each thread handles 250 iterations

**Execution model**:

</div>

```text
Sequential    #pragma omp parallel for     Sequential
   code    ─→  ┌─ Thread 0: i=0..249  ─┐ ─→  code
               │  Thread 1: i=250..499  │
               │  Thread 2: i=500..749  │
               └─ Thread 3: i=750..999 ─┘
                    (implicit barrier)
```

---

# OpenMP — Additional Features

<div class="text-left text-base leading-8">

**Manually setting the number of threads**:

</div>

```c
#include <omp.h>
#include <stdio.h>

int main() {
    omp_set_num_threads(4);  // set number of threads to 4

    #pragma omp parallel
    {
        printf("Thread %d of %d\n",
            omp_get_thread_num(),
            omp_get_num_threads());
    }

    return 0;
}
```

<div class="text-left text-base leading-8">

**Data sharing attributes**:
- `shared(var)` — variable shared by all threads
- `private(var)` — each thread gets its own copy of the variable
- `reduction(+:sum)` — combines each thread's result using the specified operation

**Compilation**: `gcc -fopenmp program.c -o program`

</div>

---

# OpenMP — reduction Example

<div class="text-left text-base leading-8">

**Parallelizing array summation with reduction**:

</div>

```c
#include <omp.h>
#include <stdio.h>

int main() {
    int sum = 0;
    int a[1000];

    // Initialize array
    for (int i = 0; i < 1000; i++)
        a[i] = i + 1;

    // reduction: automatically sums partial results from each thread
    #pragma omp parallel for reduction(+:sum)
    for (int i = 0; i < 1000; i++) {
        sum += a[i];
    }

    printf("Sum = %d\n", sum);  // 500500
    return 0;
}
```

<div class="text-left text-base leading-8">

- Each thread maintains a **private copy** of sum
- At the end of the parallel region, all threads' sums are **combined using the + operation**
- Enables safe summation without race conditions

</div>

---
layout: section
---

# Grand Central Dispatch (GCD)

---

# Grand Central Dispatch — Overview

<div class="text-left text-base leading-8">

**GCD (Grand Central Dispatch)**:
- Technology developed by Apple for macOS/iOS
- A combination of **runtime library + API + language extensions**
- Similar to OpenMP, automatically manages threading details

**Core mechanism**: **Dispatch Queue**
- Tasks placed in a dispatch queue are executed by GCD, which **assigns threads from a thread pool**

**Two types of Dispatch Queues**:

| Type | Description |
|------|-------------|
| **Serial Queue** | Dequeues in FIFO order, executes only one task at a time |
| **Concurrent Queue** | Dequeues in FIFO order, executes multiple tasks simultaneously |

</div>

---

# GCD — Serial vs Concurrent Queue

<div class="text-left text-base leading-8">

**Serial Queue** (= Private Dispatch Queue):
- Each process has its own serial queue (**main queue**)
- Developers can create additional serial queues
- Useful for **tasks requiring sequential execution**

**Concurrent Queue** (= Global Dispatch Queue):
- System-wide queues
- Four **QoS (Quality of Service)** classes:

</div>

| QoS Class | Description | Example |
|-----------|-------------|---------|
| `QOS_CLASS_USER_INTERACTIVE` | UI/event handling, small tasks | Animations |
| `QOS_CLASS_USER_INITIATED` | User is waiting for results | Opening file/URL |
| `QOS_CLASS_UTILITY` | Long-running tasks, immediate result not needed | Data import |
| `QOS_CLASS_BACKGROUND` | Tasks invisible to the user | Indexing, backup |

---

# GCD — Blocks and Closures

<div class="text-left text-base leading-8">

**In C/C++/Objective-C: Block** — defines a self-contained unit of work using `^{ }` syntax

</div>

```c
// block definition
^{ printf("I am a block"); }

// create a serial queue and submit work
dispatch_queue_t q =
    dispatch_queue_create("my.queue", NULL);

// use concurrent global queue
dispatch_queue_t gq =
    dispatch_get_global_queue(
        DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

// submit asynchronous work
dispatch_async(gq, ^{
    printf("Running in background\n");
});
```

<div class="text-left text-base leading-8">

**In Swift: Closure** — same concept as Block, used without `^`

```text
let queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0)
dispatch_async(queue, { print("I am a closure.") })
```

</div>

---

# GCD — Internal Architecture

<div class="text-left text-base leading-8">

**GCD's Thread Pool Management**:
- Internally composed of **POSIX threads**
- GCD **dynamically adjusts** the pool size (expand/shrink)
- Automatically managed based on application demand and system capacity

**libdispatch library**:
- The implementation of GCD
- Open-sourced by Apple under the **Apache Commons License**
- Ported to other operating systems such as FreeBSD

</div>

```text
┌──────────────────────────────────────────┐
│  Application  (blocks / closures)        │
├──────────────────────────────────────────┤
│  Dispatch Queues (serial / concurrent)   │
├──────────────────────────────────────────┤
│  libdispatch (GCD runtime)               │
│  - Dynamic thread pool management        │
│  - QoS-based scheduling                  │
├──────────────────────────────────────────┤
│  POSIX Threads (pthreads)                │
├──────────────────────────────────────────┤
│  Kernel Threads                          │
└──────────────────────────────────────────┘
```

---
layout: section
---

# Intel Thread Building Blocks (TBB)

---

# Intel TBB — Overview

<div class="text-left text-base leading-8">

**Intel TBB (Threading Building Blocks)**:
- A **C++ template library** for designing parallel applications
- **No special compiler or language extensions required** (works with library alone)
- Developers **identify parallelizable tasks**, and the TBB task scheduler maps them to threads

**TBB Task Scheduler characteristics**:
- **Load balancing**: distributes workload evenly among threads
- **Cache aware**: prioritizes tasks that are likely to have data in cache

**Key features provided**:
- Parallel loop constructs (`parallel_for`, `parallel_reduce`)
- Atomic operations
- Mutual exclusion locking
- Concurrent data structures (hash map, queue, vector)

</div>

---

# Intel TBB — parallel_for Example

<div class="text-left text-base leading-8">

**Serial code**:

</div>

```cpp
for (int i = 0; i < n; i++) {
    apply(v[i]);
}
```

<div class="text-left text-base leading-8">

**Converted to TBB parallel_for**:

</div>

```cpp
#include <tbb/tbb.h>
using namespace tbb;

parallel_for(size_t(0), n, [=](size_t i) {
    apply(v[i]);
});
```

<div class="text-left text-base leading-8">

**How it works**:
- First two parameters: iteration space (0 to n-1)
- Third parameter: **C++ lambda function** `[=](size_t i)` → executed for each i
- TBB splits loop iterations into **chunks**
- Creates multiple tasks and assigns them to available threads
- Similar approach to Java's Fork-Join

</div>

---

# Intel TBB — parallel_reduce Example

<div class="text-left text-base leading-8">

**Implementing array summation with parallel_reduce**:

</div>

```cpp
#include <tbb/tbb.h>
using namespace tbb;

// Using parallel_reduce
int sum = parallel_reduce(
    blocked_range<int>(0, n),  // iteration space
    0,                          // initial value
    [&](const blocked_range<int>& r, int local_sum) {
        for (int i = r.begin(); i < r.end(); i++)
            local_sum += data[i];
        return local_sum;
    },
    [](int x, int y) { return x + y; }  // combine function
);
```

<div class="text-left text-base leading-8">

- Task partitioning and scheduling are **automatically handled by the library**
- Maintains load balance using the **work-stealing** algorithm
- Available in both commercial and open-source versions (Windows, Linux, macOS)

</div>

---

# Implicit Threading — Comparison of 5 Techniques

| Technique | Language/Platform | Approach | Key Feature |
|-----------|------------------|----------|-------------|
| **Thread Pool** | Java, Windows, Android | Pre-create threads in pool | General-purpose, reuse |
| **Fork-Join** | Java 1.7+ | Divide-and-conquer | Work stealing |
| **OpenMP** | C/C++/Fortran | Compiler directives | Simplest syntax |
| **GCD** | macOS/iOS (C, Swift) | Dispatch queue | QoS-based |
| **Intel TBB** | C++ | Template library | Cache aware |

<div class="text-left text-base leading-8">

**Common traits**:
- Developers only **identify tasks (parallel work units)**
- Thread creation/management/scheduling is **handled by the library/runtime**
- Improves program correctness and reduces development complexity

</div>

---
layout: section
---

# Part 2
## Threading Issues

---

# Threading Issues Overview

<div class="text-left text-lg leading-10">

Five key issues to consider in multithreaded programming:

1. **fork() and exec() semantics** — duplicating a multithreaded process
2. **Signal Handling** — delivering signals in a multithreaded environment
3. **Thread Cancellation** — canceling a running thread
4. **Thread-Local Storage (TLS)** — per-thread unique data
5. **Scheduler Activations** — communication between kernel and thread library

</div>

---

# fork() and exec() Semantics

<div class="text-left text-base leading-8">

**Problem**: What happens when one thread in a multithreaded process calls fork()?

**Two possible behaviors**:
1. **Duplicate all threads** — new process has all threads from the parent
2. **Duplicate only the calling thread** — new process is single-threaded

</div>

```text
  Parent Process                  Child Process (Option 1)
  ┌─────────────┐                ┌─────────────┐
  │ Thread 1    │   fork()       │ Thread 1'   │
  │ Thread 2 ───┼──────────→    │ Thread 2'   │  All threads duplicated
  │ Thread 3    │                │ Thread 3'   │
  └─────────────┘                └─────────────┘

  Parent Process                  Child Process (Option 2)
  ┌─────────────┐                ┌─────────────┐
  │ Thread 1    │   fork()       │             │
  │ Thread 2 ───┼──────────→    │ Thread 2'   │  Only calling thread duplicated
  │ Thread 3    │                │             │
  └─────────────┘                └─────────────┘
```

---

# fork()/exec() — Selection Criteria

<div class="text-left text-lg leading-10">

| Scenario | fork() behavior | Reason |
|----------|----------------|--------|
| exec() called immediately after fork() | Duplicate only the calling thread | Since exec() replaces the entire process, duplicating other threads is wasteful |
| Running without exec() after fork() | Duplicate all threads | The child process also needs all threads |

**When exec() is called**:
- **Replaces** the entire process (**including all threads**) with a new program
- Therefore, in the fork() + exec() pattern, duplicating only the calling thread is sufficient

**Some UNIX systems**: Provide both versions of fork()

</div>

---

# Signal Handling — Concepts and Patterns

<div class="text-left text-base leading-8">

**Signal**: A mechanism in UNIX systems to notify a process that a specific event has occurred

**Three-step pattern followed by all signals**:
1. A signal is **generated** by a specific event
2. The signal is **delivered** to the process
3. The delivered signal is **handled**

**Types of signal handlers**:
1. **Default signal handler**: default handler executed by the kernel
2. **User-defined signal handler**: custom handler defined by the programmer

| Category | Synchronous Signal | Asynchronous Signal |
|----------|-------------------|-------------------|
| **Cause** | Triggered by the process itself | Generated externally |
| **Delivered to** | The process that performed the action | Delivered to another process |
| **Examples** | Illegal memory access, div by 0 | `Ctrl+C`, timer expire |

</div>

---

# Signal Handling — Multithreaded Environment

<div class="text-left text-base leading-8">

**Single-threaded**: Signals are always delivered to that process (simple)

**Multi-threaded**: Where should the signal be delivered?

**Four options** (textbook p.189):
1. Deliver to the **thread that caused the signal** (suitable for synchronous signals)
2. Deliver to **every thread in the process** (e.g., `Ctrl+C`)
3. Deliver to **certain threads only** (threads that have not blocked the signal)
4. Designate a **dedicated signal handler thread**

</div>

```c
// UNIX: deliver signal to a specific process
kill(pid_t pid, int signal);

// Pthreads: deliver signal to a specific thread
pthread_kill(pthread_t tid, int signal);
```

<div class="text-left text-base leading-8">

**POSIX**: Each thread can specify which signals to accept/block
- Asynchronous signal → delivered to the **first thread** that does not block it
- **Windows**: Uses **APC (Asynchronous Procedure Call)** instead of signals — delivered directly to a specific thread

</div>

---

# Thread Cancellation — Concept

<div class="text-left text-base leading-8">

**Thread Cancellation**: Terminating a thread **before it has completed**

**Usage examples**:
- Multiple threads searching a DB → when one finds the result, **cancel the remaining threads**
- Pressing the Stop button in a web browser → **cancel all** image loading threads

**The thread to be canceled**: called the **target thread**

**Two cancellation approaches**:

| Approach | Description | Safety |
|----------|-------------|--------|
| **Asynchronous cancellation** | Immediate termination | Dangerous |
| **Deferred cancellation** | Terminates at a cancellation point | Safe |

</div>

---

# Asynchronous vs Deferred Cancellation

<div class="text-left text-base leading-8">

**Problems with Asynchronous Cancellation**:
- Immediate termination while resources are allocated → **resource leak**
- Cancellation during shared data update → **data inconsistency**
- The OS reclaims system resources but **cannot reclaim all resources**
- Pthreads documentation **discourages its use**

**Deferred Cancellation**:
- Only sends a "cancellation request" to the thread
- Actual termination occurs only when the target thread reaches a **cancellation point**
- Enables safe resource cleanup

</div>

```text
Asynchronous:  cancel ──→ [Immediate termination!] (no resource cleanup)

Deferred:      cancel ──→ [continues running] ──→ [cancellation point] ──→ [cleanup then terminate]
```

---

# Thread Cancellation — Pthreads API

<div class="text-left text-base leading-8">

**Three cancellation modes in Pthreads**:

| Mode | State | Type |
|------|-------|------|
| **Off** | Disabled | -- |
| **Deferred** (default) | Enabled | Deferred |
| **Asynchronous** | Enabled | Asynchronous |

</div>

```c
pthread_t tid;

/* create the thread */
pthread_create(&tid, 0, worker, NULL);

/* cancel the thread */
pthread_cancel(tid);

/* wait for the thread to terminate */
pthread_join(tid, NULL);
```

<div class="text-left text-base leading-8">

- `pthread_cancel(tid)` — only sends a cancellation **request** (does not terminate immediately!)
- Actual cancellation is determined by the target thread's cancellation mode
- If cancellation is **disabled** → the request remains in **pending** state

</div>

---

# Cancellation Points and Cleanup Handlers

<div class="text-left text-base leading-8">

**Cancellation Point**:
- In deferred cancellation, a point where the thread "can be canceled"
- Most **blocking system calls** in POSIX/standard C library are cancellation points
  - Examples: `read()`, `write()`, `sleep()`, `pthread_join()`, etc.
- The full list can be checked with `man pthreads`

</div>

```c
while (1) {
    /* do some work for awhile */
    ...

    /* check if there is a cancellation request */
    pthread_testcancel();
}
```

<div class="text-left text-base leading-8">

**pthread_testcancel()**:
- Explicitly creates a cancellation point
- If a pending cancellation request exists → thread terminates
- If none → returns normally, execution continues

**Cleanup Handler**: A function that safely releases resources (memory, files, locks, etc.) when a thread is canceled

</div>

---

# Thread Cancellation — Java

<div class="text-left text-base leading-8">

Java uses a policy **similar to Pthreads' deferred cancellation**

</div>

```java
Thread worker;
...

/* set the interruption status of the thread */
worker.interrupt();
```

<div class="text-left text-base leading-8">

`interrupt()`: Sets the target thread's **interruption status to true**

</div>

```java
while (!Thread.currentThread().isInterrupted()) {
    // ... do work ...
}
```

<div class="text-left text-base leading-8">

**isInterrupted()**: Returns the thread's interruption status as a boolean

**Comparison with Pthreads**:

| | Pthreads | Java |
|--|---------|------|
| Cancellation request | `pthread_cancel(tid)` | `worker.interrupt()` |
| Status check | `pthread_testcancel()` | `isInterrupted()` |
| Automatic cancellation points | Blocking system calls | Blocking methods (sleep, wait, join) |

**Note**: On Linux, Pthreads cancellation is internally implemented using **signals**

</div>

---

# Thread-Local Storage — Concept

<div class="text-left text-base leading-8">

**TLS (Thread-Local Storage)**: A mechanism where each thread maintains its own **copy of data**

**Why is it needed?**
- Threads fundamentally **share** process data
- However, sometimes threads need **their own unique data**
- Example: Assigning a **unique transaction ID** to each thread in a transaction processing system

**TLS vs Local Variables**:

| | Local Variables | TLS |
|--|----------------|-----|
| Lifetime | Only during the function call | **Throughout the thread's entire lifetime** |
| Visibility | Within the function scope | Accessible from all functions |
| Similarity | Allocated on the stack | Similar to **static data** (but unique per thread) |

</div>

---

# TLS — Implementation by Language

<div class="text-left text-base leading-8">

**TLS support across languages/compilers**:

</div>

```c
// GCC: __thread keyword
static __thread int threadID;

// C11 standard: _Thread_local keyword
_Thread_local int var = 0;
```

```java
// Java: ThreadLocal<T> class
ThreadLocal<Integer> value =
    ThreadLocal.withInitial(() -> 0);

value.set(42);           // set value for the current thread
int v = value.get();     // get the current thread's value
```

```csharp
// C#: [ThreadStatic] attribute
[ThreadStatic]
static int threadLocalVar;
```

<div class="text-left text-base leading-8">

**Especially useful in thread pool environments**: Each worker thread can maintain unique state even when thread creation is not directly controlled by the developer
- Pthreads: Uses `pthread_key_t` type to create/query/delete TLS keys

</div>

---

# Scheduler Activations — LWP and Upcalls

<div class="text-left text-base leading-8">

**Problem**: In Many-to-Many / Two-Level models, **communication** between the kernel and thread library is needed

**LWP (Lightweight Process)**:
- An **intermediate data structure** between user threads and kernel threads
- Appears as a **virtual processor** to the user-level thread library
- Each LWP is **attached to one kernel thread**

</div>

<img src="./images/figures/fig4_20.png" class="h-52 mx-auto" />
<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 4.20 — Lightweight process (LWP)</p>

<div class="text-left text-base leading-8">

**Number of LWPs**: A CPU-bound application needs only 1, while an I/O-intensive application needs as many as the number of concurrent blocking I/O operations

</div>

---

# Scheduler Activations — Upcall Mechanism

<div class="text-left text-base leading-8">

**Upcall**: An **event notification** from the kernel to the user-level thread library

**Workflow** (when a thread blocks):

</div>

```text
1. Thread A blocks on an I/O request
   ┌──────────────────────────────────────┐
   │ Kernel: "Thread A is blocking"       │ ──→ Upcall
   └──────────────────────────────────────┘

2. Kernel allocates a new virtual processor (LWP)

3. Thread library executes the upcall handler:
   - Saves the state of the blocking thread
   - Schedules another eligible thread on the new LWP

4. When the blocking event completes:
   ┌──────────────────────────────────────┐
   │ Kernel: "Thread A is ready"          │ ──→ Upcall
   └──────────────────────────────────────┘

5. Thread library transitions Thread A back to runnable state
```

---
layout: section
---

# Part 3
## Operating-System Examples

---

# Windows Threads — Overview

<div class="text-left text-base leading-8">

**Windows Thread implementation**:
- Uses the **One-to-One model** (each user-level thread → kernel thread)
- Threads are created with the Windows API's `CreateThread()`

**General components of a thread** (the thread's **context**):
- Thread ID (unique identifier), Register set (processor state), Program counter
- **User stack** (for user mode) + **Kernel stack** (for kernel mode)
- Private storage area (used by run-time libraries, DLLs)

**Three key data structures** (Figure 4.21):

| Structure | Location | Key Content |
|-----------|----------|------------|
| **ETHREAD** | Kernel space | Thread start address, parent process pointer, KTHREAD pointer |
| **KTHREAD** | Kernel space | Scheduling/synchronization info, kernel stack, TEB pointer |
| **TEB** | User space | Thread ID, user-mode stack, TLS array |

</div>

---

# Windows Threads — Data Structures (Figure 4.21)

<img src="./images/figures/fig4_21.png" class="h-72 mx-auto" />
<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 4.21 — Windows thread data structures</p>

<div class="text-left text-base leading-8">

- ETHREAD, KTHREAD → **accessible only by the kernel** (kernel space)
- TEB → **accessible in user mode** (user space)
- Connection: ETHREAD → KTHREAD → TEB

</div>

---

# Linux Threads — Task Concept

<div class="text-left text-base leading-8">

**Linux's unique approach**:
- **Does not distinguish** between process and thread
- All execution flows are called **"tasks"**
- Each task has its own unique **struct task_struct**

**Key point**: task_struct does not store data directly but contains **pointers to other data structures**

</div>

```text
  ┌─ task_struct ──────────────────────────┐
  │                                         │
  │  *files  ──→ [open file table]          │
  │  *signal ──→ [signal handling info]     │
  │  *mm     ──→ [memory descriptors]       │
  │  *fs     ──→ [filesystem info]          │
  │  pid, state, priority, ...              │
  │                                         │
  └─────────────────────────────────────────┘
```

---

# Linux — clone() System Call

<div class="grid grid-cols-[1fr_1fr] gap-4">
<div class="text-left text-base leading-8">

**clone()**: The system call for creating tasks in Linux
- **Flags** determine the **level of sharing** between parent and child

**Usage pattern**:

```text
fork() = clone(no flags)
  → Nothing shared (complete copy)

thread = clone(CLONE_VM | CLONE_FS
  | CLONE_FILES | CLONE_SIGHAND)
  → Almost everything shared
    (= thread creation)
```

</div>
<div>

<img src="./images/figures/fig4_22.png" class="h-56 mx-auto" />
<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 4.22 — clone() flags</p>

</div>
</div>

---

# Linux — fork() vs clone() Comparison

<div class="text-left text-base leading-8">

**When fork() is called**: New task + **copies** all of the parent's data structures

**When clone() is called (with sharing flags)**: New task + **shares** the parent's data structures via pointers

</div>

```text
  fork() — No sharing                clone() — With sharing flags
  ┌──────────┐  ┌──────────┐        ┌──────────┐  ┌──────────┐
  │ Parent   │  │ Child    │        │ Parent   │  │ Child    │
  │ task_    │  │ task_    │        │ task_    │  │ task_    │
  │ struct   │  │ struct   │        │ struct   │  │ struct   │
  │          │  │          │        │          │  │          │
  │ *mm ─→[A]│  │ *mm ─→[A']│      │ *mm ──┐  │  │ *mm ──┐  │
  │ *files→[B]│  │*files→[B']│      │ *files─┤  │  │*files─┤  │
  └──────────┘  └──────────┘        └───────┼──┘  └──────┼──┘
   Different copies                         └──→ [SHARED] ←┘
                                           Same data structures shared
```

<div class="text-left text-base leading-8">

**Extension of clone() — Containers**: By using specific flags to isolate namespaces, containers (Docker, LXC) can be created (Ch 18)

</div>

---
layout: section
---

# Lab
## OpenMP Parallel Programming Lab

---

# Lab: OpenMP Matrix Multiplication Parallelization

```c
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define N 1000

double A[N][N], B[N][N], C[N][N];

void multiply_parallel(int num_threads) {
    omp_set_num_threads(num_threads);
    double start = omp_get_wtime();

    #pragma omp parallel for collapse(2)
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            C[i][j] = 0.0;
            for (int k = 0; k < N; k++)
                C[i][j] += A[i][k] * B[k][j];
        }

    double end = omp_get_wtime();
    printf("Threads: %d, Time: %.3f sec\n",
           num_threads, end - start);
}
```

<div class="text-left text-sm">

Compile: `gcc -fopenmp matrix_mul.c -o matrix_mul`

</div>

---

# Lab: Performance Comparison by Thread Count

```c
int main() {
    srand(time(NULL));
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            A[i][j] = (double)rand() / RAND_MAX;
            B[i][j] = (double)rand() / RAND_MAX;
        }

    multiply_parallel(1);
    multiply_parallel(2);
    multiply_parallel(4);
    multiply_parallel(8);
    return 0;
}
```

<div class="text-left text-base leading-8">

**Things to observe in the experiment**:
- Change in execution time as the number of threads increases
- Comparison between Amdahl's Law and actual speedup
- Performance impact when creating more threads than the number of cores
- Cache effects and memory bandwidth bottleneck

</div>

---

# Lab: OpenMP reduction Lab

```c
#include <omp.h>
#include <stdio.h>
#define SIZE 10000000

int main() {
    double *arr = malloc(SIZE * sizeof(double));
    double sum = 0.0;

    // Initialize array
    for (int i = 0; i < SIZE; i++)
        arr[i] = 1.0 / (i + 1);

    // Sequential execution
    double start = omp_get_wtime();
    for (int i = 0; i < SIZE; i++)
        sum += arr[i];
    printf("Sequential sum: %.6f, Time: %.4f sec\n",
           sum, omp_get_wtime() - start);

    // Parallel execution (reduction)
    sum = 0.0;
    start = omp_get_wtime();
    #pragma omp parallel for reduction(+:sum)
    for (int i = 0; i < SIZE; i++)
        sum += arr[i];
    printf("Parallel   sum: %.6f, Time: %.4f sec\n",
           sum, omp_get_wtime() - start);

    free(arr);
    return 0;
}
```

---

# Summary — This Week's Key Points

<div class="text-left text-base leading-8">

**1. Implicit Threading**:
- Delegates thread creation/management to the **compiler and runtime**
- Thread Pool (reuse), Fork-Join (divide-and-conquer), OpenMP (directives)
- GCD (dispatch queue + QoS), Intel TBB (C++ templates)

**2. Threading Issues**:
- fork(): Duplicate all threads vs. duplicate only the calling thread
- Signal: **4 delivery options** in multithreaded environments
- Cancellation: **Deferred** (safe) vs. Asynchronous (dangerous)
- TLS: Each thread maintains its own unique data
- Scheduler Activations: **LWP + Upcall** mechanism

**3. OS Examples**:
- Windows: ETHREAD → KTHREAD → TEB (kernel/user space separation)
- Linux: **clone()** flags determine sharing level, no distinction between process/thread — all are **tasks**

</div>

---

# Next Week Preview (Week 6)

<div class="text-left text-lg leading-10">

**CPU Scheduling (Ch 5)**:

- Basic concepts of CPU Scheduling
  - CPU-I/O Burst Cycle
  - CPU Scheduler and Dispatcher
- Scheduling Criteria
  - CPU utilization, throughput, turnaround time, waiting time, response time
- Scheduling Algorithms
  - FCFS (First-Come, First-Served)
  - SJF (Shortest-Job-First)
  - Priority Scheduling
  - Round-Robin (RR)
- Multilevel Queue Scheduling

</div>

