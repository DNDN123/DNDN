# Week 06 — CPU Scheduling 1

## Overview
- **Textbook**: Silberschatz Ch 5 (CPU Scheduling) first half
- **xv6 Reference**: Ch 7.1–7.6 (Scheduling, Context Switching)
- **Learning Objectives**:
  - Understand CPU Scheduling basics and Scheduling Criteria
  - Learn major scheduling algorithms (FCFS, SJF, Priority, RR)
  - Observe the xv6 scheduler in action

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 5 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: CPU Scheduling Basics
  - CPU-I/O Burst Cycle, CPU Scheduler, Preemptive vs Non-preemptive
  - Scheduling Criteria (CPU utilization, throughput, turnaround, waiting, response time)
  - Slides: `1_theory/06_cpu_scheduling_1_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Scheduling Algorithms
  - FCFS (First-Come, First-Served)
  - SJF (Shortest-Job-First), SRTF
  - Priority Scheduling (+ aging)
  - Round-Robin
  - Multilevel Queue

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:40] Lab — Observing the xv6 scheduler
  - Apply scheduler_trace.patch → scheduling log output
  - Run multiple processes concurrently → observe round-robin
  - Analyze swtch.S: context switch flow
  - Analyze scheduler() function
  - Guide: `2_lab/README.md`
- [00:40–00:50] Homework briefing: User-level Thread library

## Materials
- Theory: `1_theory/06_cpu_scheduling_1_en.md`
- Lab: `2_lab/README.md`, `2_lab/scheduler_trace.patch`
- Homework: `3_assignment/` — User-level Thread (skeleton + 12 tests)
- Quiz: Yes (10 min, beginning of 1st hour)
