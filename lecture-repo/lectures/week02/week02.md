# Week 02 — Process 1

## Overview
- **Textbook**: Silberschatz Ch 3 (Processes)
- **xv6 Reference**: Ch 1 (Operating System Interfaces)
- **Learning Objectives**:
  - Understand the Process concept (State, PCB, Context Switch)
  - Learn Process Scheduling and Operations on Processes
  - Practice system calls: fork, exec, pipe

## Class Schedule

### 1st Hour (Theory — Part 1) — 50 min
- [00:00–00:05] Last week review / Announcements
- [00:05–00:50] Theory: Process Concept
  - Process State (New, Ready, Running, Waiting, Terminated)
  - PCB (Process Control Block)
  - Context Switch
  - Slides: `1_theory/02_process_1_en.md`

### 2nd Hour (Theory — Part 2) — 50 min
- [00:00–00:50] Theory: Process Scheduling & Operations
  - Scheduling Queues (Ready Queue, Wait Queue)
  - Schedulers (Short-term, Long-term)
  - Process Creation (fork), Process Termination (exit, wait)
  - IPC: Shared Memory, Message Passing

### 3rd Hour (Lab) — 50 min
- [00:00–00:05] Lab objectives
- [00:05–00:40] Lab — fork/exec/pipe hands-on
  - fork_basic.c: Observe fork + wait behavior
  - exec_example.c: Practice exec family
  - pipe_example.c: Inter-process communication via pipe
  - redirect.c: I/O redirection with dup2
  - Guide: `2_lab/README.md`
- [00:40–00:50] Homework briefing: Ping-pong + Minishell

## Materials
- Theory: `1_theory/02_process_1_en.md`
- Lab: `2_lab/README.md`, `2_lab/examples/`
- Environment Setup: `2_lab/setup_xv6_env.md` — xv6 build environment (macOS / Ubuntu / Windows WSL2)
- Homework: `3_assignment/` — Ping-pong, Minishell (skeleton + tests)
- Quiz: None
