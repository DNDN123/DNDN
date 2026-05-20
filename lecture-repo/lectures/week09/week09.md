# Week 09 — Synchronization

## Overview
- **Textbook**: Silberschatz Ch 6 (Synchronization Tools) review/advanced
- **xv6 Reference**: Ch 7.5–7.7 (sleep/wakeup, pipe)
- **Learning Objectives**:
  - Understand the sleep/wakeup mechanism at the xv6 code level
  - Analyze pipe implementation as a real-world synchronization example
  - Introduce the final project

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:10] **Quiz** (10 min) — covers Weeks 1–7 material (midterm review)
- [00:10–00:15] Midterm review / Announcements
- [00:15–00:50] Theory: Synchronization review + xv6 sleep/wakeup
  - Mutex, Semaphore, Monitor review
  - xv6 sleep/wakeup mechanism
  - Lost wakeup problem and solution
  - Slides: `1_theory/09_synchronization_en.md`

### 2nd Hour (Theory — Part 2 + Project Briefing) — 50 min
- [00:00–00:30] Theory: xv6 pipe implementation analysis
  - sleep/wakeup in pipe read/write
  - Synchronization pattern analysis
- [00:30–00:50] Final project briefing
  - Teams of 3–4, OS prototype development
  - Coding agents allowed
  - Week 14 presentation (Professor 15% + Peer review 15%)

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:45] Lab — sleep/wakeup & pipe analysis
  - xv6 sleep/wakeup code analysis
  - Pipe implementation analysis (piperead, pipewrite)
  - producer_consumer.c: Producer-consumer in xv6
  - wakeup_demo.c: sleep/wakeup behavior demo
  - Guide: `2_lab/README.md`
- [00:45–00:50] Wrap-up

## Materials
- Theory: `1_theory/09_synchronization_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/` (producer_consumer.c, wakeup_demo.c)
- Homework: None
- Quiz: Yes (10 min, beginning of 1st hour)
