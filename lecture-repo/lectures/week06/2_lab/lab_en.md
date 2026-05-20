---
theme: default
title: "Week 06 — Lab: Context Switching"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 6 — Context Switching

Korea University Sejong Campus, Department of Computer Science & Software

---

# Lab Overview

**Duration**: ~50 minutes · 4 exercises

```mermaid
graph LR
    E1["Ex 1<br/>swtch.S"] --> E2["Ex 2<br/>Scheduler Trace"]
    E2 --> E3["Ex 3<br/>Round-Robin"]
    E3 --> E4["Ex 4<br/>sleep/wakeup"]
    style E1 fill:#e3f2fd
    style E2 fill:#fff3e0
    style E3 fill:#e8f5e9
    style E4 fill:#fce4ec
```

**Prerequisites**:

```bash
cd xv6-riscv && make qemu   # verify clean boot
```

---

# Exercise 1: swtch.S Analysis

**Files**: `kernel/swtch.S`, `kernel/proc.h`

<div class="grid grid-cols-2 gap-4">
<div>

**`struct context`** — only **callee-saved** registers:

```c
struct context {
  uint64 ra;  // return address
  uint64 sp;  // stack pointer
  uint64 s0;  // s0 – s11
  uint64 s1;
  /* ... s2 through s11 ... */
};
```

**`swtch(old, new)`:**

```asm
swtch:
  sd ra, 0(a0)   # save to old
  sd sp, 8(a0)
  sd s0, 16(a0) ...
  ld ra, 0(a1)   # load from new
  ld sp, 8(a1)
  ld s0, 16(a1) ...
  ret             # jump to new->ra
```

</div>
<div>

**Context switch flow:**

```mermaid
graph TD
    P1["Process A<br/>(running)"] -->|"1. save regs<br/>to A.context"| SW["swtch()"]
    SW -->|"2. load regs<br/>from B.context"| P2["Process B<br/>(resumes)"]
    SW -->|"ret jumps to<br/>B's saved ra"| P2
    style P1 fill:#ffcdd2
    style P2 fill:#c8e6c9
    style SW fill:#fff3e0
```

**New process bootstrap**: `allocproc()` sets `p->context.ra = forkret` so the first `swtch` into a new process lands in `forkret()`.

</div>
</div>

---

# Exercise 2: Scheduler Tracing

**Goal**: Make the scheduler visible with `printf`

**Edit `kernel/proc.c` — inside `scheduler()`:**

```c
if (p->state == RUNNABLE) {
    printf("[sched] cpu%d: switch to pid=%d name=%s\n",
           cpuid(), p->pid, p->name);   // ← add this
    p->state = RUNNING;
    c->proc = p;
    swtch(&c->context, &p->context);
```

Or apply: `git apply scheduler_trace.patch`

> **Skeleton**: `examples/skeletons/lab2_scheduler_trace.patch` (TODO comments only)
> **Solution**: `examples/solutions/lab2_scheduler_trace.patch`

```bash
make clean && make CPUS=1 qemu   # single CPU for readable output
```

```mermaid
sequenceDiagram
    participant S as Scheduler
    participant Init as pid=1 (init)
    participant Sh as pid=2 (sh)
    S->>Init: switch to pid=1
    Init->>S: yield (spawns sh)
    S->>Sh: switch to pid=2
    Note over Sh: waiting for keyboard<br/>(SLEEPING, not scheduled)
```

---

# Exercise 3: Round-Robin Observation

**Start multiple background processes in xv6 shell:**

> **Skeleton**: `examples/skeletons/spin.c` (TODO: add infinite loop)
> **Solution**: `examples/solutions/spin.c`

```
$ spin &
$ spin &
$ spin &
```

```mermaid
sequenceDiagram
    participant S as Scheduler
    participant P3 as pid=3 (spin)
    participant P4 as pid=4 (spin)
    participant P5 as pid=5 (spin)
    S->>P3: switch to pid=3
    P3->>S: timer interrupt
    S->>P4: switch to pid=4
    P4->>S: timer interrupt
    S->>P5: switch to pid=5
    P5->>S: timer interrupt
    S->>P3: switch to pid=3
    Note over S,P5: Round-robin cycle repeats
```

**Why round-robin?** The scheduler scans `proc[]` linearly:

```c
for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == RUNNABLE) { /* run it */ }
}
```

**Subtle bias**: lower-index processes checked first every cycle.

**Multi-CPU**: rebuild with `CPUS=3` → multiple CPUs pick different processes simultaneously.

---

# Exercise 4: sleep/wakeup Tracing

**Goal**: Follow a process through blocking and unblocking

**Add trace to `kernel/proc.c` — `sleep()` function:**

```c
printf("[sleep]  pid=%d name=%s chan=%p\n", p->pid, p->name, chan);
p->state = SLEEPING;
sched();
printf("[wakeup] pid=%d name=%s\n", p->pid, p->name);
```

> **Skeleton**: `examples/skeletons/lab4_sleep_trace.patch` (TODO comments only)
> **Solution**: `examples/solutions/lab4_sleep_trace.patch`

**Test**: `echo hello | cat` in xv6 shell

```mermaid
sequenceDiagram
    participant Cat as cat (pid=3)
    participant Sched as Scheduler
    participant Echo as echo (pid=4)
    Cat->>Sched: sleep(pipe) — buffer empty
    Sched->>Echo: switch to echo
    Echo->>Echo: write "hello" to pipe
    Echo->>Cat: wakeup(pipe)
    Note over Cat: state → RUNNABLE
    Echo->>Sched: exit
    Sched->>Cat: switch to cat
    Cat->>Cat: read "hello", print it
```

`wakeup` only sets state to RUNNABLE — the woken process does **not** run immediately.

---

# How to Run

<div class="grid grid-cols-2 gap-4">
<div>

**1. Apply a patch**

```bash
cd xv6-riscv
git apply path/to/patch
# e.g. git apply ../lectures/week06/2_lab/examples/solutions/lab2_scheduler_trace.patch
```

**2. Add `spin.c` to the build**

Copy `spin.c` into `user/` and edit `Makefile` — add to `UPROGS`:

```makefile
UPROGS=\
  ...
  $U/_spin\
```

**3. Build with single CPU**

```bash
make clean && make CPUS=1 qemu
```

</div>
<div>

**4. Run inside xv6**

```
$ spin &
$ spin &
$ spin &
```

**5. Exit QEMU**

Press **Ctrl-A** then **X**

**6. Revert patches** (restore original source)

```bash
cd xv6-riscv
git checkout -- kernel/proc.c
```

**7. Apply multiple patches**

```bash
git apply path/to/lab2_scheduler_trace.patch
git apply path/to/lab4_sleep_trace.patch
```

</div>
</div>

---

# Key Takeaways

```mermaid
graph TD
    Y["yield()"] --> SCH["sched()"]
    SCH --> SW["swtch(old, new)"]
    SW -->|"save old regs"| CTX1["old context"]
    SW -->|"load new regs"| CTX2["new context"]
    CTX2 --> RET["ret → new->ra"]
    style Y fill:#e3f2fd
    style SW fill:#fff3e0
    style RET fill:#c8e6c9
```

| Concept | Key Insight |
|---|---|
| **swtch.S** | Save callee-saved regs → load new regs → `ret` to `new->ra` |
| **Scheduler** | Linear scan of `proc[]` for RUNNABLE — simple round-robin |
| **sleep/wakeup** | `sleep(chan, lk)` → SLEEPING; `wakeup(chan)` → RUNNABLE |
| **End-to-end** | yield → sched → swtch (to scheduler) → swtch (to next) → resume |
