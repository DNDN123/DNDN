# Week 11 — Main Memory

## Overview
- **Textbook**: Silberschatz Ch 9 (Main Memory)
- **xv6 Reference**: Ch 3 (Page Tables)
- **Learning Objectives**:
  - Understand Memory Management basics (Contiguous Allocation, Paging)
  - Learn Page Table structures (Sv39) and TLB
  - Analyze xv6 page tables directly

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 10 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: Memory Management Basics
  - Address Binding, Logical vs Physical Address
  - Contiguous Memory Allocation, Fragmentation
  - Paging basics
  - Slides: `1_theory/11_main_memory_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Page Table & TLB
  - Page Table structures (Hierarchical, Hashed, Inverted)
  - RISC-V Sv39 (3-level page table)
  - TLB (Translation Look-aside Buffer)
  - Segmentation

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:45] Lab — xv6 Page Table analysis
  - Understand xv6 Sv39 page table structure
  - vmprint.c: Implement/analyze page table print function
  - va_translate.py: Virtual address decomposition tool
  - Guide: `2_lab/README.md`
- [00:45–00:50] Wrap-up

## Materials
- Theory: `1_theory/11_main_memory_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (vmprint.c, va_translate.py)
- Homework: None
- Quiz: Yes (10 min, beginning of 1st hour)
