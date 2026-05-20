---
theme: default
title: "Week 2 Supplement — Process Memory Layout During fork()"
info: "Operating Systems"
class: text-center
drawings:
  persist: false
transition: slide-left
---

# Week 2 Supplement

## Process Memory Layout During fork()

<div class="pt-4 text-gray-400">
Operating Systems — Supplementary Material
</div>

---

# 1. Process Memory Layout

<div class="grid grid-cols-2 gap-8">
<div>

```
+--------------------+  High address
|   Stack            |  Local variables,
|                    |  function call frames
+--------------------+
|        |           |
|        v           |  Stack grows downward
|                    |
|        ^           |  Heap grows upward
|        |           |
+--------------------+
|   Heap             |  malloc() memory
+--------------------+
|   Data / BSS       |  Global & static vars
+--------------------+
|   Text (code)      |  Machine instructions
+--------------------+  Low address
```

</div>
<div class="text-left text-sm">

| Region | Contents | Permission |
|--------|----------|------------|
| **Text** | Compiled machine instructions | **Read-only**, executable |
| **Data/BSS** | Global & static variables | Read-write |
| **Heap** | `malloc()`/`free()` managed | Read-write |
| **Stack** | Local vars, return addresses | Read-write |

<br>

Every process has its own **virtual address space**, divided into these four main regions.

</div>
</div>

---

# 2. What Happens When fork() Is Called?

<div class="text-left text-base leading-8">

### Key point: The child is NOT "inside" the parent

A common misconception is that the child process lives inside the parent's memory.

In reality, the OS creates a **completely separate process** with its own:

- PCB (Process Control Block)
- Virtual address space
- Page table

</div>

```
Kernel Process Table
-----------------------------------------
PID 200  parent    PPID=1     RUNNING
PID 201  child     PPID=200   RUNNABLE
-----------------------------------------
```

---

# fork() Creates Side-by-Side Processes

<div class="text-left">

The two processes exist **side by side**, not nested:

</div>

```
   Parent Process Memory          Child Process Memory
  +------------------+           +------------------+
  | Stack            |           | Stack            |
  +------------------+           +------------------+
  | Heap             |           | Heap             |
  +------------------+           +------------------+
  | Data             |           | Data             |
  +------------------+           +------------------+
  | Text             |----+----->| Text (shared)    |
  +------------------+    |      +------------------+
                          |
                  Same physical pages
```

<div class="text-left pt-4">

- Text segment: **shared** (same physical pages)
- Data, Heap, Stack: **separated** (independent copies)

</div>

---

# 3. Why Is Text Shared But Data Is Not?

<div class="grid grid-cols-2 gap-6">
<div>

### Text is immutable (read-only)

```asm
main:
  push rbp
  mov  rbp, rsp
  call printf    ; Same for parent & child
```

```
Physical Memory

  [ bash text code ]
       ^       ^
     proc1   proc2   <-- no conflict
```

**Benefits:**
- Memory savings (10 bash → 1 copy)
- Faster fork() (no text copy needed)
- Better cache performance

</div>
<div>

### Data is mutable (read-write)

```c
int counter = 0;   // global (Data segment)
counter++;         // parent increments to 1
```

If parent and child shared data:

```
parent: counter++  ->  counter = 1
child:  counter++  ->  counter = 2
// WRONG! child should see 1, not 2
```

Sharing mutable data would **break process isolation** — each process must have its own independent copy.

</div>
</div>

---

# fork() Behavior by Region

<div class="text-left text-lg leading-10 pt-8">

| Region | Mutable? | fork() behavior |
|--------|----------|-----------------|
| **Text** | No (read-only) | **Shared** — same physical pages |
| **Data** | Yes | **Separated** (via copy-on-write) |
| **Heap** | Yes | **Separated** (via copy-on-write) |
| **Stack** | Yes | **Separated** (via copy-on-write) |

</div>

---

# 4. Copy-on-Write (COW)

<div class="text-left text-base leading-7">

Even though data/heap/stack must be independent, the OS **doesn't copy them immediately** at fork() time. Instead, it uses **Copy-on-Write (COW)**.

</div>

<div class="grid grid-cols-2 gap-8 pt-4">
<div>

### Step 1: fork() — share everything

```
Parent  ----\
              > same physical page
Child   ----/   (marked read-only)
```

Both parent and child point to the **same physical page**, but page table entries have **write permission removed**.

</div>
<div>

### Step 2: First write — copy on demand

1. CPU triggers a **page fault**
2. OS detects this is a COW page
3. OS allocates a **new physical page** and copies content
4. OS updates page table → new page
5. OS restores write permission
6. Write instruction is **retried**

```
Parent -> [new copy]    (read-write)
Child  -> [original]    (still read-only)
```

</div>
</div>

---

# Why COW?

<div class="text-left text-lg leading-10 pt-4">

- **fork() becomes nearly instant**: only page table entries are copied, not actual data

- **fork() + exec() pattern**: if the child immediately calls `exec()`, the entire address space is replaced anyway — copying pages would be wasted work

- **Memory efficient**: pages that are only read (never written) are never copied

</div>

---

# 5. Analogy: Chefs in a Kitchen

<div class="text-left text-lg leading-10 pt-4">

| Concept | Analogy |
|---------|---------|
| **Text** (code) | **Recipe book** — all chefs read the same book, no conflict |
| **Data** (variables) | **Ingredients** — each chef needs their own set |
| **fork()** | **Hiring a new chef** — they get the recipe book (shared) and their own ingredients (separate) |
| **COW** | Ingredients are labeled "shared" until someone starts cooking — then they get their own portion |

</div>

---

# 6. What About exec()?

<div class="text-left">

When a process calls `exec()`, the **entire** address space is replaced:

</div>

```
Before exec():                After exec("ls"):
+------------------+         +------------------+
| Stack (old)      |         | Stack (new)      |
+------------------+         +------------------+
| Heap  (old)      |         | Heap  (new)      |
+------------------+         +------------------+
| Data  (old)      |         | Data  (new)      |
+------------------+         +------------------+
| Text  (old)      |         | Text  (ls code)  |
+------------------+         +------------------+
```

<div class="text-left pt-4">

- Old text, data, heap, and stack are all **discarded**
- New program's code and data are loaded from the executable
- **PID stays the same** — same process, different program
- This is why **fork() + exec()** benefits enormously from COW

</div>

---

# 7. Shared Libraries: Text Sharing Extended

<div class="text-left pt-4">

Shared libraries (`.so` on Linux, `.dylib` on macOS) extend text sharing across **unrelated** processes:

</div>

```
Process A (bash)     Process B (python)     Process C (gcc)
     |                    |                      |
     +-----> libc.so text <------+               |
                                  |               |
                                  +--> libc.so text (same pages!)
```

<div class="text-left pt-4 text-base leading-8">

- `libc.so` code pages are loaded **once** into physical memory
- Every process that uses libc maps those **same physical pages**
- Saves huge amounts of memory system-wide

</div>

---

# Key Takeaways

<div class="text-left text-lg leading-10 pt-4">

1. **Child process is independent**, not inside the parent — it gets its own address space

2. **Text is shared** because code is read-only and identical across processes

3. **Data/heap/stack are separated** because each process must modify them independently

4. **Copy-on-Write** delays actual copying until a write occurs, making fork() fast

5. **fork() + exec()** is efficient because COW avoids copying pages that exec() will discard

6. **Shared libraries** extend text sharing across unrelated processes

</div>
