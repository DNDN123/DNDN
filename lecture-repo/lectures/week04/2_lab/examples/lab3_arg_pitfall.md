# Lab 3: Thread Argument Pitfall

## Description

Demonstrates a common bug when passing arguments to threads. Passing `&i` (the loop variable's address) causes all threads to share the same pointer, leading to incorrect or duplicated IDs. The fix is to use a separate array (`tids[i]`) so each thread gets its own stable value.

## Build & Run

```bash
gcc -Wall -pthread -o lab3_arg_pitfall lab3_arg_pitfall.c
./lab3_arg_pitfall
```

Run multiple times to observe the buggy version's non-deterministic output.

## Key Concepts

- Buggy: `pthread_create(&t[i], NULL, func, &i)` — all threads share `&i`
- Correct: `pthread_create(&t[i], NULL, func, &tids[i])` — each thread gets its own copy
- This is a data dependency issue: the loop variable changes before threads read it

## Theory Connection

- Ch 4.4: Pthreads examples — the textbook explicitly shows why a `thread_ids[]` array is needed
