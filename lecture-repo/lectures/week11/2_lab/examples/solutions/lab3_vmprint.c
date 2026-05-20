/*
 * vmprint() - Function to print page table contents of a process
 *
 * [Overview]
 * Recursively traverses the xv6 Sv39 3-level page table and
 * prints all valid PTEs (those with PTE_V set).
 * Allows quick inspection of the page table structure and permission flags of each mapping.
 *
 * [Sv39 3-Level Page Table Structure]
 *
 *   satp register ----> L2 table (512 PTEs, 8 bytes each = 4KB)
 *                         |
 *                    Select PTE using VPN[2] index
 *                         |
 *                         v
 *                       L1 table (512 PTEs)
 *                         |
 *                    Select PTE using VPN[1] index
 *                         |
 *                         v
 *                       L0 table (512 PTEs)
 *                         |
 *                    Select PTE using VPN[0] index
 *                         |
 *                         v
 *                    Physical page (4KB)
 *
 * [PTE Flag Bits (kernel/riscv.h)]
 *   PTE_V (bit 0): Valid - whether this PTE is valid
 *   PTE_R (bit 1): Read  - read permission
 *   PTE_W (bit 2): Write - write permission
 *   PTE_X (bit 3): eXecute - execute permission
 *   PTE_U (bit 4): User - accessible from user mode
 *
 *   Leaf PTE: at least one of R, W, X is set -> points to an actual physical page
 *   Non-leaf PTE: R=0, W=0, X=0 and V=1 -> points to the next level page table
 *
 * [xv6 Key Macros (kernel/riscv.h)]
 *   PTE2PA(pte): Extract physical address (PA) from PTE
 *     -> ((pte) >> 10) << 12   (convert PPN field to page-aligned address)
 *   PA2PTE(pa):  Convert physical address to PTE's PPN field
 *     -> ((pa) >> 12) << 10
 *
 * Add this code to the end of the kernel/vm.c file.
 *
 * === How to Add ===
 *
 * Step 1: Copy the two functions from this file to the end of kernel/vm.c.
 *
 * Step 2: Find the "// vm.c" section in kernel/defs.h and add the following declaration:
 *
 *     void            vmprint(pagetable_t);
 *
 * Step 3: In the kexec() function in kernel/exec.c, add the following code just before "return argc;":
 *
 *     if(p->pid == 1){
 *       printf("== pid 1 (init) page table ==\n");
 *       vmprint(p->pagetable);
 *     }
 *
 * Step 4: Build and run
 *     $ make clean
 *     $ make qemu
 *
 * === Expected Output ===
 *
 * == pid 1 (init) page table ==
 * page table 0x0000000087f6b000
 *  ..0: pte 0x0000000021fd9c01 pa 0x0000000087f67000 [--]
 *  .. ..0: pte 0x0000000021fd9801 pa 0x0000000087f66000 [--]
 *  .. .. ..0: pte 0x0000000021fda41f pa 0x0000000087f69000 [RWXU]
 *  .. .. ..1: pte 0x0000000021fd9017 pa 0x0000000087f64000 [RWU]
 *  .. .. ..2: pte 0x0000000021fd8c07 pa 0x0000000087f63000 [RW]
 *  .. .. ..3: pte 0x0000000021fd8817 pa 0x0000000087f62000 [RWU]
 *  ..255: pte 0x0000000021fda801 pa 0x0000000087f6a000 [--]
 *  .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000 [--]
 *  .. .. ..510: pte 0x0000000021fdcc17 pa 0x0000000087f73000 [RWU]
 *  .. .. ..511: pte 0x0000000020001c4b pa 0x0000000080007000 [RX]
 *
 * === Output Interpretation ===
 *
 * 1) L2 index 0 -> virtual address range 0x0000000000 ~ 0x003FFFFFFF (user code/data region)
 *    - L1 index 0 -> links to L0 table
 *      - L0 index 0: user code page (RWXU) -- text segment (read+write+execute+user)
 *      - L0 index 1: user data page (RWU) -- data segment (read+write+user)
 *      - L0 index 2: guard page (RW, no U) -- PTE_U removed by uvmclear() in exec
 *                     accessing from user causes page fault -> stack overflow detection
 *      - L0 index 3: user stack page (RWU) -- stack segment
 *
 * 2) L2 index 255 -> top of virtual address space (TRAMPOLINE/TRAPFRAME)
 *    - L1 index 511 -> links to L0 table
 *      - L0 index 510: TRAPFRAME page (RWU) -- saves/restores registers during trap
 *      - L0 index 511: TRAMPOLINE page (RX, no U) -- uservec/userret code
 *                       mapped at the same VA in both kernel and user page tables,
 *                       so code runs without interruption during page table switch
 *
 * Note: actual output may show different physical addresses depending on the execution environment.
 *       PTE values and flag structures remain the same.
 */

// ====================================================================
// Add the code below to the end of kernel/vm.c
// ====================================================================

/*
 * vmprint_recursive - Recursively print valid entries of a page table
 *
 * Parameters:
 *   pagetable: pointer to the current level's page table (pagetable_t = uint64*)
 *              in xv6, each page table is a 4KB page containing 512 PTEs
 *   level: current level (2 = L2 root, 1 = L1 middle, 0 = L0 leaf)
 *
 * Behavior:
 *   1. Iterate through 512 PTEs and print only valid (PTE_V) entries
 *   2. When a non-leaf PTE (valid PTE with R=0, W=0, X=0) is found,
 *      recursively call into the next level table pointed to by that PTE
 *   3. Leaf PTEs (at least one of R, W, X set) represent actual mappings, so print their flags
 *
 * This function has a recursive structure similar to xv6's freewalk() function.
 * freewalk() frees the page table, while vmprint_recursive() prints it.
 */
void
vmprint_recursive(pagetable_t pagetable, int level)
{
  // A page table consists of 512 PTEs (index 0~511)
  // Each PTE is 8 bytes (uint64) -> 512 * 8 = 4096 = one page
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];   // Read the i-th PTE (pte_t = uint64)
    if(!(pte & PTE_V))
      continue;           // If PTE_V (bit 0) is 0, the entry is invalid -> skip

    // Indentation output: deeper levels get deeper indentation
    // level 2 (L2 root):   " .."
    // level 1 (L1 middle): " .. .."
    // level 0 (L0 leaf):   " .. .. .."
    // This visually represents the 3-level hierarchy
    for(int j = 2; j >= level; j--){
      printf(" ..");
    }

    // PTE2PA macro: extract physical address from PTE
    // Bits 53~10 of PTE are the PPN (Physical Page Number)
    // PTE2PA(pte) = ((pte >> 10) << 12) -> page-aligned physical address
    uint64 pa = PTE2PA(pte);
    printf("%d: pte 0x%016lx pa 0x%016lx", i, (uint64)pte, pa);

    // Print PTE flag bits in human-readable form
    // Show R(Read), W(Write), X(eXecute), U(User) flags
    // Non-leaf PTEs have R=W=X=0, displayed as "--"
    printf(" [");
    if(pte & PTE_R) printf("R");   // Bit 1: read permission
    if(pte & PTE_W) printf("W");   // Bit 2: write permission
    if(pte & PTE_X) printf("X");   // Bit 3: execute permission
    if(pte & PTE_U) printf("U");   // Bit 4: user mode access permission
    if((pte & (PTE_R|PTE_W|PTE_X)) == 0) printf("--");  // Non-leaf PTE
    printf("]");
    printf("\n");

    // If non-leaf PTE, recurse into the next level page table
    // Sv39 spec: a PTE with R=0, W=0, X=0 and V=1 points to the next level table
    // Leaf PTE: at least one of R, W, X set -> actual physical page mapping
    // Non-leaf PTE: R=0, W=0, X=0 -> PPN is the physical address of the next level table
    if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // Cast pa to pagetable_t to use as the next level table pointer
      // xv6 kernel uses direct mapping (VA==PA), so PA can be used directly as a pointer
      vmprint_recursive((pagetable_t)pa, level - 1);
    }
  }
}

/*
 * vmprint - Print the entire page table of a process
 *
 * Parameters:
 *   pagetable: pointer to the L2 (top-level, root) page table
 *              in xv6, a process's page table is stored in proc->pagetable
 *              this value is loaded into the satp register for hardware address translation
 *
 * Usage examples:
 *   vmprint(p->pagetable);     // Print process p's user page table
 *   vmprint(kernel_pagetable); // Print the kernel page table
 *
 * Output format: first prints the root table's physical address, then calls
 *                vmprint_recursive() to recursively print all valid PTEs
 */
void
vmprint(pagetable_t pagetable)
{
  printf("page table 0x%016lx\n", (uint64)pagetable);
  vmprint_recursive(pagetable, 2);
}
