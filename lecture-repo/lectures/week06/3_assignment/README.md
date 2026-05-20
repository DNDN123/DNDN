# Week 6 Assignment: xv6 Priority Scheduler

> **Deadline**: Before the next class
> **Based on**: Week 6 Theory (CPU Scheduling) + Lab (Context Switching in xv6)
> **Difficulty**: ★★★☆☆
> **Environment**: xv6-riscv (QEMU + RISC-V toolchain required — see [Week 2 setup guide](../../week02/2_lab/setup_xv6_env.md))

---

## Overview

In this assignment, you will modify xv6's scheduler to implement **priority-based scheduling**.

Currently, xv6 uses a simple **round-robin** scheduler that scans the `proc[]` array linearly and runs the first RUNNABLE process it finds (as you observed in Lab Exercise 2–3). You will change this so the scheduler always picks the **highest-priority** RUNNABLE process.

You will also add two system calls — `setpriority` and `getpriority` — so that user programs can change and query process priorities at runtime.

---

## Background

### From Theory: Scheduling Algorithms

In this week's lecture, we learned several scheduling algorithms:

- **FCFS**: Select the process that arrived first — simple but suffers from the **convoy effect**
- **SJF**: Select the process with the shortest next CPU burst — optimal for average waiting time, but requires knowing burst length in advance
- **RR**: Give each process a fixed time quantum in circular order — fair but higher turnaround time

**SJF is actually a special case of priority scheduling** where the priority is the inverse of the predicted CPU burst. The general idea is: assign each process a numeric **priority**, and the scheduler always runs the highest-priority RUNNABLE process.

### From Lab: xv6 Scheduler Internals

In the lab, you analyzed xv6's scheduler:

- **Exercise 1**: `swtch.S` saves/restores callee-saved registers to switch between contexts
- **Exercise 2**: `scheduler()` in `kernel/proc.c` linearly scans `proc[]` for RUNNABLE processes
- **Exercise 3**: With multiple `spin` processes, you observed round-robin interleaving

The key code you traced:

```c
// Current scheduler — simple round-robin
for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNABLE) {
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
    }
    release(&p->lock);
}
```

Your task is to replace this with priority-based selection.

---

## Functional Specification

### Priority Rules

| Property | Value |
|----------|-------|
| Range | 0 (highest) to 20 (lowest) |
| Default | 10 |
| Tie-breaking | Round-robin among same-priority processes |

### System Calls

#### `setpriority(int pid, int priority)`

Sets the priority of process `pid` to `priority`.

- Returns `0` on success
- Returns `-1` if `pid` not found or `priority` is out of range (0–20)
- A process can set its own priority or its children's priority

#### `getpriority(int pid)`

Returns the priority of process `pid`.

- Returns the priority value (0–20) on success
- Returns `-1` if `pid` not found

### Scheduler Behavior

Instead of running the first RUNNABLE process found, the scheduler must:

1. Scan **all** RUNNABLE processes
2. Select the one with the **lowest priority number** (= highest priority)
3. If multiple processes share the same priority, use **round-robin** among them

---

## Step-by-Step Implementation Guide

### List of Files to Modify

| File | Modification |
|------|-------------|
| `kernel/proc.h` | Add `priority` field to `struct proc` |
| `kernel/proc.c` | Initialize priority in `allocproc()`, reset in `freeproc()`, copy in `kfork()`, modify `scheduler()` |
| `kernel/syscall.h` | Add `SYS_setpriority` (22) and `SYS_getpriority` (23) |
| `kernel/sysproc.c` | Implement `sys_setpriority()` and `sys_getpriority()` |
| `kernel/syscall.c` | Add to syscalls table |
| `user/usys.pl` | Add entries for both syscalls |
| `user/user.h` | Add function declarations |
| `Makefile` | Add test program to UPROGS |

### Step 1: Add Priority Field to `struct proc`

In `kernel/proc.h`, add a `priority` field:

```c
struct proc {
  // ... existing fields ...
  char name[16];               // Process name (debugging)
  int priority;                // Scheduling priority (0=highest, 20=lowest)
};
```

### Step 2: Initialize, Reset, and Inherit Priority

**(a)** In `allocproc()` in `kernel/proc.c`, set the default priority:

```c
// After other initialization (e.g., setting pid)
// TODO: Set default priority to 10
```

**(b)** In `freeproc()` in `kernel/proc.c`, reset priority:

```c
// TODO: Reset priority to 0
```

**(c)** In `kfork()` in `kernel/proc.c`, copy priority from parent to child:

```c
// After: safestrcpy(np->name, p->name, sizeof(p->name));
// TODO: Copy parent's priority to child
```

### Step 3: Register the System Call Numbers

Add to `kernel/syscall.h`:

```c
#define SYS_setpriority 22
#define SYS_getpriority 23
```

### Step 4: User Space Interface

Add entries to `user/usys.pl`:

```perl
entry("setpriority");
entry("getpriority");
```

Add declarations to `user/user.h`:

```c
int setpriority(int, int);
int getpriority(int);
```

### Step 5: Implement the System Calls

In `kernel/sysproc.c`, implement both functions:

#### `sys_setpriority()`

```c
uint64
sys_setpriority(void)
{
  // TODO:
  // 1. Read arguments: pid (argint 0), priority (argint 1)
  // 2. Validate priority is in range [0, 20]
  // 3. Find the process with the given pid in proc[]
  // 4. Set its priority field
  // 5. Return 0 on success, -1 on failure
}
```

**Hint**: You need to iterate the `proc[]` array (declared `extern` in `defs.h`) to find the process. Remember to `acquire(&p->lock)` before accessing and `release(&p->lock)` afterward.

#### `sys_getpriority()`

```c
uint64
sys_getpriority(void)
{
  // TODO:
  // 1. Read argument: pid (argint 0)
  // 2. Find the process with the given pid in proc[]
  // 3. Return its priority, or -1 if not found
}
```

### Step 6: Update the Syscall Dispatch Table

In `kernel/syscall.c`:

**(a)** Add function prototypes:

```c
extern uint64 sys_setpriority(void);
extern uint64 sys_getpriority(void);
```

**(b)** Add entries to the `syscalls[]` array:

```c
[SYS_setpriority] sys_setpriority,
[SYS_getpriority] sys_getpriority,
```

### Step 7: Modify the Scheduler (Core Task)

This is the main part of the assignment. Modify `scheduler()` in `kernel/proc.c`.

**Current behavior** (round-robin): Runs the first RUNNABLE process found.

**New behavior** (priority): Scans all processes, selects the one with the lowest priority number.

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();
    intr_off();

    // TODO: Replace the existing round-robin loop with priority-based selection
    //
    // Algorithm:
    //   1. Scan all proc[] entries to find the RUNNABLE process
    //      with the lowest priority number (highest priority).
    //   2. If found, set its state to RUNNING and swtch to it.
    //   3. If no RUNNABLE process exists, execute wfi.
    //
    // Important:
    //   - You must acquire p->lock before checking p->state
    //     and release it after you're done with p.
    //   - For tie-breaking (same priority), round-robin is acceptable
    //     (the linear scan naturally provides this).
    //
    // Hint: Use two passes:
    //   Pass 1: Find the minimum priority value among all RUNNABLE processes
    //   Pass 2: Run the first RUNNABLE process with that priority value
    //
    //   OR: Single pass — track the "best so far" candidate.

    /* ====== YOUR CODE HERE ====== */


    /* ====== END YOUR CODE ====== */
  }
}
```

**Hint for locking**: The single-pass approach is simpler. Keep a pointer to the best candidate found so far. When you find a better one, release the old lock and acquire the new one:

```
best = NULL
for each p in proc[]:
    acquire(&p->lock)
    if p is RUNNABLE and (best == NULL or p->priority < best->priority):
        if best != NULL: release(&best->lock)
        best = p
    else:
        release(&p->lock)

if best != NULL:
    // dispatch best (it's already locked)
    best->state = RUNNING
    c->proc = best
    swtch(...)
    c->proc = 0
    release(&best->lock)
```

---

## Testing

### Test Program

Copy `priority_test.c` to `xv6-riscv/user/` and add `$U/_priority_test\` to `UPROGS` in the `Makefile`.

```bash
make clean && make qemu

# In xv6 shell:
$ priority_test
```

### Expected Output

```
=== Priority Scheduler Test ===

--- Test 1: setpriority/getpriority ---
PID 4: default priority = 10
PID 4: after setpriority(4, 5), priority = 5
setpriority with invalid priority (-1): returned -1 (OK)
setpriority with invalid priority (21): returned -1 (OK)
Test 1 PASSED

--- Test 2: Priority inheritance via fork ---
Parent priority = 3
Child priority = 3
Test 2 PASSED

--- Test 3: High-priority process runs first ---
[HIGH prio=1] finished
[MED  prio=10] finished
[LOW  prio=19] finished
Test 3 PASSED

All tests passed!
```

**Key observation** for Test 3: The high-priority process (priority 1) should complete all its work before the medium-priority process (priority 10), which completes before the low-priority process (priority 19). This is different from the round-robin behavior you observed in the lab, where all processes would interleave equally.

---

## Submission Method

Submit using one of the following:

**Method 1: patch file** (recommended)
```bash
cd xv6-riscv
git diff > priority.patch
```

**Method 2: Modified files**

Submit the following files:
- `kernel/proc.h`
- `kernel/proc.c`
- `kernel/syscall.h`
- `kernel/syscall.c`
- `kernel/sysproc.c`
- `user/usys.pl`
- `user/user.h`

---

## Grading Criteria

| Item | Weight |
|------|--------|
| `priority` field added and properly initialized/reset/inherited | 15% |
| `setpriority` syscall works correctly (including validation) | 15% |
| `getpriority` syscall works correctly | 10% |
| `scheduler()` selects highest-priority process | 40% |
| Test program passes all tests | 10% |
| Code style and comments | 10% |

---

## Correspondence with Theory and Lab

| Assignment Task | Theory Concept | Lab Exercise |
|---|---|---|
| Modify `scheduler()` | Scheduling algorithms (beyond RR) | Ex 2: Traced `scheduler()`, Ex 3: Observed RR |
| Add `priority` to `struct proc` | Process attributes for scheduling decisions | Ex 1: Analyzed `struct context`, Ex 3: Studied `struct proc` |
| `setpriority`/`getpriority` syscalls | — (syscall mechanism from Week 3) | Week 3 Lab: Syscall path tracing |
| Observe scheduling order | Scheduling criteria (waiting time, response time) | Ex 3: Observed interleaved execution |
| High-priority starvation | Convoy effect (FCFS), SJF optimality trade-offs | Ex 4: sleep/wakeup state transitions |

---

## Questions (include in report)

1. With your priority scheduler, what happens to a low-priority process if high-priority processes keep arriving? How does this relate to the **convoy effect** from FCFS that we studied in theory?

2. Run 3 `spin` processes with priorities 1, 10, and 19 (using `setpriority` in a wrapper). Compare the scheduling behavior with the round-robin behavior you observed in Lab Exercise 3. What is different?

3. SJF is considered optimal for minimizing average waiting time. How would you use priority scheduling to approximate SJF? What information would you need?

---

## References

- Silberschatz Ch 5 (Sections 5.1–5.3): CPU Scheduling Algorithms
- xv6 textbook Chapter 7: Scheduling
- Week 3 Assignment: Adding system calls to xv6
- Week 6 Lab: Context switching and scheduler tracing
