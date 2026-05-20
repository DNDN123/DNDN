---
theme: default
title: "Week 10 — Lab: Deadlocks"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 10 — Deadlocks

Korea University Sejong Campus, Department of Computer Science & Software

---

# Lab Overview

**Objectives**: Observe, resolve, and prevent deadlocks in user-space and xv6 kernel

| # | Topic | Time |
|---|-------|------|
| 1 | Observe deadlock with `pthread_mutex` | 10 min |
| 2 | Fix with lock ordering | 10 min |
| 3 | `trylock` + back-off strategy | 15 min |
| 4 | xv6 lock ordering analysis | 15 min |

```mermaid
graph LR
    E1["Ex 1<br/>🔴 Observe"] --> E2["Ex 2<br/>🟢 Lock Ordering"]
    E2 --> E3["Ex 3<br/>🟡 trylock"]
    E3 --> E4["Ex 4<br/>🔵 xv6 Analysis"]
    style E1 fill:#ffcdd2
    style E2 fill:#c8e6c9
    style E3 fill:#fff3e0
    style E4 fill:#bbdefb
```

---

# Exercise 1: Observe Deadlock

**Two threads, two mutexes** — classic circular wait

<div class="grid grid-cols-2 gap-4">
<div>

```c
void *thread1(void *arg) {
  pthread_mutex_lock(&lock_A);
  sleep(1);
  pthread_mutex_lock(&lock_B);
  // ... never reached ...
}

void *thread2(void *arg) {
  pthread_mutex_lock(&lock_B);
  sleep(1);
  pthread_mutex_lock(&lock_A);
  // ... never reached ...
}
```

```bash
./deadlock_demo   # hangs! Ctrl-C to kill
```

</div>
<div>

**Resource Allocation Graph:**

```mermaid
graph LR
    T1["Thread 1"] -->|holds| LA["Lock A"]
    T1 -->|wants| LB["Lock B"]
    T2["Thread 2"] -->|holds| LB
    T2 -->|wants| LA
    style T1 fill:#ffcdd2
    style T2 fill:#ffcdd2
    style LA fill:#fff3e0
    style LB fill:#fff3e0
```

**Cycle detected** → Deadlock!

All 4 Coffman conditions hold:
1. Mutual exclusion
2. Hold and wait
3. No preemption
4. **Circular wait** ←

</div>
</div>

<div class="text-right text-sm text-gray-400">📁 Skeleton: `examples/skeletons/lab1_deadlock_demo.c`</div>
<div class="text-right text-sm text-gray-400">📁 Solution: `examples/solutions/lab1_deadlock_demo.c`</div>

---

# Exercise 2: Lock Ordering Fix

**Principle**: establish a **global total order** — always acquire in the same order

<div class="grid grid-cols-2 gap-4">
<div>

**Before (deadlock):**

```mermaid
graph TD
    T1["Thread 1: A → B"] --> DL["💀 Deadlock"]
    T2["Thread 2: B → A"] --> DL
    style DL fill:#ffcdd2
```

</div>
<div>

**After (safe):**

```mermaid
graph TD
    T1["Thread 1: A → B"] --> OK["✅ Safe"]
    T2["Thread 2: A → B"] --> OK
    style OK fill:#c8e6c9
```

</div>
</div>

```c
// Both threads acquire in the SAME order
void *thread_fixed(void *arg) {
    pthread_mutex_lock(&lock_A);   // always A first
    pthread_mutex_lock(&lock_B);   // then B
    // ... critical section ...
    pthread_mutex_unlock(&lock_B);
    pthread_mutex_unlock(&lock_A);
}
```

**Why it works**: circular wait is impossible — to hold B, you must already hold A.

<div class="text-right text-sm text-gray-400">📁 Skeleton: `examples/skeletons/lab2_deadlock_fix_ordering.c`</div>
<div class="text-right text-sm text-gray-400">📁 Solution: `examples/solutions/lab2_deadlock_fix_ordering.c`</div>

---

# Exercise 3: trylock Strategy

**`pthread_mutex_trylock`** — non-blocking; returns `EBUSY` if unavailable

```c
void *thread_trylock(void *arg) {
  while (1) {
    pthread_mutex_lock(&lock_A);
    if (pthread_mutex_trylock(&lock_B) == 0) {
      // Success — hold both locks
      // ... critical section ...
      pthread_mutex_unlock(&lock_B);
      pthread_mutex_unlock(&lock_A);
      break;
    }
    // Failed — release A and back off
    pthread_mutex_unlock(&lock_A);
    usleep(rand() % 1000);   // randomized delay
  }
}
```

```mermaid
graph TD
    A["Lock A"] -->|"trylock B?"| D{Success?}
    D -->|Yes| CS["Critical Section ✅"]
    D -->|No| R["Release A<br/>Random backoff"]
    R --> A
    style CS fill:#c8e6c9
    style R fill:#fff3e0
```

**Key**: randomized back-off prevents **livelock** (both retrying in lockstep).

<div class="text-right text-sm text-gray-400">📁 Skeleton: `examples/skeletons/lab3_deadlock_fix_trylock.c`</div>
<div class="text-right text-sm text-gray-400">📁 Solution: `examples/solutions/lab3_deadlock_fix_trylock.c`</div>

---

# Exercise 4: xv6 Lock Ordering

**xv6 enforces a strict global lock hierarchy:**

```mermaid
graph TD
    WL["wait_lock<br/>(global)"] --> PL["p->lock<br/>(per-process)"]
    PL --> TL["tickslock<br/>(timer)"]
    style WL fill:#bbdefb
    style PL fill:#e3f2fd
    style TL fill:#e8f5e9
```

**Example: `wait()` and `exit()` both follow the same order:**

```c
// wait()                          // exit()
acquire(&wait_lock);    // 1st     acquire(&wait_lock);    // 1st
  acquire(&p->lock);    // 2nd       acquire(&p->lock);    // 2nd
  if (p->state == ZOMBIE)              p->state = ZOMBIE;
  release(&p->lock);                   wakeup(p->parent);
release(&wait_lock);                 release(&p->lock);
                                   release(&wait_lock);
```

- Violating order (holding `p->lock` then acquiring `wait_lock`) → circular wait
- `acquire()` in xv6 **panics** on double-acquire (catches mistakes early)

---

# Key Takeaways

```mermaid
graph TD
    DL["🔴 Deadlock<br/>4 Coffman conditions"] -->|"break circular wait"| LO["🟢 Lock Ordering<br/>global total order"]
    DL -->|"detect & retreat"| TL["🟡 trylock + backoff"]
    LO --> XV["🔵 xv6 kernel<br/>wait_lock → p->lock"]
    style DL fill:#ffcdd2
    style LO fill:#c8e6c9
    style TL fill:#fff3e0
    style XV fill:#bbdefb
```

| Strategy | When to Use |
|---|---|
| **Lock ordering** | Default choice — simple, provably correct |
| **trylock + backoff** | When strict ordering is impractical |
| **xv6 discipline** | Every kernel path follows documented lock hierarchy |
