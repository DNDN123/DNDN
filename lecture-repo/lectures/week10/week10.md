# Week 10 — Deadlocks

## Overview
- **Textbook**: Silberschatz Ch 8 (Deadlocks)
- **xv6 Reference**: Ch 6.4 (Deadlock), Ch 7.9 (Process Locking)
- **Learning Objectives**:
  - Understand the 4 necessary conditions for deadlock
  - Learn Deadlock Prevention, Avoidance, Detection, and Recovery strategies
  - Practice lock ordering and trylock as real-world solutions

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 9 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: Deadlock Concepts
  - 4 Necessary Conditions (Mutual Exclusion, Hold & Wait, No Preemption, Circular Wait)
  - Resource Allocation Graph
  - Deadlock Prevention (breaking each condition)
  - Slides: `1_theory/10_deadlocks_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Deadlock Resolution Strategies
  - Deadlock Avoidance (Banker's Algorithm)
  - Deadlock Detection & Recovery
  - Real-world approach (most systems ignore deadlocks)

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:45] Lab — Deadlock analysis & resolution
  - deadlock_demo.c: Observe deadlock occurrence
  - deadlock_fix_ordering.c: Fix with lock ordering
  - deadlock_fix_trylock.c: Avoidance with trylock
  - Guide: `2_lab/README.md`
- [00:45–00:50] Wrap-up

## Materials
- Theory: `1_theory/10_deadlocks_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (3 demos)
- Homework: None
- Quiz: Yes (10 min, beginning of 1st hour)
