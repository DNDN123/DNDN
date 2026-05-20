# Week 12 Lab: Virtual Memory - COW Fork & Lazy Allocation

## Learning Objectives

In this lab, we study **Copy-On-Write (COW) Fork** and **Lazy Allocation**, two key optimization techniques for virtual memory. Both strategies intentionally leverage **page faults** to improve performance.

After completing this lab, you will understand:
- The complete flow of how a page fault is generated and handled on RISC-V
- The principle of COW fork and its implementation design in xv6
- The principle of lazy allocation and its implementation design in xv6
- Memory management techniques using PTE flags

**Reference:** xv6 book Chapter 4.6

---

## Background: Page Fault

### What Is a Page Fault?

When a process accesses an invalid virtual address, the MMU (Memory Management Unit) raises a **page fault** exception. "Invalid" includes the following cases:

1. No PTE (Page Table Entry) exists for the virtual address (PTE_V = 0)
2. A PTE exists but the access permissions are insufficient (e.g., attempting to write to a read-only page)
3. Accessing a kernel-only page from user mode (PTE_U = 0)

### RISC-V scause Values (Page Fault Related)

When a trap occurs on RISC-V, the cause is recorded in the `scause` register:

| scause Value | Name | Description |
|-----------|------|------|
| **12** | Instruction page fault | Page fault during instruction fetch |
| **13** | Load page fault | Page fault during memory read (load) |
| **15** | Store/AMO page fault | Page fault during memory write (store) |

### stval Register

When a page fault occurs, the `stval` (Supervisor Trap Value) register stores the **virtual address that caused the fault**. The kernel uses this value to determine which page triggered the fault.

### xv6 PTE Flags (kernel/riscv.h)

```c
#define PTE_V (1L << 0) // valid - valid PTE
#define PTE_R (1L << 1) // read  - read permission
#define PTE_W (1L << 2) // write - write permission
#define PTE_X (1L << 3) // execute - execute permission
#define PTE_U (1L << 4) // user - user mode access permission
```

RISC-V PTE Structure (Sv39):
```
63    54 53    28 27    19 18    10 9  8 7 6 5 4 3 2 1 0
+------+--------+--------+--------+----+-+-+-+-+-+-+-+-+
|  (0) | PPN[2] | PPN[1] | PPN[0] | RSW|D|A|G|U|X|W|R|V|
+------+--------+--------+--------+----+-+-+-+-+-+-+-+-+
```

- **RSW** (bits 8-9): Reserved for Software - bits that the OS can use freely
- These RSW bits can be used for COW marking

---

## Exercise 1: Tracing the Page Fault Handling Flow (10 min)

### Goal
Understand how the xv6 kernel handles a page fault by tracing through the code.

### 1.1 usertrap() Analysis

Let's examine the `usertrap()` function in `kernel/trap.c`:

```c
uint64
usertrap(void)
{
  // ... (omitted) ...

  struct proc *p = myproc();
  p->trapframe->epc = r_sepc();  // Save the PC at the time of the fault

  if(r_scause() == 8){
    // System call handling
    // ...
  } else if((which_dev = devintr()) != 0){
    // Device interrupt handling
  } else if((r_scause() == 15 || r_scause() == 13) &&
            vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
    // Page fault handling (lazy allocation)
  } else {
    // Unhandled exception -> kill the process
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    setkilled(p);
  }
  // ...
}
```

**Question 1-1:** What condition does `usertrap()` use to detect a page fault?
- A page fault is identified when the `r_scause()` value is `13` (load page fault) or `15` (store page fault).

**Question 1-2:** How can we determine the virtual address that caused the page fault?
- By calling `r_stval()`, which reads the faulting virtual address from the `stval` register.

**Question 1-3:** Why is `scause == 12` (instruction page fault) not handled?

> **Think about it:** Under what circumstances could an instruction page fault occur during normal program execution? Why is it reasonable for xv6 not to handle it separately?

### 1.2 vmfault() Analysis

Let's analyze the `vmfault()` function currently implemented in xv6 (`kernel/vm.c`):

```c
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();

  if (va >= p->sz)           // (1) Check if address is within process address space
    return 0;
  va = PGROUNDDOWN(va);      // (2) Align to page boundary
  if(ismapped(pagetable, va)) {  // (3) Check if page is already mapped
    return 0;
  }
  mem = (uint64) kalloc();   // (4) Allocate a physical page
  if(mem == 0)
    return 0;
  memset((void *) mem, 0, PGSIZE);  // (5) Initialize to zero
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);      // (6) Map in page table
    return 0;
  }
  return mem;
}
```

**Question 1-4:** What function does `vmfault()` currently perform?

> This function is a page fault handler for **lazy allocation**. When a process accesses a page whose virtual address space was expanded by `sbrk()` but for which physical memory has not yet been allocated, it allocates physical memory and creates the mapping at that point.

**Question 1-5:** Describe the steps performed by `vmfault()` in order.

| Step | Code | Description |
|------|------|------|
| (1) | `va >= p->sz` | Check if the fault address is within the process memory range |
| (2) | `PGROUNDDOWN(va)` | Align the virtual address to the start of the page |
| (3) | `ismapped()` | If the page is already mapped, no handling needed (different cause) |
| (4) | `kalloc()` | Allocate a new physical page |
| (5) | `memset(0)` | Initialize to zero for security |
| (6) | `mappages()` | Add the virtual-to-physical address mapping to the page table |

### 1.3 Complete Page Fault Handling Flow Diagram

```
User program: memory access (e.g., *ptr = 42;)
        |
        v
   [MMU] PTE check -> invalid!
        |
        v
   [Hardware] scause = 15 (store page fault)
              stval = faulting virtual address
              sepc = faulting instruction address
        |
        v
   [trampoline.S] uservec -> save registers
        |
        v
   [trap.c] usertrap()
        |--- scause == 8? --> syscall handling
        |--- devintr()?    --> device interrupt handling
        |--- scause == 13 or 15? --> call vmfault()
        |         |
        |         |--- success: allocate physical memory + create mapping
        |         |--- failure: kill the process
        |
        v
   [trampoline.S] userret -> return to user mode
                              (re-execute the same instruction -> succeeds this time!)
```

**Key Point:** When returning to the user program after handling a page fault, the `epc` still points to the instruction that caused the fault, so **the same instruction is re-executed**. This time the mapping is in place, so it succeeds. (For system calls, `epc += 4` advances to the next instruction, but for page faults this is not done.)

---

## Exercise 2: Copy-On-Write (COW) Fork Design (20 min)

### 2.1 Problem with the Current fork

The current xv6 `kfork()` (`kernel/proc.c`) calls `uvmcopy()` to **immediately copy all pages** of the parent process:

```c
// kernel/vm.c - uvmcopy()
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)       // <-- allocate new physical memory for every page
      goto err;
    memmove(mem, (char*)pa, PGSIZE); // <-- copy all 4KB
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;
  // ...
}
```

**Problem:** When `exec()` is called immediately after `fork()` (e.g., running a command from the shell), the copied memory is discarded right away. This unnecessary copying significantly degrades performance.

**Question 2-1:** When a process using 100 pages (400KB) calls fork(), how many times are `kalloc()` and `memmove()` called in the current implementation?

> `kalloc()` 100 times, `memmove()` 100 times (4KB each, 400KB total copied)

### 2.2 Core Idea of COW Fork

```
[ Current fork ]                        [ COW fork ]

  Parent PT      Child PT                Parent PT      Child PT
  +----+      +----+                  +----+      +----+
  | VA | -->  | VA | --> new phys page   | VA | -+   | VA | -+
  +----+      +----+    (content copy)  +----+  |   +----+  |
                                              |           |
                                              +---> same physical page <---+
                                                   (shared as read-only)
```

**COW fork behavior:**
1. At `fork()` time: instead of copying pages, the parent and child **share the same physical pages**
2. Both processes set those pages to **read-only**
3. When either side **attempts a write**, a store page fault occurs
4. The page fault handler **copies the page at that point** and grants write permission

### 2.3 COW Fork Implementation Design

#### Step 1: Define the COW Bit

Use a RSW (Reserved for Software) bit in the PTE for COW marking.

Add to `kernel/riscv.h`:
```c
#define PTE_COW (1L << 8)  // Use one of the RSW bits as a COW marker
```

**Question 2-2:** Which bits of the PTE are the RSW bits? How does the hardware treat these bits?

> RSW is bits 8-9. The hardware (MMU) ignores these bits. Therefore, the OS can use them freely for any purpose.

#### Step 2: Modify uvmcopy() (COW Version)

Modify the existing `uvmcopy()` as follows:

```c
// Modified uvmcopy() for COW fork (pseudocode)
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    // [Key change] Change writable pages to read-only + COW marked
    if(flags & PTE_W) {
      flags = (flags & ~PTE_W) | PTE_COW;  // Remove W, add COW
      *pte = PA2PTE(pa) | flags;           // Modify the parent's PTE too!
    }

    // Map the same physical page in the child's page table (no copy!)
    if(mappages(new, i, PGSIZE, pa, flags) != 0)
      goto err;

    // Increment reference count (explained below)
    krefpage((void*)pa);
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

**Question 2-3:** Why must the parent's PTE also be modified?

> Because the parent might attempt a write first. The parent's PTE must also be changed to read-only + COW so that whichever side (parent or child) attempts a write first triggers a page fault that causes the copy.

#### Step 3: Handle COW in the Page Fault Handler

The following logic is performed in `usertrap()` or a separate COW fault handler:

```c
// COW page fault handling pseudocode
int
cowfault(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa, flags;
  char *mem;

  va = PGROUNDDOWN(va);

  // 1. Find the PTE
  pte = walk(pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0)
    return -1;

  // 2. Check if it is a COW page
  if((*pte & PTE_COW) == 0)
    return -1;  // Write fault on a non-COW page -> real error

  pa = PTE2PA(*pte);
  flags = PTE_FLAGS(*pte);

  // 3. Check reference count
  if(kgetrefcnt((void*)pa) == 1) {
    // If this process is the only one using this physical page, no copy needed
    // Just modify the PTE flags: remove COW, restore W
    *pte = PA2PTE(pa) | ((flags & ~PTE_COW) | PTE_W);
    return 0;
  }

  // 4. Allocate a new physical page and copy
  if((mem = kalloc()) == 0)
    return -1;
  memmove(mem, (char*)pa, PGSIZE);

  // 5. Update PTE: new physical address, remove COW, restore W
  *pte = PA2PTE(mem) | ((flags & ~PTE_COW) | PTE_W);

  // 6. Decrement reference count of the old physical page (free if it reaches 0)
  kfree((void*)pa);

  return 0;
}
```

**Question 2-4:** How does `usertrap()` detect a COW fault?

> When `scause == 15` (store page fault) and the corresponding PTE has the `PTE_COW` flag set, it is a COW fault.

#### Step 4: Reference Count Management

In COW, multiple processes share a single physical page, so the page must only be freed when no process uses it anymore. This requires a **reference count**.

```c
// Content to add to kernel/kalloc.c (pseudocode)

struct {
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];  // Reference count for each physical page
} refcnt;

// Increment reference count
void krefpage(void *pa) {
  acquire(&refcnt.lock);
  refcnt.cnt[(uint64)pa / PGSIZE]++;
  release(&refcnt.lock);
}

// Get reference count
int kgetrefcnt(void *pa) {
  int cnt;
  acquire(&refcnt.lock);
  cnt = refcnt.cnt[(uint64)pa / PGSIZE];
  release(&refcnt.lock);
  return cnt;
}

// Modified kfree(): only actually free when reference count reaches 0
void kfree(void *pa) {
  acquire(&refcnt.lock);
  refcnt.cnt[(uint64)pa / PGSIZE]--;
  if(refcnt.cnt[(uint64)pa / PGSIZE] > 0) {
    release(&refcnt.lock);
    return;  // Still in use by another process
  }
  release(&refcnt.lock);
  // ... existing free logic ...
}
```

**Question 2-5:** Why is a lock needed for the reference count array?

> In a multi-CPU environment, multiple processes can simultaneously fork/exit and modify the reference count of the same physical page. A spinlock is needed to prevent data races.

### 2.4 Summary of Files to Modify for COW Fork

| File | Modification |
|------|----------|
| `kernel/riscv.h` | Define the `PTE_COW` macro |
| `kernel/kalloc.c` | Add reference count array and management functions, modify `kfree()` |
| `kernel/vm.c` | Modify `uvmcopy()` (share instead of copy), add `cowfault()` |
| `kernel/trap.c` | Add COW fault handling branch in `usertrap()` |
| `kernel/defs.h` | Declare prototypes for newly added functions |

---

## Exercise 3: Lazy Allocation Design (10 min)

### 3.1 Current sbrk() Behavior

Let's examine `sys_sbrk()` (`kernel/sysproc.c`):

```c
uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {       // Immediate allocation (eager allocation)
      return -1;
    }
  } else {
    // Lazy allocation: only increase size, do not allocate physical memory
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;          // Only change the process size!
  }
  return addr;
}
```

**Question 3-1:** What is the difference between `SBRK_EAGER` mode and lazy allocation?

> `SBRK_EAGER`: calls `growproc()` -> `uvmalloc()` to immediately allocate physical memory and map it in the page table. Lazy: only increases `p->sz` without allocating physical memory.

### 3.2 Core Idea of Lazy Allocation

```
[ Eager Allocation (existing) ]          [ Lazy Allocation ]

sbrk(4096) called                        sbrk(4096) called
  |                                        |
  v                                        v
kalloc() -> allocate physical page         p->sz += 4096 (only change size)
mappages() -> create PTE                   No PTE, no physical page
  |                                        |
  v                                        v
Ready to use immediately                   Page fault on access!
                                           |
                                           v
                                         vmfault() calls kalloc() + mappages()
                                           |
                                           v
                                         Now ready to use
```

**Advantage:** A program may not use all the memory it allocates. Lazy allocation only consumes physical memory for pages that are actually accessed, resulting in higher memory efficiency.

### 3.3 Considerations for Lazy Allocation Implementation

Lazy allocation is already partially implemented in the current xv6. Let's examine the points that need to be considered for a complete implementation.

#### Consideration 1: Handling in uvmunmap()

When freeing a lazily allocated region, there may be PTEs for which physical pages have not yet been allocated:

```c
// kernel/vm.c - uvmunmap() (current implementation)
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  // ...
  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      continue;       // <-- lazy: skip if PTE does not exist
    if((*pte & PTE_V) == 0)
      continue;       // <-- lazy: skip if not valid
    // ...
  }
}
```

**Question 3-2:** The original xv6 `uvmunmap()` called `panic()` when a PTE was missing or invalid. Why must this be changed to `continue` to support lazy allocation?

> With lazy allocation, there are pages whose address space was expanded by `sbrk()` but for which physical pages have not yet been allocated. When a process exits, `uvmunmap()` is called, and it must not panic on unallocated pages.

#### Consideration 2: Handling in copyin/copyout

When the kernel accesses user memory (`copyin`, `copyout`), it may encounter lazily allocated pages:

```c
// kernel/vm.c - copyout() (current implementation)
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  // ...
  pa0 = walkaddr(pagetable, va0);
  if(pa0 == 0) {
    if((pa0 = vmfault(pagetable, va0, 0)) == 0) {  // <-- handle lazy page
      return -1;
    }
  }
  // ...
}
```

**Question 3-3:** What happens when a page fault occurs while kernel code is accessing a user address? Why is the approach of calling `vmfault()` directly in `copyout()` used?

> Page faults in kernel mode are handled by `kerneltrap()`, which by default triggers a panic (considered a kernel code bug). Therefore, when the kernel accesses lazily allocated user memory, it does not rely on a hardware page fault. Instead, it checks the mapping with `walkaddr()`, and if no mapping exists, explicitly calls `vmfault()` to perform the allocation.

#### Consideration 3: Shrinking Memory with sbrk (n < 0)

```c
if(t == SBRK_EAGER || n < 0) {
  if(growproc(n) < 0) {
    return -1;
  }
}
```

**Question 3-4:** Why is the case `n < 0` not handled lazily but processed immediately?

> When shrinking memory, physical pages must actually be freed to reclaim memory immediately. Handling this lazily could result in failure to properly free memory that is already in use.

### 3.4 Complete Lazy Allocation Flow

```
1. sbrk(n) called (n > 0, SBRK_LAZY)
   -> p->sz += n (only change size, no physical memory allocation)

2. Program accesses the newly allocated region
   -> MMU: no PTE -> store/load page fault occurs
   -> scause = 13 or 15, stval = fault address

3. usertrap() -> vmfault() called
   -> Check va < p->sz (verify it is a valid address)
   -> Check ismapped() (confirm it is not yet mapped)
   -> kalloc() + memset(0) + mappages()

4. usertrap() returns -> return to user mode
   -> Re-execute same instruction -> succeeds this time
```

---

## Summary and Key Takeaways

### Comparison of Page Fault Utilization Techniques

| | COW Fork | Lazy Allocation |
|---|---------|-----------------|
| **Purpose** | Avoid unnecessary memory copying during fork | Avoid allocating unused memory |
| **Timing** | At fork() time | At sbrk() time |
| **Behavior** | Share first, copy on write | No allocation, allocate on access |
| **Fault type** | Store page fault (scause=15) | Load/Store page fault (scause=13,15) |
| **PTE change** | Remove W, set COW bit | PTE does not exist at all |
| **Additional data structure** | Reference count array | None (range determined by p->sz) |
| **Key functions** | uvmcopy(), cowfault() | sys_sbrk(), vmfault() |

### Key Concepts

1. **A page fault is not an error but an opportunity**
   - Traditionally, a page fault meant an invalid memory access by the program, but modern operating systems actively use it as a **mechanism for memory management optimization**.

2. **The value of deferral**
   - Both COW and lazy allocation follow the principle of "defer work that does not need to be done right now."
   - Deferring expensive operations until they are actually needed improves overall performance.

3. **Hardware-software cooperation**
   - The MMU checks PTEs and raises page faults (hardware), and the kernel's trap handler handles them appropriately (software). The PTE's RSW bits are an interface designed for this cooperation.

4. **Importance of TLB flush**
   - After modifying a PTE, you must flush the TLB with `sfence_vma()`. Otherwise, the MMU may use the cached old PTE and behave incorrectly.
