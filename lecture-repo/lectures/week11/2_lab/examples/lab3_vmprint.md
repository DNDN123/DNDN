# Lab 3: Implementing vmprint() for xv6 (vmprint.c)

## Description

`lab3_vmprint.c` contains the implementation of a `vmprint()` function that recursively walks a process's 3-level Sv39 page table and prints every valid PTE. This is **not** a standalone program -- it is code that must be integrated into the xv6 kernel source tree.

The function prints each valid page table entry with its index, raw PTE value, physical address, and decoded permission flags (`R`, `W`, `X`, `U`). Indentation indicates the page table level: one `..` for L2, two for L1, three for L0.

This exercise corresponds to Exercise 3 of the lab worksheet.

## How to Use

Since this is xv6 kernel code, you integrate it into the xv6 source and run it inside QEMU. Follow these four steps:

### Step 1: Add the functions to kernel/vm.c

Copy the two functions (`vmprint_recursive` and `vmprint`) from `lab3_vmprint.c` and paste them at the end of `kernel/vm.c`.

### Step 2: Declare vmprint() in kernel/defs.h

Find the `// vm.c` section in `kernel/defs.h` and add:

```c
void            vmprint(pagetable_t);
```

### Step 3: Call vmprint() from kernel/exec.c

In the `kexec()` function of `kernel/exec.c`, add the following just before `return argc;`:

```c
if(p->pid == 1){
  printf("== pid 1 (init) page table ==\n");
  vmprint(p->pagetable);
}
```

The `pid == 1` check limits the output to the init process so the boot log stays readable.

### Step 4: Build and run

```bash
make clean
make qemu
```

The page table dump will appear in the console output during boot, before the shell prompt.

## What to Observe

1. **Number of valid L2 entries** -- The init process should have only two valid L2 PTEs (index 0 for user code/data, index 255 for trampoline/trapframe). Most of the 512-entry L2 table is empty.
2. **Permission flags on leaf PTEs** --
   - `[RWXU]`: user code page (readable, writable, executable, user-accessible)
   - `[RWU]`: user data or stack page
   - `[RW]` (no U): guard page -- the kernel cleared `PTE_U` via `uvmclear()` so user code cannot access it
   - `[RX]` (no U): trampoline page -- kernel-only executable code
   - `[--]`: non-leaf PTE pointing to the next-level table (R=W=X=0)
3. **Physical addresses** -- Each PTE's PA shows which physical frame was allocated. These addresses will differ between runs.
4. **Recursive structure** -- The indentation visually mirrors the 3-level tree: L2 entries point to L1 tables, which point to L0 tables, which point to physical pages.

## Discussion Questions

1. The init process has a guard page at L0 index 2 with flags `[RW]` (no `PTE_U`). What happens if user code tries to access this page? Why is this useful?
2. The `vmprint_recursive()` function checks `(pte & (PTE_R|PTE_W|PTE_X)) == 0` to decide whether to recurse. Why does this condition correctly distinguish non-leaf PTEs from leaf PTEs?
3. Compare `vmprint_recursive()` with `freewalk()` in `kernel/vm.c`. Both traverse the page table recursively. What does `freewalk()` do differently, and why does it panic on leaf PTEs?
4. If you remove the `pid == 1` check so that `vmprint()` runs for every `exec()`, what would you expect to see? Would the output be useful or overwhelming?
5. How would you modify `vmprint()` to also print the virtual address that each L0 PTE maps? (Hint: you would need to track the accumulated VPN indices across levels.)
