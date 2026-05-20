# Week 05 — Thread & Concurrency 2

## Overview
- **Textbook**: Silberschatz Ch 4 (Sections 4.5–4.7)
- **Learning Objectives**:
  - Understand Implicit Threading techniques (Thread Pool, Fork-Join, OpenMP)
  - Learn Threading Issues (fork/exec, signals, cancellation, TLS)
  - Implement a thread pool and use OpenMP directives

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 4 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: Implicit Threading
  - Thread Pools, Fork-Join, OpenMP, GCD, TBB
  - Slides: `1_theory/05_thread_concurrency_2_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Threading Issues
  - fork() and exec() Semantics
  - Signal Handling, Thread Cancellation
  - Thread-Local Storage (TLS)
  - Scheduler Activations
  - Windows/Linux Thread Implementation

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:40] Lab — Implicit Threading and Threading Issues
  - lab1_thread_pool.c: Thread pool with work queue and worker threads
  - lab2_openmp_parallel.c: OpenMP parallel for and reduction
  - lab3_fork_threads.c: fork() behavior in multithreaded programs
  - lab4_tls.c: Thread-Local Storage demonstration
  - Guide: `2_lab/README.md`
- [00:40–00:50] Homework briefing

## Materials
- Theory: `1_theory/05_thread_concurrency_2_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (4 examples)
- Quiz: Yes (10 min, beginning of 1st hour)
