---
theme: default
title: "Week 11 — Lab: Page Tables"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Operating Systems Lab

## Week 11 — Page Tables

Korea University Sejong Campus, Department of Computer Science & Software

---

# Lab Overview

**Objectives**: Trace address translation and implement `vmprint()` in xv6

| # | Topic | Time |
|---|-------|------|
| 1 | Kernel memory map — `kvmmake()` | 10 min |
| 2 | `walk()` tracing — manual translation | 15 min |
| 3 | `vmprint()` implementation | 15 min |
| 4 | `sbrk()` behavior tracing | 10 min |

```mermaid
graph LR
    E1["Ex 1<br/>Kernel Map"] --> E2["Ex 2<br/>walk()"]
    E2 --> E3["Ex 3<br/>vmprint()"]
    E3 --> E4["Ex 4<br/>sbrk()"]
    style E1 fill:#e3f2fd
    style E2 fill:#fff3e0
    style E3 fill:#e8f5e9
    style E4 fill:#fce4ec
```

---

# Exercise 1: Kernel Memory Map

**`kvmmake()`** — `kernel/vm.c` — builds kernel page table at boot

```mermaid
graph TD
    subgraph "Virtual Address Space"
        direction TB
        TR["TRAMPOLINE<br/>(MAXVA - PGSIZE)"]
        FREE["Free memory"]
        ETEXT["etext → PHYSTOP<br/>Kernel data (R/W)"]
        KERN["KERNBASE → etext<br/>Kernel code (R/X)"]
        VIRTIO["VIRTIO0 (R/W)"]
        UART["UART0 (R/W)"]
    end
    subgraph "Physical Address Space"
        direction TB
        PTR["trampoline page"]
        PFREE["Free memory"]
        PETEXT["Kernel data"]
        PKERN["0x80000000<br/>Kernel code"]
        PVIRTIO["VIRTIO"]
        PUART["UART"]
    end
    UART ---|"VA == PA"| PUART
    VIRTIO ---|"VA == PA"| PVIRTIO
    KERN ---|"VA == PA"| PKERN
    ETEXT ---|"VA == PA"| PETEXT
    TR -.->|"remapped"| PTR
    style TR fill:#fce4ec
```

**Direct mapping**: `VA == PA` for most kernel regions — simplifies pointer arithmetic.
**Exception**: trampoline mapped at top of VA space, shared across all processes.

---

# Exercise 2: walk() Tracing — Sv39

**RISC-V Sv39**: 39-bit virtual address → 3-level page table

```mermaid
graph LR
    VA["Virtual Address<br/>39 bits"] --> VPN2["VPN[2]<br/>9 bits"]
    VA --> VPN1["VPN[1]<br/>9 bits"]
    VA --> VPN0["VPN[0]<br/>9 bits"]
    VA --> OFF["Offset<br/>12 bits"]
    VPN2 -->|"index into"| L2["L2 Page Table<br/>(512 PTEs)"]
    L2 -->|"PTE → PA"| L1["L1 Page Table<br/>(512 PTEs)"]
    VPN1 -->|"index into"| L1
    L1 -->|"PTE → PA"| L0["L0 Page Table<br/>(512 PTEs)"]
    VPN0 -->|"index into"| L0
    L0 -->|"PTE → PA"| PHY["Physical Page"]
    OFF -->|"add offset"| ADDR["Physical Address"]
    PHY --> ADDR
    style VA fill:#e3f2fd
    style L2 fill:#fff3e0
    style L1 fill:#fff3e0
    style L0 fill:#fff3e0
    style PHY fill:#c8e6c9
```

**`walk(pagetable, va, alloc)`** — `kernel/vm.c`:
- Traverses 3 levels using `PX(level, va)` to extract VPN bits
- `alloc=1` → allocates missing intermediate pages on demand
- Returns pointer to the **L0 PTE** for the given virtual address

<div class="text-right text-sm text-gray-400">📁 Skeleton: <code>examples/skeletons/lab2_va_translate.py</code></div>
<div class="text-right text-sm text-gray-400">📁 Solution: <code>examples/solutions/lab2_va_translate.py</code></div>

---

# Exercise 3: vmprint() Implementation

**Goal**: Print a page table in the xv6 book format

```
page table 0x0000000087f6b000
 ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000
 .. ..0: pte 0x0000000021fd9401 pa 0x0000000087f65000
 .. .. ..0: pte 0x0000000021fd98c7 pa 0x0000000087f66000
```

```c
void vmprint_level(pagetable_t pt, int level) {
  for (int i = 0; i < 512; i++) {
    pte_t pte = pt[i];
    if (!(pte & PTE_V)) continue;
    // print with indentation based on level
    printf("%s%d: pte %p pa %p\n",
           indent[level], i, pte, PTE2PA(pte));
    if (level > 0 && (pte & (PTE_R|PTE_W|PTE_X)) == 0)
      vmprint_level((pagetable_t)PTE2PA(pte), level-1);
  }
}
```

```mermaid
graph TD
    L2["L2: iterate 512 PTEs"] -->|"valid & non-leaf"| L1["L1: iterate 512 PTEs"]
    L1 -->|"valid & non-leaf"| L0["L0: iterate 512 PTEs"]
    L0 -->|"leaf PTE"| PRINT["print pa + flags"]
    style L2 fill:#e3f2fd
    style L1 fill:#fff3e0
    style L0 fill:#e8f5e9
```

**Leaf vs non-leaf**: PTE with **no R/W/X** flags = pointer to next level (recurse). PTE with R/W/X = leaf (print).

<div class="text-right text-sm text-gray-400">📁 Skeleton: <code>examples/skeletons/lab3_vmprint.c</code></div>
<div class="text-right text-sm text-gray-400">📁 Solution: <code>examples/solutions/lab3_vmprint.c</code></div>

---

# Exercise 4: sbrk() Behavior Tracing

**`sbrk(n)`** grows the heap → new pages appear in the page table

```mermaid
sequenceDiagram
    participant U as User program
    participant K as Kernel
    participant PT as Page Table
    U->>K: sbrk(4096)
    K->>K: growproc(4096)
    K->>K: uvmalloc(old_sz, old_sz + 4096)
    K->>K: kalloc() → get physical page
    K->>PT: mappages() → walk(alloc=1)
    Note over PT: New L0 PTE created<br/>flags: R|W|U|V
    K->>K: p->sz += PGSIZE
    K->>U: return old_sz
```

**Observation**: Use `vmprint()` **before** and **after** `sbrk()` to see the new PTE appear at the heap boundary.

**What happens inside**:
1. `uvmalloc` calls `kalloc()` for a physical page
2. `mappages` calls `walk(alloc=1)` to create/find PTE entries
3. New L0 PTE: `PTE_R | PTE_W | PTE_U | PTE_V`
4. `p->sz` advances — new VA is now accessible

---

# Key Takeaways

```mermaid
graph TD
    KM["Kernel Map<br/>VA == PA (direct)"] --> W["walk()<br/>3-level Sv39"]
    W --> VP["vmprint()<br/>recursive traversal"]
    VP --> SB["sbrk()<br/>heap growth"]
    style KM fill:#e3f2fd
    style W fill:#fff3e0
    style VP fill:#e8f5e9
    style SB fill:#fce4ec
```

| Concept | Key Insight |
|---|---|
| **Direct mapping** | Kernel VA == PA from KERNBASE up; trampoline is the exception |
| **Sv39** | 3 × 9-bit VPN + 12-bit offset; 512 entries per page table page |
| **walk()** | Traverses 3 levels; `alloc=1` creates missing intermediate pages |
| **vmprint()** | Non-leaf PTE (no R/W/X) → recurse; leaf PTE → print |
| **sbrk()** | `growproc` → `uvmalloc` → `mappages` → `walk` → new PTE |
