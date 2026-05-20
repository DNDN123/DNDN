# Lab 1: Thread Pool

## Description

Implements a minimal thread pool with a fixed number of worker threads and a shared task queue. Workers wait for tasks using condition variables, execute them, then go back to waiting. Demonstrates the producer-consumer pattern used internally by thread pool frameworks.

## Build & Run

```bash
gcc -Wall -pthread -o lab1_thread_pool lab1_thread_pool.c
./lab1_thread_pool
```

## Key Concepts

- Thread pool: pre-created workers + shared task queue
- Workers block on `pthread_cond_wait` when queue is empty
- Submitter signals `not_empty` when a task is added
- Graceful shutdown with `shutdown` flag + `broadcast`

## Theory Connection

- Ch 4.5.1: Thread Pools — benefits, concept, implementation
