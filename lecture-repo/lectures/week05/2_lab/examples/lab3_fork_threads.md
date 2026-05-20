# Lab 3: fork() in Multithreaded Programs

## Description

Demonstrates what happens when a multithreaded process calls `fork()`. Only the calling thread is duplicated in the child; all other threads disappear. Shows the safe pattern of `fork()` + `exec()`.

## Build & Run

```bash
gcc -Wall -pthread -o lab3_fork_threads lab3_fork_threads.c
./lab3_fork_threads
```

## Key Concepts

- POSIX `fork()` duplicates only the calling thread
- Background threads in the parent are NOT copied to the child
- Safe pattern: call `exec()` immediately after `fork()`
- Dangerous: child inherits locks that dead threads may have held

## Theory Connection

- Ch 4.6.1: fork() and exec() semantics in multithreaded programs
