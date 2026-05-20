---
theme: default
title: "Week 12 — Lab: COW Fork & Lazy Allocation"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 12 — COW Fork & Lazy Allocation

Korea University Sejong Campus, Department of Computer Science & Software

---

# Lab Overview

- **Goal**: Understand page fault handling and design virtual memory optimizations
- **Duration**: ~60 minutes · 3 exercises
- **Key insight**: Page faults are **not errors** — they are opportunities to defer and share work

```mermaid
graph LR
    E1["Ex 1<br/>Page Fault<br/>Handling"] --> E2["Ex 2<br/>COW Fork"]
    E2 --> E3["Ex 3<br/>Lazy<br/>Allocation"]
    style E1 fill:#e3f2fd
    style E2 fill:#fff3e0
    style E3 fill:#e8f5e9
```

---

# Exercise 1: Page Fault Handling

**Flow when a user process accesses an unmapped/protected page:**

```mermaid
graph TD
    TRAP["usertrap()<br/>kernel/trap.c"] --> SC{"r_scause()"}
    SC -->|"13"| LF["Load page fault<br/>(read to unmapped addr)"]
    SC -->|"15"| SF["Store page fault<br/>(write to RO or unmapped)"]
    LF --> VMF["vmfault()"]
    SF --> VMF
    VMF --> D{"What kind?"}
    D -->|"PTE_COW set"| COW["COW: copy page,<br/>remap writable"]
    D -->|"addr < p->sz,<br/>no PTE"| LAZY["Lazy: allocate<br/>page on demand"]
    D -->|"invalid"| KILL["Kill process"]
    style TRAP fill:#e3f2fd
    style COW fill:#fff3e0
    style LAZY fill:#e8f5e9
    style KILL fill:#ffcdd2
```

- `r_stval()` returns the **faulting virtual address**
- **Task**: Read `usertrap()` and identify where page faults are dispatched

---

# Exercise 2: COW Fork — Concept

**Problem**: Naive `fork()` copies ALL physical pages immediately — slow and wasteful

```mermaid
graph LR
    subgraph "Naive fork()"
        P1["Parent<br/>pages A,B,C"] -->|"copy all"| C1["Child<br/>pages A',B',C'"]
    end
    subgraph "COW fork()"
        P2["Parent<br/>pages A,B,C<br/>(read-only)"] -->|"share!"| C2["Child<br/>same A,B,C<br/>(read-only)"]
        C2 -->|"write to B"| FAULT["page fault"]
        FAULT --> COPY["copy only B → B'"]
    end
    style C1 fill:#ffcdd2
    style C2 fill:#c8e6c9
    style COPY fill:#fff3e0
```

**Why it works**: Most `fork()` children call `exec()` immediately → pages are **never** written → **zero copies** needed!

<div class="text-right text-sm text-gray-400">📁 Skeleton: <code>examples/skeletons/lab2_cow_concept.c</code></div>
<div class="text-right text-sm text-gray-400">📁 Solution: <code>examples/solutions/lab2_cow_concept.c</code></div>

---

# Exercise 2: COW Fork — Implementation

<div class="grid grid-cols-2 gap-4">
<div>

**Step 1 — `uvmcopy()`: mark PTEs**

```c
*pte &= ~PTE_W;   // clear writable
*pte |= PTE_COW;  // set COW flag
// (RSW bit 8 in PTE)
// child PTE gets same flags
```

**Step 2 — Reference counting**

```c
int refcount[PHYSTOP / PGSIZE];
// increment on fork
// decrement on free
// kfree() only when count → 0
```

</div>
<div>

**Step 3 — Fault handler**

```mermaid
graph TD
    F["Store page fault<br/>scause = 15"] --> C{"PTE_COW?"}
    C -->|Yes| A["kalloc() new page"]
    A --> CP["memmove() copy content"]
    CP --> RM["Remap PTE:<br/>writable, clear COW"]
    RM --> DEC["decref(old_pa)"]
    C -->|No| K["Kill process"]
    style F fill:#fce4ec
    style A fill:#fff3e0
    style RM fill:#c8e6c9
    style K fill:#ffcdd2
```

```c
if (*pte & PTE_COW) {
  char *mem = kalloc();
  memmove(mem, old_pa, PGSIZE);
  *pte = PA2PTE(mem) | PTE_W;
  *pte &= ~PTE_COW;
  decref(old_pa);
}
```

</div>
</div>

---

# Exercise 3: Lazy Allocation

**Problem**: `sbrk(n)` allocates `n` bytes of physical memory even if the process never uses it

```mermaid
graph TD
    subgraph "Eager (current xv6)"
        SB1["sbrk(n)"] --> AL["Allocate n bytes<br/>of physical pages"]
        AL --> MAP["Map all pages<br/>in page table"]
    end
    subgraph "Lazy (optimization)"
        SB2["sbrk(n)"] --> SZ["Just update p->sz += n<br/>NO physical alloc"]
        SZ --> ACC["First access"]
        ACC --> PF["Page fault"]
        PF --> ALP["kalloc() + mappages()<br/>allocate ONE page"]
    end
    style AL fill:#ffcdd2
    style SZ fill:#c8e6c9
    style ALP fill:#fff3e0
```

**Edge cases to handle**:
- `sbrk(-n)`: must `uvmdealloc()` existing pages
- Stack guard page: must still fault fatally
- Syscall I/O: `walkaddr()` must trigger allocation for lazy pages passed to `read()`/`write()`

<div class="text-right text-sm text-gray-400">📁 Skeleton: <code>examples/skeletons/lab3_lazy_concept.c</code></div>
<div class="text-right text-sm text-gray-400">📁 Solution: <code>examples/solutions/lab3_lazy_concept.c</code></div>

---

# Key Takeaways

```mermaid
graph TD
    PF["Page Fault<br/>(scause 13/15)"] -->|"mechanism"| UT["usertrap() → vmfault()"]
    UT --> COW["COW Fork<br/>share pages RO,<br/>copy on write"]
    UT --> LAZY["Lazy Alloc<br/>grow p->sz only,<br/>alloc on fault"]
    COW --> REF["Reference counting<br/>prevents premature kfree"]
    style PF fill:#e3f2fd
    style COW fill:#fff3e0
    style LAZY fill:#e8f5e9
```

| Concept | Key Insight |
|---|---|
| **Page faults** | Not errors — opportunities to defer work |
| **COW fork** | Share pages RO + PTE_COW; copy only on write |
| **Refcount** | Track sharing; `kfree` only when count reaches 0 |
| **Lazy alloc** | `sbrk` updates `p->sz` only; physical pages on demand |
| **Pitfall** | Kernel must handle lazy pages in `walkaddr()` for syscall I/O |
