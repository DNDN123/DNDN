# Week 12 — Virtual Memory

## Overview
- **Textbook**: Silberschatz Ch 10 (Virtual Memory)
- **xv6 Reference**: Ch 4.6 (Page Fault, COW, Lazy Allocation)
- **Learning Objectives**:
  - Understand Demand Paging, Page Fault, and Copy-on-Write
  - Learn Page Replacement Algorithms and Thrashing
  - Design COW fork and Lazy Allocation in practice

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 11 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: Virtual Memory Basics
  - Demand Paging, Page Fault handling
  - Copy-on-Write (COW) Fork
  - Why Page Replacement is needed
  - Slides: `1_theory/12_virtual_memory_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Page Replacement & Thrashing
  - FIFO, Optimal, LRU, Clock Algorithm
  - Belady's Anomaly
  - Frame Allocation, Thrashing
  - Working-Set Model

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:45] Lab — COW & Lazy Allocation
  - cow_concept.c: COW behavior demo (Linux/macOS mmap)
  - lazy_concept.c: Lazy Allocation demo
  - Discussion: COW fork design in xv6
  - Discussion: Lazy Allocation design in xv6
  - Guide: `2_lab/README.md`
- [00:45–00:50] Wrap-up

## Materials
- Theory: `1_theory/12_virtual_memory_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (cow_concept.c, lazy_concept.c)
- Homework: None
- Quiz: Yes (10 min, beginning of 1st hour)
