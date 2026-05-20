# Lab 3: Avoiding Deadlock with trylock + Back-off

## Description

This program avoids deadlock using `pthread_mutex_trylock` and a back-off
strategy. The two threads still attempt to lock mutexes in **opposite order**
(Thread 1: A then B; Thread 2: B then A), but instead of blocking on the
second lock, each thread uses `trylock`. If `trylock` fails, the thread
releases the lock it already holds (back-off), waits a random amount of time,
and retries. This breaks the **Hold & Wait** condition.

**Source file**: `lab3_deadlock_fix_trylock.c`

## Build & Run

```bash
gcc -Wall -pthread -o lab3_deadlock_fix_trylock lab3_deadlock_fix_trylock.c
./lab3_deadlock_fix_trylock
```

The program should terminate normally. Each thread reports how many attempts
it needed.

## What to Observe

- Look for "trylock failed! -> back-off" messages in the output. These show
  moments where a thread could not acquire the second lock and had to release
  and retry.
- Each thread prints the total number of attempts it took. The count is
  typically greater than 1, showing that back-off occurred.
- Despite the opposite locking order, the program completes without deadlock.
- The random wait (`rand() % 50000` microseconds) helps the two threads
  desynchronize and avoid repeated collisions.

## Experiments / Try It

1. **Remove the random wait** (replace `usleep(rand() % 50000)` with nothing
   or a fixed delay). Does the program still finish? Does the number of
   attempts increase? Can you trigger a livelock?
2. **Replace `trylock` with regular `lock`** in one thread and observe that the
   deadlock returns immediately.
3. **Increase contention**: wrap the main logic in a loop so each thread must
   succeed 10 times. Track the total number of back-off attempts.
4. **Compare with Lab 2**: run both solutions 100 times with `time` and compare
   the total elapsed time. Which approach has less overhead?

## Discussion Questions

1. Which of the four Coffman conditions does the trylock + back-off strategy
   break? Explain why releasing the held lock on failure prevents deadlock.
2. What is livelock and how does it differ from deadlock? How does the random
   back-off delay help mitigate livelock?
3. Why is lock ordering generally preferred over trylock + back-off in
   production systems? In what situations might trylock be the better choice?
4. The program uses `srand(42)` for reproducibility. What would change if you
   used `srand(time(NULL))` instead?
