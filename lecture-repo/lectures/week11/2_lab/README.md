# Week 11 Lab: Main Memory - Page Tables

## Learning Objectives

Through this lab, you will understand and practice the following:

1. Understand the RISC-V Sv39 3-level page table structure
2. Read and analyze the code for how the xv6 kernel sets up page tables
3. Trace the process of virtual-to-physical address translation through the `walk()` function
4. Implement a `vmprint()` function that prints the contents of a process's page table
5. Trace how the page table changes when memory is allocated via `sbrk()`

**Textbook reference**: xv6 book Chapter 3 (Page Tables)

---

## Background Knowledge: RISC-V Sv39 Paging Structure

### What is a Page Table?

The operating system provides each process with an independent virtual address space. The page table's role is to translate the **virtual addresses (VA)** used by processes into actual memory **physical addresses (PA)**.

RISC-V uses the **Sv39** scheme:
- Virtual address: **39 bits** (512GB address space)
- Physical address: **56 bits**
- Page size: **4KB** (4096 bytes = 2^12 bytes)
- Page table: **3 levels** (Level 2 -> Level 1 -> Level 0)
- Each page table page: **512 PTEs** (2^9 = 512)

### Sv39 Virtual Address Structure (39 bits)

```
  38       30 29       21 20       12 11          0
 +-----------+-----------+-----------+-------------+
 | L2 index  | L1 index  | L0 index  | Page Offset |
 | (9 bits)  | (9 bits)  | (9 bits)  |  (12 bits)  |
 +-----------+-----------+-----------+-------------+
   VPN[2]      VPN[1]      VPN[0]       offset

 Bits 63~39: Unused (must be 0)
```

- **L2 index (VPN[2])**: Bits 38~30 -- Index in the Level-2 page table (0~511)
- **L1 index (VPN[1])**: Bits 29~21 -- Index in the Level-1 page table (0~511)
- **L0 index (VPN[0])**: Bits 20~12 -- Index in the Level-0 page table (0~511)
- **Page Offset**: Bits 11~0 -- Offset within the 4KB page (0~4095)

### 3-Level Page Table Translation Process

```
 satp register
 (physical address
  of L2 table)
      |
      v
 +----------+      +----------+      +----------+
 | L2 Table |      | L1 Table |      | L0 Table |
 |----------|      |----------|      |----------|
 |  PTE  0  |      |  PTE  0  |      |  PTE  0  |
 |  PTE  1  |      |  PTE  1  |      |  PTE  1  |
 |  ...     |      |  ...     |      |  ...     |
 |  PTE  i  |----->|  PTE  j  |----->|  PTE  k  |-----> Physical page
 |  ...     |      |  ...     |      |  ...     |        + offset
 |  PTE 511 |      |  PTE 511 |      |  PTE 511 |
 +----------+      +----------+      +----------+

  i = VPN[2]        j = VPN[1]        k = VPN[0]
  (L2 index)        (L1 index)        (L0 index)
```

**Translation procedure:**
1. Obtain the physical address of the L2 page table from the `satp` register
2. Read the PTE at index `VPN[2]` in the L2 table -> obtain the physical address of the L1 table
3. Read the PTE at index `VPN[1]` in the L1 table -> obtain the physical address of the L0 table
4. Read the PTE at index `VPN[0]` in the L0 table -> obtain the address of the final physical page
5. Physical page address + Page Offset = final physical address

### PTE (Page Table Entry) Structure (64 bits)

```
  63    54 53                    10 9   8 7 6 5 4 3 2 1 0
 +--------+------------------------+-----+-+-+-+-+-+-+-+-+
 |Reserved|   PPN (Physical Page #) |RSW  |D|A|G|U|X|W|R|V|
 | (10)   |       (44 bits)        | (2) | | | | | | | | |
 +--------+------------------------+-----+-+-+-+-+-+-+-+-+
```

**Flag bit descriptions:**

| Bit | Name | Description |
|-----|------|-------------|
| 0 | V (Valid) | Whether this PTE is valid |
| 1 | R (Read) | Readable |
| 2 | W (Write) | Writable |
| 3 | X (Execute) | Executable |
| 4 | U (User) | Accessible in user mode |
| 5 | G (Global) | Global mapping |
| 6 | A (Accessed) | Has been accessed |
| 7 | D (Dirty) | Has been written to |

**Important rules:**
- If R=0, W=0, X=0 and V=1: This PTE is a **pointer to the next-level page table**
- If any of R, W, X is 1 and V=1: This PTE is a **leaf entry** (points to an actual physical page)

### PTE-related Macros in xv6 (kernel/riscv.h)

```c
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1) // read
#define PTE_W (1L << 2) // write
#define PTE_X (1L << 3) // execute
#define PTE_U (1L << 4) // user can access

// Physical address -> PTE format: remove lower 12 bits, shift left 10 bits
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

// PTE -> Physical address: remove lower 10 bits (flags), shift left 12 bits
#define PTE2PA(pte) (((pte) >> 10) << 12)

// Extract only the flags from a PTE (lower 10 bits)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// Extract the index for each level from a virtual address
#define PXMASK          0x1FF // 9 bits (= 0b111111111)
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)
//   PX(2, va) -> Extract bits 38~30 (L2 index)
//   PX(1, va) -> Extract bits 29~21 (L1 index)
//   PX(0, va) -> Extract bits 20~12 (L0 index)
```

**Understanding the PA2PTE and PTE2PA conversions:**

```
 Physical Address (PA):
  55                          12 11       0
 +------------------------------+---------+
 |      PPN (44 bits)           | offset  |
 +------------------------------+---------+

 PA >> 12:  (remove offset)
  43                             0
 +-------------------------------+
 |      PPN (44 bits)            |
 +-------------------------------+

 (PA >> 12) << 10:  (make room for flags)
  53                    10 9    0
 +------------------------+------+
 |   PPN (44 bits)        | 0000 |
 +------------------------+------+
  = PTE format (flag bits are 0)
```

---

## Exercise 1: Analyzing the xv6 Kernel Memory Map (15 min)

### Goal
Read the `kvmmake()` function in `kernel/vm.c` and analyze how the xv6 kernel maps physical memory into the virtual address space.

### Code Reading

Open the `kvmmake()` function in `kernel/vm.c`:

```c
// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}
```

Signature of `kvmmap()`: `kvmmap(pagetable, va, pa, size, perm)`
- `va`: Virtual address
- `pa`: Physical address
- `size`: Mapping size
- `perm`: Permission flags

Key constants from `kernel/memlayout.h`:

```c
#define UART0     0x10000000L
#define VIRTIO0   0x10001000
#define PLIC      0x0c000000L
#define KERNBASE  0x80000000L
#define PHYSTOP   (KERNBASE + 128*1024*1024)  // = 0x88000000
#define TRAMPOLINE (MAXVA - PGSIZE)           // Top of virtual address space
#define TRAPFRAME  (TRAMPOLINE - PGSIZE)
```

### Question 1-1: Complete the Kernel Memory Map Table

Complete the table below by referencing the `kvmmake()` code.

| Region | Virtual Address (VA) | Physical Address (PA) | Size | Permission | Purpose |
|--------|---------------------|----------------------|------|------------|---------|
| UART | `0x10000000` | `___________` | 4KB | R, W | UART device registers |
| VIRTIO | `0x10001000` | `___________` | 4KB | R, W | Disk I/O interface |
| PLIC | `0x0c000000` | `___________` | `___`MB | R, W | Interrupt controller |
| Kernel text | `0x80000000` | `___________` | Up to etext | `___, ___` | Kernel code |
| Kernel data | `etext` | `___________` | Up to PHYSTOP | `___, ___` | Kernel data+heap |
| TRAMPOLINE | `MAXVA-PGSIZE` | `___________` | 4KB | `___, ___` | Trap entry/return |

### Question 1-2: Direct Mapping

In the `kvmmake()` code, most kernel mappings have `VA == PA` (direct mapping).

1. Which region is **not** directly mapped?

   ```
   Answer: ________________________________________
   ```

2. Why does that region not use direct mapping?

   ```
   Answer: ________________________________________
   ```

### Question 1-3: Permission Analysis

1. Why does the kernel text region have permissions `PTE_R | PTE_X`? Why is `PTE_W` omitted?

   ```
   Answer: ________________________________________
   ```

2. Why does the kernel data region have permissions `PTE_R | PTE_W`? Why is `PTE_X` omitted?

   ```
   Answer: ________________________________________
   ```

3. The kernel regions do not have the `PTE_U` flag. What does this mean?

   ```
   Answer: ________________________________________
   ```

### Kernel Memory Layout Diagram

```
Virtual Address                Physical Address

MAXVA (0x4000000000) ----+
                         |
  TRAMPOLINE  -----------+-------> trampoline code (physical)
  (MAXVA - 4KB)          |         (within kernel code)
                         |
  Kernel Stacks          |         (separately allocated physical pages)
  (with Guard pages)     |
                         |
         ...             |
         (empty space)   |
         ...             |
                         |
  PHYSTOP (0x88000000) --+-------> 0x88000000
         |               |            |
    Kernel data (R/W)    |       Kernel data (physical)
         |               |            |
  etext  ----------------+-------> etext
         |               |            |
    Kernel text (R/X)    |       Kernel text (physical)
         |               |            |
  KERNBASE (0x80000000) -+-------> 0x80000000
         |               |
         ...              |
         |               |
  VIRTIO0 (0x10001000)---+-------> 0x10001000
  UART0 (0x10000000) ----+-------> 0x10000000
         |               |
  PLIC (0x0C000000) -----+-------> 0x0C000000
         |               |
  0x00000000 ------------+
```

---

## Exercise 2: Tracing the walk() Function (10 min)

### Goal
Read the `walk()` function code and manually trace the 3-level page table traversal process for a given virtual address.

### walk() Function Code

```c
// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}
```

### Function Behavior Analysis

1. **Input**: Top-level page table (`pagetable`), virtual address (`va`), allocation flag (`alloc`)
2. **Loop**: `level = 2, 1` (processes the L2 and L1 levels)
   - Extract the current level's index with `PX(level, va)`
   - If the PTE is valid (V=1): extract the physical address of the next-level table from the PTE
   - If the PTE is invalid (V=0):
     - If `alloc=1`: allocate a new page and link it
     - If `alloc=0`: return `NULL`
3. **Return**: Return the address of the PTE at index `PX(0, va)` in the L0 table

### Question 2-1: Manual Address Translation

Use the helper Python script (`examples/lab2_va_translate.py`) or calculate manually to solve the following.

**For virtual address `0x0000000080001234`:**

```
Full VA (binary):  0000...0 10 000000000 000000000 001 001000110100

Extract 39 bits:   1_00000000_000000000_000000001_001000110100
                   ^         ^         ^          ^
```

1. L2 index (VPN[2], bits 38~30) = ?

   ```
   Calculation: (0x80001234 >> 30) & 0x1FF = _______________
   Answer: ___
   ```

2. L1 index (VPN[1], bits 29~21) = ?

   ```
   Calculation: (0x80001234 >> 21) & 0x1FF = _______________
   Answer: ___
   ```

3. L0 index (VPN[0], bits 20~12) = ?

   ```
   Calculation: (0x80001234 >> 12) & 0x1FF = _______________
   Answer: ___
   ```

4. Page Offset (bits 11~0) = ?

   ```
   Calculation: 0x80001234 & 0xFFF = _______________
   Answer: ___
   ```

### Question 2-2: Tracing walk() Execution

Trace the execution of the `walk()` function for the virtual address `0x80001234` above.

```
Step 1 (level=2):
  PX(2, 0x80001234) = ___ (L2 index)
  Read PTE at L2 table[___]
  If PTE is valid -> PTE2PA(PTE) = physical address of L1 table

Step 2 (level=1):
  PX(1, 0x80001234) = ___ (L1 index)
  Read PTE at L1 table[___]
  If PTE is valid -> PTE2PA(PTE) = physical address of L0 table

Step 3 (after loop ends):
  PX(0, 0x80001234) = ___ (L0 index)
  Return address of PTE at L0 table[___]

Final physical address = PTE2PA(L0_PTE) + offset(0x___) = 0x___________
```

### Question 2-3: The Role of the alloc Parameter in walk()

1. What is the difference between `walk(pagetable, va, 0)` and `walk(pagetable, va, 1)`?

   ```
   Answer: ________________________________________
   ```

2. The `mappages()` function calls `walk(pagetable, a, 1)`. Why is `alloc=1`?

   ```
   Answer: ________________________________________
   ```

3. The `walkaddr()` function calls `walk(pagetable, va, 0)`. Why is `alloc=0`?

   ```
   Answer: ________________________________________
   ```

---

## Exercise 3: Implementing vmprint() (15 min)

### Goal
Write a `vmprint()` function that prints the contents of a process's 3-level page table. This function allows you to visually inspect the actual structure of the page table.

### Implementation Guide

The implementation code is in the `examples/lab3_vmprint.c` file. Add this code to `kernel/vm.c`.

**Output format:**
```
page table 0x0000000087f6b000
 ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000
 .. ..0: pte 0x0000000021fd9801 pa 0x0000000087f66000
 .. .. ..0: pte 0x0000000021fda41f pa 0x0000000087f69000
 .. .. ..1: pte 0x0000000021fd9017 pa 0x0000000087f64000
 .. .. ..2: pte 0x0000000021fd8c07 pa 0x0000000087f63000
 ..255: pte 0x0000000021fda801 pa 0x0000000087f6a000
 .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
 .. .. ..510: pte 0x0000000021fdcc17 pa 0x0000000087f73000
 .. .. ..511: pte 0x0000000020001c4b pa 0x0000000080007000
```

- Indentation indicates the level: `..` (L2), `.. ..` (L1), `.. .. ..` (L0)
- For each valid PTE: print the index, raw PTE value, and physical address

### Step 1: Add vmprint() to kernel/vm.c

Add the code from the `examples/lab3_vmprint.c` file to the end of the `kernel/vm.c` file.

```c
// Recursive helper function for printing page tables
void
vmprint_recursive(pagetable_t pagetable, int level)
{
  // Iterate through all 512 entries of the page table
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){   // Only print valid PTEs
      // Indentation: repeat ".." according to the level
      for(int j = 2; j >= level; j--){
        if(j == 2)
          printf(" ..");
        else
          printf(" ..");
      }
      uint64 pa = PTE2PA(pte);
      printf("%d: pte 0x%016lx pa 0x%016lx", i, (uint64)pte, pa);

      // Print flags
      printf(" [");
      if(pte & PTE_R) printf("R");
      if(pte & PTE_W) printf("W");
      if(pte & PTE_X) printf("X");
      if(pte & PTE_U) printf("U");
      printf("]");
      printf("\n");

      // Non-leaf PTE (R=0, W=0, X=0) -> recurse to next level
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        vmprint_recursive((pagetable_t)pa, level - 1);
      }
    }
  }
}

// Print the contents of a process's page table
void
vmprint(pagetable_t pagetable)
{
  printf("page table 0x%016lx\n", (uint64)pagetable);
  vmprint_recursive(pagetable, 2);
}
```

### Step 2: Add Function Declaration to kernel/defs.h

Find the vm.c-related declarations section in `kernel/defs.h` and add the following:

```c
void            vmprint(pagetable_t);
```

### Step 3: Call vmprint() from kernel/exec.c

Add the following just before `return argc;` in the `kexec()` function of `kernel/exec.c`:

```c
  // Debugging: print new process's page table
  if(p->pid == 1){
    printf("== pid 1 (init) page table ==\n");
    vmprint(p->pagetable);
  }
```

The restriction to `pid == 1` is to reduce output by only printing the page table of the first process (init).

### Step 4: Build and Run

```bash
$ make clean
$ make qemu
```

### Question 3-1: Output Analysis

Answer the following questions based on the `vmprint()` output.

1. How many valid PTEs are in the L2 table? What are their indices?

   ```
   Answer: ________________________________________
   ```

2. What virtual address range does the L2 PTE at index 0 cover?
   (Hint: L2 index 0 -> VA range = `0x0` ~ `0x0000003FFFFFFF`, i.e., 1GB)

   ```
   Answer: ________________________________________
   ```

3. What virtual address range does the L2 PTE at index 255 cover?
   (Hint: 255 * 1GB = ?)

   ```
   Answer: ________________________________________
   ```

4. What is the difference between L0-level PTEs with the `[U]` flag and those without it?

   ```
   Answer: ________________________________________
   ```

### Question 3-2: Understanding the Recursive Structure of vmprint()

1. What does the condition `(pte & (PTE_R|PTE_W|PTE_X)) == 0` check in the `vmprint_recursive()` function?

   ```
   Answer: ________________________________________
   ```

2. If this condition is true, a recursive call is made. Why is this the correct behavior?

   ```
   Answer: ________________________________________
   ```

3. The `freewalk()` function has a similar recursive structure. Find it in `kernel/vm.c` and compare. What are the similarities and differences between `vmprint_recursive()` and `freewalk()`?

   ```c
   // freewalk() code reference
   void
   freewalk(pagetable_t pagetable)
   {
     for(int i = 0; i < 512; i++){
       pte_t pte = pagetable[i];
       if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
         uint64 child = PTE2PA(pte);
         freewalk((pagetable_t)child);
         pagetable[i] = 0;
       } else if(pte & PTE_V){
         panic("freewalk: leaf");
       }
     }
     kfree((void*)pagetable);
   }
   ```

   ```
   Similarities: ________________________________________
   Differences: ________________________________________
   ```

---

## Exercise 4: Tracing sbrk() Behavior (10 min)

### Goal
Trace how the page table changes when a process expands its heap memory using the `sbrk()` system call.

### sbrk() Call Path

```
User program: sbrk(n)
    |
    v
sys_sbrk()          [kernel/sysproc.c]
    |
    v
growproc(n)         [kernel/proc.c]
    |
    v
uvmalloc()          [kernel/vm.c]
    |
    v
kalloc() + mappages()
```

### Related Code

**sys_sbrk() - kernel/sysproc.c:**

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
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory
    myproc()->sz += n;
  }
  return addr;
}
```

**growproc() - kernel/proc.c:**

```c
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if(sz + n > TRAPFRAME) {
      return -1;
    }
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
```

**uvmalloc() - kernel/vm.c:**

```c
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();                // 1. Allocate physical page
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);       // 2. Initialize to zero
    if(mappages(pagetable, a, PGSIZE, (uint64)mem,
                PTE_R|PTE_U|xperm) != 0){  // 3. Map in the page table
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}
```

### Question 4-1: Analyzing uvmalloc() Behavior

Assume the process's current size (`p->sz`) is `0x3000` (12KB, 3 pages), and `sbrk(8192)` (8KB, 2 pages) is called.

1. What are the values of `oldsz` and `newsz` in `uvmalloc()`?

   ```
   oldsz = ___________
   newsz = ___________
   ```

2. How many times does the loop execute? What virtual address gets a mapping in each iteration?

   ```
   Iteration 1: va = ___________
   Iteration 2: va = ___________
   ```

3. What are the flags for the newly created PTEs? (`PTE_R|PTE_U|xperm`, where `xperm` is `PTE_W`)

   ```
   Answer: PTE_R | PTE_U | PTE_W | PTE_V = ___________
   (Hint: PTE_V is automatically added by mappages())
   ```

### Question 4-2: Page Table Changes Before and After sbrk()

In the diagram below, draw the page table changes before and after a `sbrk(4096)` call.

```
Before sbrk(4096) (p->sz = 0x3000):

 VA range         Mapping status
 0x0000~0x0FFF  [Page 0] -> PA: 0x87f60000 (code)
 0x1000~0x1FFF  [Page 1] -> PA: 0x87f5f000 (data)
 0x2000~0x2FFF  [Page 2] -> PA: 0x87f5e000 (stack)
 0x3000~0x3FFF  [Unmapped]
 0x4000~0x4FFF  [Unmapped]


After sbrk(4096) (p->sz = 0x4000):

 VA range         Mapping status
 0x0000~0x0FFF  [Page 0] -> PA: 0x87f60000 (code)
 0x1000~0x1FFF  [Page 1] -> PA: 0x87f5f000 (data)
 0x2000~0x2FFF  [Page 2] -> PA: 0x87f5e000 (stack)
 0x3000~0x3FFF  [___________] -> PA: ___________ (new heap)
 0x4000~0x4FFF  [Unmapped]
```

### Question 4-3: Lazy Allocation

This version of xv6 has lazy allocation implemented in `sys_sbrk()`.

1. What is the difference between eager allocation and lazy allocation?

   ```
   Eager: ________________________________________
   Lazy:  ________________________________________
   ```

2. With lazy allocation, is a physical page immediately allocated when `sbrk(4096)` is called?

   ```
   Answer: ________________________________________
   ```

3. When is the actual physical page allocated? (Refer to the `vmfault()` function in `kernel/vm.c`)

   ```
   Answer: ________________________________________
   ```

### Question 4-4: User Process Memory Layout

Draw the virtual address space layout of an xv6 user process.

```
  MAXVA (0x4000000000)
    +------------------+
    |   TRAMPOLINE     | <- Same physical page as kernel
    +------------------+
    |   TRAPFRAME      | <- Per-process trap frame
    +------------------+
    |                  |
    |   (empty space)  |
    |                  |
    +------------------+
    |                  |
    |    Heap          | <- Expanded by sbrk()
    |    ^^^^^^^^^^^   |
    +------------------+ <- p->sz
    |  Guard Page      | <- No PTE_U (inaccessible)
    +------------------+
    |  User Stack      | <- Stack
    +------------------+
    |  Data + BSS      | <- Global variables, initialized data
    +------------------+
    |  Text (Code)     | <- Program code
    +------------------+
    0x0000000000
```

(Note: In xv6, the stack is allocated just above text/data in the `kexec()` function of `exec.c`. The heap grows upward from above the stack via `sbrk()`.)

---

## Summary and Key Takeaways

### Core Concepts

1. **Sv39 3-level page table**: RISC-V splits a 39-bit virtual address into 9+9+9+12 and translates it through a 3-level page table.

2. **PTE structure**: Each PTE is 64 bits, containing a physical page number (PPN) and flag bits (V, R, W, X, U, etc.).

3. **Kernel direct mapping**: The xv6 kernel directly maps most physical memory with VA=PA, allowing easy access to physical addresses.

4. **walk() function**: Traverses the page table in the order L2->L1->L0 from a virtual address to find the final PTE. When `alloc=1`, it allocates new intermediate tables if they don't exist.

5. **uvmalloc()**: When expanding process memory, allocates physical pages with `kalloc()` and adds mappings to the page table with `mappages()`.

6. **Lazy allocation**: When `sbrk()` is called, only `p->sz` is increased and the physical page is allocated at the actual access point (page fault) in `vmfault()`.

### Key Function Summary

| Function | File | Role |
|----------|------|------|
| `kvmmake()` | vm.c | Create kernel page table (direct mapping) |
| `kvmmap()` | vm.c | Add mapping to kernel page table |
| `walk()` | vm.c | Find and return PTE for a VA through 3 levels |
| `walkaddr()` | vm.c | VA -> PA translation (for user pages) |
| `mappages()` | vm.c | Map a VA range to PA (set PTEs) |
| `uvmcreate()` | vm.c | Create an empty user page table |
| `uvmalloc()` | vm.c | Expand user memory (allocate physical pages + map) |
| `uvmdealloc()` | vm.c | Shrink user memory (unmap + free physical pages) |
| `growproc()` | proc.c | Change process size (sbrk backend) |
| `vmfault()` | vm.c | Page fault handler (lazy allocation) |

### Key Macro Summary

| Macro | Operation |
|-------|-----------|
| `PX(level, va)` | Extract the 9-bit index for the given level from VA |
| `PA2PTE(pa)` | Convert physical address -> PTE format |
| `PTE2PA(pte)` | Convert PTE -> physical address |
| `PTE_FLAGS(pte)` | Extract flag bits (lower 10 bits) from PTE |
| `PGROUNDUP(sz)` | Round up to page size boundary |
| `PGROUNDDOWN(a)` | Round down to page size boundary |

---

## References

- xv6 book Chapter 3: Page Tables
- RISC-V Privileged Architecture Specification (Sv39 section)
- `kernel/vm.c` - Core virtual memory code
- `kernel/riscv.h` - RISC-V related macro definitions
- `kernel/memlayout.h` - Memory layout constant definitions
- `kernel/exec.c` - exec system call implementation
- `kernel/proc.c` - Process management (growproc, etc.)
