# Lab 1: Hello Threads

## Description

Creates multiple threads using `pthread_create()`. Each thread prints a greeting message with its thread ID. The main thread waits for all threads to finish with `pthread_join()`. Run multiple times to observe that the print order is non-deterministic — it depends on OS scheduling.

## Build & Run

```bash
gcc -Wall -pthread -o lab1_hello_threads lab1_hello_threads.c
./lab1_hello_threads
```

Optional argument: `./lab1_hello_threads [num_threads]`

```bash
./lab1_hello_threads 1
./lab1_hello_threads 8
```

## Key Concepts

- `pthread_create()`: create a new thread
- `pthread_join()`: wait for a thread to terminate
- Threads run concurrently and may execute in any order
- Without `pthread_join()`, the main thread may exit before child threads finish

## Theory Connection

- Ch 4.4: Thread Libraries — Pthreads API
- Ch 4.1: Thread concept — each thread has its own PC, registers, and stack
