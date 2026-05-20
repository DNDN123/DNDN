# Week 10 Lab: Deadlocks

## Learning Objectives

By the end of this lab, you will be able to:

1. Explain the 4 necessary conditions for deadlock and identify how each condition is satisfied in real code
2. Directly observe a deadlock situation using pthread mutexes and analyze its cause
3. Implement deadlock resolution/avoidance using lock ordering and trylock
4. Analyze how lock ordering is applied in the xv6 kernel source code
5. Understand the relationship between process kill and locking in xv6

**Textbook coverage**: xv6 book Ch 6.4 (Lock ordering), Ch 7.9 (Code: Kill)

---

## Background Knowledge: What is a Deadlock?

A **deadlock** is a state where two or more processes (or threads) are each waiting for a resource held by the other, making it impossible for any of them to make progress.

### The 4 Necessary Conditions for Deadlock (Coffman Conditions)

For a deadlock to occur, the following 4 conditions must hold **simultaneously**:

| Condition | Description |
|-----------|-------------|
| **Mutual Exclusion** | A resource can only be used by one process at a time |
| **Hold & Wait** | A process holds a resource while waiting for another resource |
| **No Preemption** | A resource held by a process cannot be forcibly taken away |
| **Circular Wait** | Processes form a circular chain, each waiting for a resource held by the next |

Breaking any one of these conditions can prevent deadlock.

### Deadlock Example (Conceptual Diagram)

```
Thread 1                    Thread 2
   |                           |
   | lock(A) -- success        | lock(B) -- success
   |                           |
   | lock(B) -- waiting...     | lock(A) -- waiting...
   |     ^                     |     ^
   |     |_____________________|_____|
   |                           |
   +-- Waiting for Thread 2    +-- Waiting for Thread 1
       to release B                to release A

   ==> No progress possible (Deadlock!)
```

---

## Exercise 1: Observing a Deadlock (~10 min)

### Goal
Directly observe a program hanging due to an actual deadlock.

### Code Analysis

Open and read the code in `examples/lab1_deadlock_demo.c`.

```c
/* Thread 1: Lock A first, then try to lock B */
void *thread1_func(void *arg)
{
    pthread_mutex_lock(&mutex_A);    // (1) Lock A
    usleep(100000);                  // (2) Brief wait
    pthread_mutex_lock(&mutex_B);    // (3) Try to lock B -> blocks!
    ...
}

/* Thread 2: Lock B first, then try to lock A (reverse order!) */
void *thread2_func(void *arg)
{
    pthread_mutex_lock(&mutex_B);    // (1) Lock B
    usleep(100000);                  // (2) Brief wait
    pthread_mutex_lock(&mutex_A);    // (3) Try to lock A -> blocks!
    ...
}
```

### Lab Steps

**Step 1: Compile**

```bash
cd practice/week10/lab/examples
gcc -Wall -pthread -o lab1_deadlock_demo lab1_deadlock_demo.c
```

**Step 2: Run**

```bash
./lab1_deadlock_demo
```

Confirm that the program hangs. Wait about 10 seconds and then terminate with `Ctrl+C`.

**Step 3: Analysis**

> **Question 1**: What was the last message printed in the program output? Why did it stop progressing after that?
>
> **Question 2**: Explain how each of the 4 deadlock conditions is satisfied in this situation.

<details>
<summary>Show Answer</summary>

**Question 1 Answer**:
- Thread 1 is stuck at "Attempting to lock mutex_B..."
- Thread 2 is stuck at "Attempting to lock mutex_A..."
- Thread 1 is waiting for mutex_B, and Thread 2 is waiting for mutex_A, each waiting for a lock held by the other, so neither can ever make progress.

**Question 2 Answer**:
1. **Mutual Exclusion**: A mutex can only be locked by one thread at a time
2. **Hold & Wait**: Thread 1 holds mutex_A while waiting for mutex_B, and Thread 2 holds mutex_B while waiting for mutex_A
3. **No Preemption**: `pthread_mutex_lock` cannot forcibly take a lock; it waits until the other side releases it
4. **Circular Wait**: Thread 1 -> mutex_B (held by Thread 2) -> Thread 2 -> mutex_A (held by Thread 1) -> Thread 1... a cycle is formed

</details>

---

## Exercise 2: Resolving Deadlock with Lock Ordering (~10 min)

### Goal
Prevent deadlock by breaking the **Circular Wait** condition.

### Key Idea

If all threads acquire locks in the **same order**, circular wait cannot occur:

```
Rule: Always lock mutex_A first, then mutex_B

Thread 1: lock(A) -> lock(B)    (same as before)
Thread 2: lock(A) -> lock(B)    (order changed!)
```

This way, while Thread 2 is waiting to acquire A, Thread 1 can use both A and B, release them, and then Thread 2 can proceed.

### Lab Steps

**Step 1: Code Analysis**

Open `examples/lab2_deadlock_fix_ordering.c` and compare the two thread functions.

Key change (in `thread2_func`):
```c
/* [Before fix - deadlock occurs] */
pthread_mutex_lock(&mutex_B);  // B first
pthread_mutex_lock(&mutex_A);  // A second

/* [After fix - lock ordering applied] */
pthread_mutex_lock(&mutex_A);  // A first (unified order!)
pthread_mutex_lock(&mutex_B);  // B second
```

**Step 2: Compile and Run**

```bash
gcc -Wall -pthread -o lab2_deadlock_fix_ordering lab2_deadlock_fix_ordering.c
./lab2_deadlock_fix_ordering
```

**Step 3: Verify**

Confirm that the program terminates normally. Both threads iterate 5 times each, and all 10 critical section entries succeed.

> **Question 3**: Which of the 4 deadlock conditions does lock ordering break?

<details>
<summary>Show Answer</summary>

It breaks **Circular Wait**.

If all threads acquire locks in the same order (A -> B), while one thread holds A and waits for B, the other thread cannot even acquire A, so a cycle cannot form.

This is the strategy adopted by the xv6 kernel, and it is what is described in textbook Ch 6.4.

</details>

---

## Exercise 3: Avoiding Deadlock with pthread_mutex_trylock (~10 min)

### Goal
Avoid deadlock by breaking the **Hold & Wait** condition.

### Key Idea

`pthread_mutex_trylock` **does not block and returns failure** if the lock cannot be acquired immediately. On failure, releasing already-held locks (back-off) breaks the Hold & Wait condition:

```
Thread 1:
  lock(A)
  if trylock(B) fails:
      unlock(A)        <- Release what you hold! (Destroys Hold & Wait)
      Brief wait
      Retry from the beginning
```

### Lab Steps

**Step 1: Code Analysis**

Read the core logic of `thread1_func` in `examples/lab3_deadlock_fix_trylock.c`:

```c
while (!success) {
    pthread_mutex_lock(&mutex_A);           // Lock A

    if (pthread_mutex_trylock(&mutex_B) == 0) {
        /* Success: enter critical section */
        success = 1;
        ...
    } else {
        /* Failure: back-off */
        pthread_mutex_unlock(&mutex_A);     // Release A!
        usleep(rand() % 50000);             // Random wait
    }
}
```

**Step 2: Compile and Run**

```bash
gcc -Wall -pthread -o lab3_deadlock_fix_trylock lab3_deadlock_fix_trylock.c
./lab3_deadlock_fix_trylock
```

**Step 3: Output Analysis**

Look for "trylock failed! -> back-off" messages in the output. Observe the pattern where trylock fails on some attempts, and after back-off, succeeds on retry.

> **Question 4**: Which of the 4 deadlock conditions does the trylock approach break?

> **Question 5**: What are the disadvantages of the trylock + back-off approach?

<details>
<summary>Show Answer</summary>

**Question 4 Answer**: It breaks **Hold & Wait**. When trylock fails, the already-held lock is released, so the situation of "holding a resource while waiting for another" does not occur.

**Question 5 Answer**:
- **Livelock** risk: Two threads may continuously repeat trylock failure -> back-off -> retry without making any real progress. Random wait times are used to mitigate this.
- **Performance overhead**: Unnecessary lock/unlock repetition and wait times occur.
- **Complexity**: The code is more complex compared to lock ordering.

Therefore, lock ordering is the preferred method when possible.

</details>

---

## Exercise 4: Analyzing Lock Ordering in the xv6 Kernel (~15 min)

### Goal
Analyze how lock ordering is applied in the xv6 kernel source code to prevent deadlock.

### 4-1. Major Locks in xv6

The xv6 kernel has the following major locks:

| Lock | Location | Purpose |
|------|----------|---------|
| `p->lock` | `kernel/proc.h` | Per-process lock (protects state, killed, pid, etc.) |
| `wait_lock` | `kernel/proc.c` | Protects parent-child relationship (`p->parent`) |
| `bcache.lock` | `kernel/bio.c` | Buffer cache management |
| `ftable.lock` | `kernel/file.c` | File table management |
| `pi->lock` | `kernel/pipe.c` | Per-pipe lock |
| `cons.lock` | `kernel/console.c` | Console I/O |
| `tickslock` | `kernel/trap.c` | System timer |

### 4-2. Key Lock Ordering Rule: `wait_lock` -> `p->lock`

Read the comment at lines 26-27 of `kernel/proc.c`:

```c
// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;
```

**Rule: `wait_lock` must always be acquired before `p->lock`.**

Let's verify that this rule is actually followed in the code.

### Lab: Analyzing the `kexit()` Function

Read the `kexit()` function (line 327~) in `kernel/proc.c`:

```c
void
kexit(int status)
{
    struct proc *p = myproc();
    ...
    acquire(&wait_lock);       // (1) Acquire wait_lock first

    reparent(p);               // Re-parent children to init
    wakeup(p->parent);         // Wake up parent

    acquire(&p->lock);         // (2) Then acquire p->lock

    p->xstate = status;
    p->state = ZOMBIE;

    release(&wait_lock);       // Release wait_lock

    sched();                   // Switch to scheduler
    ...
}
```

> **Question 6**: Why must `wait_lock` be acquired before `p->lock` in `kexit()`? What problem could occur if the order were reversed?

<details>
<summary>Show Answer</summary>

Looking at the `kwait()` function:
```c
int kwait(uint64 addr)
{
    acquire(&wait_lock);        // (1) wait_lock first
    ...
    acquire(&pp->lock);         // (2) Child's p->lock second
    ...
}
```

`kwait()` also acquires locks in the order `wait_lock` -> `p->lock`. If `kexit()` acquired them in the order `p->lock` -> `wait_lock`:

- `kexit()`: holds `p->lock` -> waits for `wait_lock`
- `kwait()`: holds `wait_lock` -> waits for `pp->lock`

In this case, a Circular Wait would form, potentially causing a deadlock.

</details>

### 4-3. Lock Ordering in the `sleep()` Function

Read the `sleep()` function (line 543~) in `kernel/proc.c`:

```c
void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    acquire(&p->lock);    // (1) Acquire p->lock
    release(lk);          // (2) Release condition lock

    p->chan = chan;
    p->state = SLEEPING;

    sched();              // Switch to scheduler

    p->chan = 0;

    release(&p->lock);    // Release p->lock
    acquire(lk);          // Re-acquire condition lock
}
```

> **Question 7**: Why does `sleep()` acquire `p->lock` first and then release the condition lock (`lk`)? What problem would occur if the order were reversed (releasing `lk` first, then acquiring `p->lock`)?

<details>
<summary>Show Answer</summary>

If `release(lk)` were done before `acquire(&p->lock)`, another process could call `wakeup()` between the two operations.

```
sleep():                           wakeup():
  release(lk)                        acquire(&p->lock)
  // ---- wakeup called here! ----   // p->state == SLEEPING? Not yet!
  acquire(&p->lock)                  release(&p->lock)
  p->state = SLEEPING
  sched()    // <- Missed the wakeup! Sleeps forever
```

By acquiring `p->lock` first, `wakeup()` cannot acquire `p->lock` and must wait. Only after `sleep()` transitions to the SLEEPING state can `wakeup()` check the state, ensuring that the wakeup signal is not missed. This prevents the "lost wakeup" problem.

</details>

### 4-4. Process Kill and Locking Issues (Ch 7.9)

xv6's `kkill()` function (`kernel/proc.c` line 593~) is designed very carefully:

```c
int
kkill(int pid)
{
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->pid == pid){
            p->killed = 1;
            if(p->state == SLEEPING){
                p->state = RUNNABLE;     // Wake up sleeping process
            }
            release(&p->lock);
            return 0;
        }
        release(&p->lock);
    }
    return -1;
}
```

`kkill()` does **not immediately terminate** the target process. Instead, it only sets the flag `p->killed = 1`.

> **Question 8**: Explain from a locking perspective why `kkill()` does not immediately terminate the target process.

<details>
<summary>Show Answer</summary>

The target process may be executing kernel code and holding multiple locks. For example:
- Holding a file system lock while waiting for disk I/O
- Holding a pipe lock while waiting for data
- Holding `wait_lock` while performing an operation

If the process were immediately terminated in this state, **those locks would never be released**, potentially causing other processes to deadlock.

Therefore, `kkill()` only sets a flag, and the target process safely terminates itself at **the point when it returns to user space** (after trap handling). This can be seen in the `usertrap()` function (`kernel/trap.c`):

```c
uint64 usertrap(void)
{
    ...
    if(killed(p))        // Check before system call
        kexit(-1);
    ...
    if(killed(p))        // Check after system call
        kexit(-1);
    ...
}
```

Functions like `piperead()` and `consoleread()` also perform `killed()` checks:

```c
// pipe.c - piperead()
while(pi->nread == pi->nwrite && pi->writeopen){
    if(killed(pr)){
        release(&pi->lock);    // Safely release pipe lock
        return -1;
    }
    sleep(&pi->nread, &pi->lock);
}
```

This way, the process cleans up all locks it holds before terminating, preventing deadlock.

</details>

### 4-5. Deadlock Prevention in acquire()

Looking at the `acquire()` function in `kernel/spinlock.c`:

```c
void
acquire(struct spinlock *lk)
{
    push_off();             // Disable interrupts!
    if(holding(lk))
        panic("acquire");   // Panic if trying to acquire the same lock twice!
    ...
}
```

> **Question 9**: Why does `acquire()` disable interrupts? What kind of deadlock could occur if interrupts were not disabled?

<details>
<summary>Show Answer</summary>

If a spinlock is acquired with interrupts enabled:

1. CPU acquires spinlock A
2. Timer interrupt fires -> interrupt handler executes
3. Interrupt handler tries to acquire spinlock A
4. Since the **same CPU** already holds A, it spins forever -> Deadlock!

This is a deadlock that can occur even on a single CPU. Disabling interrupts with `push_off()` prevents the interrupt handler from executing while the lock is held, avoiding this problem.

Additionally, the `holding(lk)` check detects programming errors where the same lock is acquired recursively.

</details>

---

## Summary and Key Takeaways

### Deadlock 4 Conditions and Solutions

| Condition | Solution | Example in this lab |
|-----------|----------|---------------------|
| Mutual Exclusion | (Generally cannot be removed) | - |
| Hold & Wait | trylock + back-off | Exercise 3: `lab3_deadlock_fix_trylock.c` |
| No Preemption | (Generally cannot be removed) | - |
| Circular Wait | **Lock ordering** | Exercise 2: `lab2_deadlock_fix_ordering.c` |

### xv6's Deadlock Prevention Strategies

1. **Lock ordering**: Define a global lock order and always follow it
   - Example: `wait_lock` -> `p->lock`
   - Example: `bcache.lock` -> `buf->lock` (spinlock -> sleeplock)

2. **Disabling interrupts**: Disable interrupts while holding a spinlock
   - `acquire()` -> `push_off()` -> `intr_off()`

3. **Deferred kill**: Do not immediately terminate a process; only set a flag
   - Terminate at a point where locks can be safely cleaned up

4. **Holding check**: Panic if trying to acquire the same lock twice
   - `if(holding(lk)) panic("acquire");`

### Key Lessons

- **Lock ordering is the most practical and widely used deadlock prevention technique**.
- The xv6 kernel documents lock ordering through code comments and conventions.
- In complex situations like process kill, it is important to **defer actions until a point where locks can be safely cleaned up**.
- These same principles apply in real operating systems (Linux, etc.) as well.
