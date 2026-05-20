# Lab 4: Thread-Local Storage (TLS)

## Description

Compares shared global variables (race condition) with thread-local variables (safe, private copy per thread). Also demonstrates the errno-like pattern where each thread maintains independent state via TLS.

## Build & Run

```bash
gcc -Wall -pthread -o lab4_tls lab4_tls.c
./lab4_tls
```

## Key Concepts

- `__thread` keyword: each thread gets its own copy of the variable
- Shared global: race condition when multiple threads write
- TLS global: no conflict, each thread has a private copy
- Use case: per-thread error codes (like `errno`), per-thread caches

## Theory Connection

- Ch 4.6.4: Thread-Local Storage — concept, implementations by language
