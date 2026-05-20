# Lab 1: Observing a Deadlock

## Description

This program demonstrates a classic deadlock scenario using two POSIX threads
and two mutexes. Thread 1 acquires `mutex_A` then tries to acquire `mutex_B`,
while Thread 2 acquires `mutex_B` then tries to acquire `mutex_A`. Because the
two threads lock in opposite order, all four Coffman conditions are satisfied
and the program hangs indefinitely.

**Source file**: `lab1_deadlock_demo.c`

## Build & Run

```bash
gcc -Wall -pthread -o lab1_deadlock_demo lab1_deadlock_demo.c
./lab1_deadlock_demo
```

The program will hang once the deadlock occurs. Terminate it with `Ctrl+C`.

## What to Observe

- The program prints lock-acquisition messages from both threads.
- At some point both threads report "trying to lock" the second mutex and no
  further output appears -- this is the deadlock.
- The final "both threads finished normally" message is **never** printed.
- Note the last messages: Thread 1 is stuck waiting for `mutex_B` and Thread 2
  is stuck waiting for `mutex_A`.

## Experiments / Try It

1. **Remove the `usleep` calls** and run the program several times. Does the
   deadlock still occur every time, or only sometimes? Why?
2. **Add a third mutex** (`mutex_C`) and a third thread that locks in yet
   another order. Does the program still deadlock? How does the behavior change?
3. **Use `strace` (Linux) or `dtruss` (macOS)** to trace the program while it
   is stuck. Identify the `futex` / `__psynch_mutexwait` system calls that show
   each thread blocking.

## Discussion Questions

1. Identify how each of the four deadlock conditions (Mutual Exclusion, Hold &
   Wait, No Preemption, Circular Wait) is satisfied in this program.
2. If you removed the `usleep` delay, the deadlock might not reproduce on every
   run. Does this mean the bug is gone? Explain the difference between a
   guaranteed deadlock and a race-dependent one.
3. In a real application, how would you detect that a deadlock has occurred at
   runtime? What tools or techniques could you use?
