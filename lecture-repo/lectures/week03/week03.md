# Week 03 — Process 2

## Overview
- **Textbook**: Silberschatz Ch 2 (OS Structures), Ch 3 (Processes) advanced
- **xv6 Reference**: Ch 2 (OS Organization), Ch 4.1–4.4 (Traps and System Calls)
- **Learning Objectives**:
  - Compare OS structures (Monolithic, Microkernel, Hybrid)
  - Understand the Trap/System Call flow
  - Trace the syscall path in xv6 source code

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Week 2 material
- [00:10–00:15] Last week review / Announcements
- [00:15–00:50] Theory: OS Structures & Trap/System Call
  - OS Services, System Call Interface
  - System Call types (Process, File, Device, Info, Communication, Protection)
  - Linkers and Loaders
  - Slides: `1_theory/03_process_2_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Comparing OS Structures
  - Monolithic, Layered, Microkernel, LKM, Hybrid
  - System Boot (BIOS vs UEFI)

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:40] Lab — Exploring xv6 source code
  - xv6 directory structure (kernel/, user/)
  - Trace system call path: user/ → usys.S → ecall → syscall.c
  - Analyze struct proc
  - Guide: `2_lab/README.md`
- [00:40–00:50] Homework briefing: Add a trace system call

## Materials
- Theory: `1_theory/03_process_2_en.md`
- Lab: `2_lab/README.md`
- Homework: `3_assignment/` — trace syscall (patch + tests)
- Quiz: Yes (10 min, beginning of 1st hour)
