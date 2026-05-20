# Lab 2: Fixing Deadlock with Lock Ordering

## Description

This program fixes the deadlock from Lab 1 by enforcing a consistent lock
acquisition order across all threads. Both Thread 1 and Thread 2 now acquire
`mutex_A` first and `mutex_B` second. This breaks the **Circular Wait**
condition, making deadlock impossible regardless of scheduling order.

Lock ordering is the same strategy used by the xv6 kernel (e.g.,
`wait_lock` is always acquired before `p->lock`) and is described in the
textbook Ch 6.4.

**Source file**: `lab2_deadlock_fix_ordering.c`

## Build & Run

```bash
gcc -Wall -pthread -o lab2_deadlock_fix_ordering lab2_deadlock_fix_ordering.c
./lab2_deadlock_fix_ordering
```

The program should terminate normally after both threads complete 5 iterations
each.

## What to Observe

- Both threads successfully enter the critical section on every iteration.
- The output shows interleaved but deadlock-free execution: while one thread
  holds both locks, the other waits on `mutex_A` (the first lock in the order).
- The final message confirms both threads finished without deadlock.
- Compare the lock acquisition order in `thread2_func` with the original
  `deadlock_demo.c` -- the only change is the order of the two `lock` calls.

## Experiments / Try It

1. **Increase the iteration count** (e.g., from 5 to 1000) and verify that the
   program still terminates. Try it multiple times.
2. **Intentionally break the ordering** in `thread2_func` (lock B before A) and
   confirm that deadlock returns.
3. **Add a third mutex** (`mutex_C`). Define a total order (A < B < C) and add
   a third thread that needs B and C. Verify that following the order prevents
   deadlock.
4. **Measure performance**: time the program with `time ./lab2_deadlock_fix_ordering`
   and compare it against the trylock approach in Lab 3.

## Discussion Questions

1. Which of the four Coffman conditions does lock ordering break? Why does
   enforcing a total order on locks prevent circular wait?
2. In a large system with dozens of locks, what practical challenges arise when
   trying to maintain a global lock ordering? How does xv6 handle this?
3. Can lock ordering prevent deadlocks involving more than two threads and more
   than two locks? Explain with an example.
