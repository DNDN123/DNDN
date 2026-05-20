# Week 04 — Thread & Concurrency 1

## Overview
- **Textbook**: Silberschatz Ch 4 (Threads & Concurrency), Ch 6 (Synchronization Tools) basics
- **xv6 Reference**: Ch 5 (Interrupts), Ch 6.1–6.5 (Locking basics)
- **Learning Objectives**:
  - Understand the Thread concept and Multicore Programming
  - Learn Race Condition, Critical Section, Mutex, Spinlock
  - Observe race conditions and fix them with locks

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 3 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: Thread Concepts
  - Thread vs Process, Multicore Programming
  - Threading Models (Many-to-One, One-to-One, Many-to-Many)
  - Thread Libraries (Pthreads, Windows Threads, Java Threads)
  - Slides: `1_theory/04_thread_concurrency_1_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Synchronization Basics
  - Race Condition, Critical-Section Problem
  - Peterson's Solution, Hardware Support
  - Mutex Locks, Spinlock

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:40] Lab — Pthreads Thread Creation, Data Parallelism, and Speedup
  - lab1_hello_threads.c: Basic thread creation and join
  - lab2_parallel_sum.c: Data parallel array summation
  - lab3_arg_pitfall.c: Thread argument pitfall (`&i` vs `&tids[i]`)
  - lab4_speedup.c: Measuring speedup with varying thread counts (Amdahl's Law)
  - Guide: `2_lab/README.md`
- [00:40–00:50] Homework briefing: Parallel histogram (data parallelism + Amdahl's Law)

## Materials
- Theory: `1_theory/04_thread_concurrency_1_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (4 demos)
- Homework: `3_assignment/` — Parallel hash table (skeleton + tests)
- Quiz: Yes (10 min, beginning of 1st hour)
