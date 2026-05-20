---
theme: default
title: "Week 2 — Process 1"
info: "Operating Systems Ch 3"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Week 2 — Process 1

Operating Systems Ch 3 (Sections 3.1 – 3.3)

<div class="pt-4 text-gray-400">
Korea University Sejong Campus, Department of Computer Science & Software
</div>

---

# Table of Contents

<div class="text-left text-lg leading-10">

1. Process Concept (The Process, State, PCB, Threads)
2. Process Scheduling (Queues, Context Switch)
3. Operations on Processes (Creation, Termination)
4. Lab — fork(), exec(), wait()
5. Summary and Next Week Preview

</div>

---
layout: section
---

# Part 1
## Process Concept
### Silberschatz Ch 3.1

---

# What Is a Process?

<div class="text-left text-base leading-8">

**Process** = a program in execution

- Early computers: only one program running at a time
- Modern computers: multiple programs loaded into memory and executed **concurrently**
- This evolution required stricter control and compartmentalization, giving rise to the **process** concept

> "A process is the unit of work in a modern computing system."

- From the OS perspective, both processes executing user code and processes executing OS code exist
- The CPU(s) are **multiplexed** among these processes

</div>

---

# The Process — Memory Layout (Figure 3.1)

<div class="grid grid-cols-[1fr_1fr] gap-4">
<div class="text-left text-base leading-8">

The memory layout of a process is divided into several **sections**:

- **Text** — executable code
- **Data** — global variables
- **Heap** — dynamically allocated at runtime (`malloc`, `new`)
- **Stack** — temporary data during function calls (parameters, return address, local variables)

<br>

- **Text / Data**: **fixed** in size
- **Stack / Heap**: grow and shrink **dynamically**

</div>
<div class="flex items-center justify-center">
<img src="./images/figures/p002_fig.png" class="h-80" />
</div>
</div>

<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 3.1 — Layout of a process in memory</p>

---

# Dynamic Growth of Stack and Heap

<div class="text-left text-base leading-8">

**Stack growth:**
- Each time a function is called, an **activation record** (function parameters, local variables, return address) is pushed onto the stack
- When a function returns, the activation record is popped
- The stack grows from **high addresses toward low addresses** (downward)

**Heap growth:**
- When memory is dynamically allocated with `malloc()`, `new`, etc., the heap grows
- When freed with `free()`, `delete`, the heap shrinks
- The heap grows from **low addresses toward high addresses** (upward)

**Role of the OS:**
- Since the stack and heap grow toward each other, the OS must ensure they do **not overlap**

</div>

---

# Memory Layout of a C Program (Detailed)

<div class="text-left text-sm leading-7">

```c
#include <stdio.h>
#include <stdlib.h>

int x;            // uninitialized data (BSS)
int y = 15;       // initialized data

int main(int argc, char *argv[]) {   // argc, argv area
    int *values;   // local variable → stack
    int i;         // local variable → stack

    values = (int *)malloc(sizeof(int)*5);  // dynamic allocation → heap
    for (i = 0; i < 5; i++)
        values[i] = i;
    return 0;
}
```

You can check the section sizes with the `size` command:

| text | data | bss | dec | hex |
|------|------|-----|-----|-----|
| 1158 | 284  | 8   | 1450| 5aa |

- **data**: initialized global variables, **bss** (block started by symbol): uninitialized global variables

</div>

---

# Program vs. Process

<div class="text-left text-base leading-8">

| Distinction | Program | Process |
|-------------|---------|---------|
| Nature | **Passive** entity | **Active** entity |
| Form | Executable file stored on disk | Execution unit with a PC and associated resources |
| Conversion | Becomes a process when the executable is loaded into memory | |

**Key distinctions:**
- The same program can run as multiple processes
  - Example: multiple users running the same web browser simultaneously
  - The text section is identical, but **data, heap, and stack are separate for each**
- A process can itself serve as the execution environment for other code
  - Example: the JVM runs as a single process and interprets/executes Java programs

</div>

---

# Process State (Figure 3.2)

<div class="text-left text-base leading-8">

A process changes its **state** during execution:

| State | Description |
|-------|-------------|
| **New** | The process is being created |
| **Running** | Instructions are being executed |
| **Waiting** | The process is waiting for some event (I/O completion, signal, etc.) |
| **Ready** | The process is waiting to be assigned to a processor |
| **Terminated** | The process has finished execution |

**Important:** At any point in time, only **one process** can be in the running state on a single processor core.
- However, multiple processes can be in the **ready** or **waiting** state simultaneously

</div>

---

# Process State Transition Diagram (Figure 3.2)

<img src="./images/figures/p005_fig.png" class="h-56 mx-auto" />

<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 3.2 — Diagram of process state</p>

---

# Process State Transitions — Detailed Explanation

<div class="text-left text-base leading-8">

| Transition | Cause |
|------------|-------|
| **New → Ready** | admitted — The OS approves process creation; memory allocation is complete |
| **Ready → Running** | scheduler dispatch — The CPU scheduler selects the process and assigns it to a CPU core |
| **Running → Ready** | interrupt — The time slice expires, causing the CPU to be preempted |
| **Running → Waiting** | I/O or event wait — An I/O request is made or a specific event is awaited |
| **Waiting → Ready** | I/O or event completion — The awaited I/O or event has completed |
| **Running → Terminated** | exit — The process finishes execution and terminates |

- **Running → Waiting**: The process voluntarily transitions (e.g., requesting a disk read)
- **Running → Ready**: An involuntary transition due to an external factor (e.g., timer interrupt)

</div>

---

# Process Control Block — PCB (Figure 3.3)

<div class="grid grid-cols-[1fr_1fr] gap-4">
<div class="text-left text-base leading-8">

Each process is represented in the OS by a **PCB (Process Control Block)**.
- Also known as: **task control block**

The PCB serves as the repository for all the data needed to **start or restart** a process.

- Process state (new, ready, running, …)
- Process number (pid)
- Program counter
- CPU registers
- Memory limits
- List of open files, etc.

</div>
<div class="flex items-center justify-center">
<img src="./images/figures/p005_fig2.png" class="h-72" />
</div>
</div>

<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 3.3 — Process control block (PCB)</p>

---

# Detailed Information Stored in the PCB

<div class="text-left text-sm leading-7">

| Field | Description |
|-------|-------------|
| **Process state** | Current state (new, ready, running, waiting, terminated) |
| **Program counter** | Address of the next instruction to execute |
| **CPU registers** | Accumulator, index register, stack pointer, general-purpose registers, condition codes, etc. |
| **CPU scheduling info** | Process priority, scheduling queue pointers, other scheduling parameters |
| **Memory-management info** | Base/limit register values, page table or segment table |
| **Accounting info** | CPU time used, elapsed real time, time limits, account number, process number |
| **I/O status info** | List of allocated I/O devices, list of open files |

**When an interrupt occurs**: The PC and register states must be saved to the PCB so the process can be correctly resumed later.

</div>

---

# PCB in Linux — task_struct

<div class="text-left text-sm leading-7">

In the Linux kernel, the PCB is represented by the **`task_struct`** C structure.
- Location: `<include/linux/sched.h>`

```c
long state;                    /* process state */
struct sched_entity se;        /* scheduling information */
struct task_struct *parent;    /* parent process */
struct list_head children;     /* list of child processes */
struct files_struct *files;    /* list of open files */
struct mm_struct *mm;          /* address space (memory management) */
```

- All active processes are managed via a **doubly linked list**
- The `current` pointer points to the `task_struct` of the currently running process

```c
current->state = new_state;  // change the state of the current process
```

</div>

---

# Thread Overview (Ch 3.1.4)

<div class="text-left text-base leading-8">

The process model discussed so far assumes a **single thread of execution**:
- A process can perform only one task at a time
- Example: in a word processor, you cannot type and run a spell checker at the same time

**Thread** = a unit of execution within a process

Modern operating systems allow **multiple threads of execution**:
- A single process can perform multiple tasks simultaneously
- **Parallel execution** is possible on multicore systems

</div>

---

# Advantages and Examples of Threads

<div class="text-left text-base leading-8">

**Multithreaded word processor example:**
- Thread 1: Manages user input
- Thread 2: Runs the spell checker
- Both can run simultaneously!

**Threads and the PCB:**
- In systems that support threads, the PCB is extended to **include information for each thread**
- System-wide changes are needed to support threads

**What threads of the same process share:**
- Text section, Data section, Heap, open files, signals, etc.

**What is independent per thread:**
- Program counter, register set, stack

> Detailed coverage of threads is in **Ch 4**.

</div>

---
layout: section
---

# Part 2
## Process Scheduling
### Silberschatz Ch 3.2

---

# Why Is Process Scheduling Needed?

<div class="text-left text-base leading-8">

**Goal of multiprogramming:**
- Maximize **CPU utilization** by ensuring some process is always running

**Goal of time sharing:**
- Frequently switch the CPU core among processes so users can **interact** with each program

-> **Process Scheduler**: Selects which process to run on a CPU core from among the available processes

- Single core: only one process can be running at a time
- Multicore: multiple processes can be running simultaneously
- If the number of processes exceeds the number of cores, the rest must wait

</div>

---

# Degree of Multiprogramming and Process Types

<div class="text-left text-base leading-8">

**Degree of Multiprogramming:**
- The number of processes currently in memory

**Classification by process type:**

| Type | Characteristics |
|------|----------------|
| **I/O-bound process** | Spends more time doing I/O than computation; issues I/O requests frequently |
| **CPU-bound process** | Spends more time doing computation than I/O; issues I/O requests infrequently |

- For efficient scheduling, it is important to have a proper mix of I/O-bound and CPU-bound processes
- Only I/O-bound processes -> ready queue empties, CPU idles
- Only CPU-bound processes -> wait queue empties, I/O devices idle

</div>

---

# Scheduling Queues (Figure 3.4)

<div class="text-left text-base leading-8">

When a process enters the system, it is placed in a **scheduling queue**.

**Ready Queue:**
- A queue of processes waiting for the CPU; implemented as a **linked list**

**Wait Queue:**
- A queue of processes waiting for a specific event (e.g., I/O completion)

<img src="./images/figures/p008_fig.png" class="h-44 mx-auto" />

<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 3.4 — The ready queue and wait queues</p>

</div>

---

# Queueing Diagram (Figure 3.5)

<div class="text-left text-base leading-8">

<img src="./images/figures/p009_fig.png" class="h-56 mx-auto" />

<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 3.5 — Queueing-diagram representation of process scheduling</p>

The process repeats this cycle until it terminates.
Upon termination, it is removed from all queues, and its PCB and resources are deallocated.

</div>

---

# CPU Scheduling (Ch 3.2.2)

<div class="text-left text-base leading-8">

**Role of the CPU Scheduler:**
- Selects one of the processes in the ready queue and assigns it to a CPU core

**Scheduling frequency:**
- I/O-bound processes: execute for only a few milliseconds before waiting for I/O
- CPU-bound processes: execute longer, but the scheduler forcibly reclaims the CPU
- Typically, the CPU scheduler runs at least once every **100 ms**

**Swapping (an intermediate form of scheduling):**
- Moves a process from memory to disk (**swap out**) and later reloads it (**swap in**)
- Reduces the degree of multiprogramming to relieve memory pressure
- Only necessary when memory is **overcommitted**
- Covered in detail in Ch 9

</div>

---

# Context Switch Concept (Figure 3.6)

<div class="text-left text-base leading-8">

**Context:**
- Represented in the PCB of the process
- Includes CPU register values, process state, memory management information, etc.

**Context Switch:**
- The operation of switching a CPU core to a different process
- **State save** of the current process + **state restore** of the new process

When an interrupt occurs:
1. The system must save the context of the current process
2. After handling, it restores that context to resume the process

</div>

---

# Context Switch Process — Detailed (Figure 3.6)

<img src="./images/figures/p010_fig.png" class="h-80 mx-auto" />

<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 3.6 — Diagram showing context switch from process to process</p>

During the P₀ → P₁ switch, P₀ is **idle**; during the P₁ → P₀ switch, P₁ is **idle**.

---

# Overhead of Context Switching

<div class="text-left text-base leading-8">

**Context switch time = pure overhead**
- The system performs **no useful work** during the switch
- Typical speed: **several microseconds**

**Factors affecting speed:**
- Memory speed
- Number of registers that must be copied
- Availability of special instructions (e.g., instructions that load/store all registers at once)

**Hardware support:**
- Some processors provide **multiple register sets**
- In such cases, a context switch can be achieved simply by changing the current register set pointer
- If the number of active processes exceeds the number of register sets, memory copying is required again

</div>

---

# Multitasking on Mobile Systems

<div class="text-left text-sm leading-7">

**iOS:**
- Early iOS: no multitasking for user apps (only the foreground app runs; the rest are suspended)
- Starting with iOS 4: limited multitasking (1 foreground + multiple background)
- Later versions: richer multitasking support as hardware improved
- iPad: simultaneous execution of 2 foreground apps via split-screen

**Android:**
- Supported multitasking from the beginning, with no restrictions on background app types
- Uses a **Service** (a separate app component) for background processing
  - Example: a music streaming app continues audio delivery via a service even when moved to the background
  - Services have no UI and use less memory
  - Even if a background app is suspended, its service continues to run

</div>

---
layout: section
---

# Part 3
## Operations on Processes
### Silberschatz Ch 3.3

---

# Process Creation (Ch 3.3.1)

<div class="text-left text-base leading-8">

A process can create several new processes during its execution.

| Term | Description |
|------|-------------|
| **Parent process** | The process that creates another |
| **Child process** | The newly created process |
| **Process tree** | A tree structure formed by parent-child relationships |
| **Process ID (pid)** | A unique identifier for each process (typically an integer) |

- The pid is used as an **index** within the kernel to access various attributes of the process
- Most operating systems including UNIX, Linux, and Windows use pids

</div>

---

# Linux Process Tree (Figure 3.7)

<div class="text-left text-sm leading-7">

<img src="./images/figures/p012_fig.png" class="h-44 mx-auto" />

<p class="text-xs text-gray-500 text-center">Silberschatz, Figure 3.7 — A tree of processes on a typical Linux system</p>

- **systemd** (pid = 1): The **root parent** of all user processes
  - The first user process created during system boot
  - Creates processes for additional services such as web servers, SSH servers, etc.
- In traditional UNIX, **init** (pid = 1) performed this role
- Recent Linux distributions have replaced init with **systemd**

</div>

---

# Process Creation — Resources and Execution Options

<div class="text-left text-base leading-8">

**How a child process obtains resources:**
1. Obtains resources directly from the OS
2. Receives a subset of the parent process's resources
   - The parent distributes or shares resources with its children
   - Resource limits prevent excessive child process creation

**Execution options:**
1. The parent continues executing **concurrently** with the child
2. The parent **waits** until the child terminates

**Address space options:**
1. The child is a **duplicate** of the parent — same program and data
2. A **new program** is loaded into the child

</div>

---

# Process Creation in UNIX/Linux

<div class="text-left text-base leading-8">

In UNIX, a new process is created with the **`fork()`** system call.

**How fork() works:**
1. Creates a new process as a **copy of the calling process's address space**
2. **Both** the parent and child continue execution from the instruction after fork()

**Distinguishing parent from child via fork()'s return value:**

| Return Value | Meaning |
|--------------|---------|
| **0** | Child process (newly created process) |
| **Positive (> 0)** | Parent process (the child's pid is returned) |
| **-1** | fork failed (error) |

- The child inherits the parent's **privileges**, **scheduling attributes**, **open files**, etc.

</div>

---

# exec() After fork()

<div class="text-left text-base leading-8">

**exec()** system call: **replaces** the process's memory space with a new program

```text
fork() → child process created (copy of parent)
         │
    child: calls exec()
         │
    child's memory image replaced with the new program
    (the original program is destroyed)
```

- exec() loads a binary file into memory and begins execution
- If exec() succeeds: it **does not return** to the original code (memory has been overwritten)
- If exec() fails: control is returned (error handling is possible)
- If the child does not call exec(): it continues executing the copy of the parent's code

</div>

---

# fork() Code Example (Figure 3.8)

<div class="text-left text-sm leading-7">

```c
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid;

    /* fork a child process */
    pid = fork();

    if (pid < 0) {          /* error occurred */
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    else if (pid == 0) {    /* child process */
        execlp("/bin/ls", "ls", NULL);
    }
    else {                  /* parent process */
        /* parent waits for child to complete */
        wait(NULL);
        printf("Child Complete");
    }

    return 0;
}
```

</div>

---

# fork() + exec() + wait() Flow Diagram

<div class="text-left text-base leading-7">

```text
Parent Process
    │
    ├── fork() called
    │       │
    │       ├── [Child process created]
    │       │       │
    │       │       ├── pid == 0 (child)
    │       │       │       │
    │       │       │       └── execlp("/bin/ls", "ls", NULL)
    │       │       │               → executes ls command
    │       │       │               → terminates via exit()
    │       │
    │       ├── pid > 0 (parent)
    │       │       │
    │       │       └── wait(NULL)  ← waits for child to terminate
    │       │               │
    │       │               └── prints "Child Complete"
    │
    └── return 0
```

This is the fundamental operating principle of the UNIX shell!

</div>

---

# Deep Dive into fork() Return Values

<div class="text-left text-base leading-8">

**After fork(), the parent and child have the same code, but the return value differs:**

```c
pid_t pid = fork();
// At this point, there are 2 processes!

printf("pid = %d\n", pid);
```

**Output (execution order is not guaranteed):**
```text
pid = 3456    ← parent: prints the child's actual pid
pid = 0       ← child: prints 0
```

**Why was it designed this way?**
- Parent: needs the child's pid for management purposes (wait, kill, etc.)
- Child: can obtain its own pid with `getpid()` and its parent's pid with `getppid()`

</div>

---

# Process Creation — Windows Comparison

<div class="text-left text-base leading-8">

**Windows: `CreateProcess()` API**

| Comparison | UNIX fork() | Windows CreateProcess() |
|------------|-------------|------------------------|
| Address space | **Duplicates** parent's address space | **Specifies and loads** a new program |
| Parameters | None (0) | **10+** required |
| Waiting for child | `wait()` | `WaitForSingleObject()` |
| Process info | pid (integer) | `PROCESS_INFORMATION` struct (handles + identifiers) |

- Windows `STARTUPINFO`: specifies the new process's window size, appearance, standard I/O handles, etc.
- Windows `PROCESS_INFORMATION`: contains handles and identifiers for the created process and thread

</div>

---

# Process Termination (Ch 3.3.2)

<div class="text-left text-base leading-8">

**Normal termination:**
- The process executes its last statement and requests deletion from the OS via the `exit()` system call
- A status value (typically an integer) is passed to the **waiting parent** (via `wait()`)
- The OS reclaims all of the process's resources (physical/virtual memory, open files, I/O buffers)

```c
/* exit with status 1 */
exit(1);
```

**Parent's wait() call:**
```c
pid_t pid;
int status;
pid = wait(&status);  // returns the terminated child's pid; stores exit status in status
```

</div>

---

# Forced Termination by Parent (abort)

<div class="text-left text-base leading-8">

Situations where a parent process may **forcibly terminate** a child:

1. The child has **exceeded its allocated resources**
   - The parent needs a mechanism to inspect the child's state
2. The **task assigned to the child is no longer needed**
3. The **parent is terminating** and the OS does not allow orphan processes

**Cascading Termination:**
- A system where all children must be terminated when the parent terminates
- Parent terminates -> children terminate -> grandchildren terminate -> ...
- Typically **initiated by the OS**

- Forcibly terminating a process in Linux: the `kill` command or the `kill()` system call
- In Windows: the `TerminateProcess()` API

</div>

---

# Zombie Process

<div class="text-left text-base leading-8">

**What is a zombie process?**
- A child process that has **terminated (exit)**, but whose parent has **not yet called wait()**
- The process has finished executing, but its **process table entry remains**
  - Because the exit status information must be retained

**Every process briefly passes through the zombie state upon termination!**
- Typically remains a zombie for only a very short time
- When the parent calls `wait()`:
  - The zombie process's pid and process table entry are **released**

**Problem:**
- If the parent never calls `wait()`, zombies continue to accumulate, wasting system resources

</div>

---

# Orphan Process

<div class="text-left text-base leading-8">

**What is an orphan process?**
- A child process left behind because its parent **terminated first** without calling `wait()`

**Handling in UNIX/Linux:**
- Traditional UNIX: The **init** process (pid = 1) becomes the new parent of orphan processes
- Modern Linux: **systemd** performs this role
  - Linux also allows processes other than systemd to inherit and manage orphan processes

**Orphan process cleanup procedure:**
1. The parent terminates
2. The orphaned child process is adopted by init/systemd
3. init/systemd periodically calls `wait()`
4. The orphan process's exit status is collected, and the process table entry is released

</div>

---

# Zombie vs. Orphan Comparison

<div class="text-left text-base leading-8">

| Distinction | Zombie Process | Orphan Process |
|-------------|---------------|----------------|
| State | Child has **terminated** | Child is **still running** (parent terminated first) |
| Cause | Parent has **not called wait()** | Parent **terminated first** |
| Resources | Occupies only a process table entry | Running normally |
| Resolution | Parent calls wait() | init/systemd adopts and manages |

```text
[Zombie Scenario]               [Orphan Scenario]
Parent (running)               Parent (terminated)
   └── Child (terminated)         └── Child (running)
       → wait() not called            → adopted by init/systemd
       → zombie state                 → continues normal execution
```

</div>

---

# Android Process Hierarchy (Ch 3.3.2.1)

<div class="text-left text-sm leading-7">

Mobile operating systems may need to terminate existing processes due to limited resources.
Android determines the termination order based on an **importance hierarchy**:

| Rank | Type | Description | Termination Priority |
|------|------|-------------|---------------------|
| 1 | **Foreground process** | The app currently visible on screen and being interacted with by the user | Last to be terminated |
| 2 | **Visible process** | Not directly visible but performing an activity referenced by a foreground app | |
| 3 | **Service process** | In the background but performing an activity apparent to the user (e.g., music streaming) | |
| 4 | **Background process** | Performing an activity not visible to the user | |
| 5 | **Empty process** | Not associated with any app component | First to be terminated |

- When resource reclamation is needed: terminated in order Empty -> Background -> Service -> ...
- If a process serves multiple roles, the **highest rank** applies

</div>

---

# Chrome Browser's Multiprocess Architecture

<div class="text-left text-base leading-8">

**Problem:** In a tab-based web browser, if a web app in one tab crashes, the entire browser crashes

**Chrome's solution:** Multiprocess architecture

| Process Type | Role | Count |
|-------------|------|-------|
| **Browser** | UI management, disk/network I/O | 1 |
| **Renderer** | Web page rendering (HTML, JS, images) | 1 per tab |
| **Plug-in** | Plug-in code execution (Flash, etc.) | 1 per plug-in type |

**Advantages:**
- Websites are **isolated** from each other — a crash on one site only affects that renderer
- Renderer processes run in a **sandbox** — restricted from disk/network I/O access
- Minimizes the impact of security vulnerabilities

</div>

---
layout: section
---

# Part 4
## Lab — fork(), exec(), wait()

---

# Lab: Basic Usage of fork()

<div class="text-left text-sm leading-7">

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid;

    printf("Before fork: pid = %d\n", getpid());

    pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        /* child process */
        printf("Child: my pid = %d, parent pid = %d\n",
               getpid(), getppid());
    } else {
        /* parent process */
        printf("Parent: my pid = %d, child pid = %d\n",
               getpid(), pid);
        wait(NULL);
    }

    printf("Process %d exiting\n", getpid());
    return 0;
}
```

</div>

---

# Lab: Creating Multiple Children with fork()

<div class="text-left text-sm leading-7">

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid;
    int i;

    for (i = 0; i < 3; i++) {
        pid = fork();
        if (pid == 0) {
            printf("Child %d: pid = %d, parent pid = %d\n",
                   i, getpid(), getppid());
            return 0;  // child exits here (important!)
        }
    }
    // parent: wait for all children to terminate
    for (i = 0; i < 3; i++) {
        wait(NULL);
    }
    printf("Parent: all children finished\n");
    return 0;
}
```

**Caution:** If the child process does not `return 0`, the child will also loop and call fork()!
This risks a **fork bomb**!

</div>

---

# Lab: Running a New Program with exec()

<div class="text-left text-sm leading-7">

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        /* child: execute ls -l command */
        printf("Child: about to exec ls -l\n");
        execlp("ls", "ls", "-l", NULL);

        /* the code below is never executed if exec() succeeds */
        perror("exec failed");
        return 1;
    } else if (pid > 0) {
        /* parent: wait for child to complete */
        wait(NULL);
        printf("Parent: child has finished\n");
    }
    return 0;
}
```

`execlp(file, arg0, arg1, ..., NULL)` — searches for file in PATH and executes it
- On success: the current process's memory image is **completely replaced** with the new program
- On failure: returns -1 and sets errno

</div>

---

# Lab: Comparing the exec() Family

<div class="text-left text-base leading-8">

The exec() family has several variants:

| Function | Argument Passing | Path Search | Environment Variables |
|----------|-----------------|-------------|----------------------|
| `execl()` | list (l) | full path | inherited |
| `execlp()` | list (l) | PATH search (p) | inherited |
| `execle()` | list (l) | full path | specified (e) |
| `execv()` | array (v) | full path | inherited |
| `execvp()` | array (v) | PATH search (p) | inherited |
| `execve()` | array (v) | full path | specified (e) |

- **l** (list): pass arguments as a variadic list — `execlp("ls", "ls", "-l", NULL)`
- **v** (vector): pass arguments as a string array — `execvp("ls", args)`
- **p** (path): search for the executable in the PATH environment variable
- **e** (environment): specify environment variables explicitly

</div>

---

# Lab: Detailed Behavior of wait()

<div class="text-left text-sm leading-7">

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        printf("Child: working...\n");
        sleep(2);
        exit(42);  // exit with status 42
    } else {
        int status;
        pid_t child_pid = wait(&status);

        if (WIFEXITED(status)) {
            printf("Child %d exited with status %d\n",
                   child_pid, WEXITSTATUS(status));
        }
    }
    return 0;
}
```

- `WIFEXITED(status)`: checks whether the child terminated normally
- `WEXITSTATUS(status)`: extracts the exit status value (0-255)
- `WIFSIGNALED(status)`: checks whether the child was terminated by a signal

</div>

---

# Lab: How a Simple Shell Works

<div class="text-left text-sm leading-7">

```c
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    char command[256];

    while (1) {
        printf("myshell> ");
        if (fgets(command, sizeof(command), stdin) == NULL)
            break;
        command[strlen(command) - 1] = '\0';  // remove newline

        if (strcmp(command, "exit") == 0)
            break;

        pid_t pid = fork();
        if (pid == 0) {
            execlp(command, command, NULL);
            perror("Command not found");
            return 1;
        } else {
            wait(NULL);  // wait for child to terminate → this is the shell's basic operation!
        }
    }
    return 0;
}
```

</div>

---

# Lab: Shell Operation = Repeated fork() + exec() + wait()

<div class="text-left text-base leading-8">

```text
Shell (Parent Process)
    │
    ├── Display prompt "myshell> "
    │
    ├── Read user input (e.g., "ls")
    │
    ├── fork() → create child process
    │       │
    │       └── [child] execlp("ls", "ls", NULL)
    │                  → execute ls
    │                  → exit()
    │
    ├── [parent] wait() → wait for child to terminate
    │
    ├── Confirm child termination
    │
    └── Loop back to the beginning for the next command
```

**This is the core operating principle of UNIX shells (bash, zsh, etc.)!**

</div>

---

# Lab Key Summary

<div class="text-left text-base leading-8">

| Function | Role | Key Characteristics |
|----------|------|---------------------|
| **fork()** | Creates a copy of the current process | Distinguishes parent/child by return value (parent: child pid, child: 0) |
| **exec()** | Replaces the process's memory image with a new program | Does not return to the original code on success |
| **wait()** | Waits for a child process to terminate | Collects child's exit status; prevents zombies |
| **exit()** | Terminates the process | Passes exit status to the parent |

**Combination patterns:**
- `fork() + exec()` = run a new program
- `fork() + exec() + wait()` = basic shell operation
- `fork()` only = create a child that executes the same code as the parent

</div>

---
layout: section
---

# Part 5
## Summary and Next Week Preview

---

# Summary — Process Concept

<div class="text-left text-base leading-8">

- **Process** = a program in execution (a program is passive; a process is active)
- **Memory layout**: text (code) / data (global variables) / heap (dynamic allocation) / stack (function calls)
  - text and data are fixed in size; heap and stack grow dynamically
- **Process State**: New -> Ready -> Running -> Waiting -> Terminated
  - Only one process can be in the running state on a single core
- **PCB (Process Control Block)**: a data structure that stores all information about a process
  - state, PC, registers, scheduling info, memory info, accounting info, I/O info
- **Thread**: a unit of execution within a process; enables parallel execution on multicore systems (Ch 4)

</div>

---

# Summary — Process Scheduling & Operations

<div class="text-left text-base leading-8">

**Process Scheduling:**
- Ready Queue / Wait Queue — processes move among various queues
- CPU Scheduler: selects a process from the ready queue and assigns it to the CPU
- **Context Switch**: save current process state + restore new process state (pure overhead)

**Operations on Processes:**
- **Process Creation**: parent -> child (distinguished by pid), forming a process tree
  - fork() = copy address space, exec() = load a new program
- **Process Termination**: exit() -> pass status to parent, collected via wait()
  - **Zombie**: terminated but wait() not called; **Orphan**: parent terminated first -> adopted by init/systemd
- **Android**: determines process termination order based on an importance hierarchy

</div>

---

# Next Week Preview

<div class="text-left text-lg leading-10">

**Week 3 — Process 2**

- Interprocess Communication (IPC)
  - Shared Memory
  - Message Passing
- Real-World IPC Implementations
  - POSIX Shared Memory, Mach Message Passing
  - Pipes (Ordinary / Named)
- Client-Server Communication
  - Sockets, Remote Procedure Calls (RPC)
- Lab: IPC implementation using pipes and shared memory

Textbook: **Ch 3, second half** (Sections 3.4 – 3.8)

</div>

