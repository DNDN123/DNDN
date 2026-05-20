# Lab 3: Lazy Allocation Concept Demonstration

## Description

This program demonstrates **lazy allocation** behavior on Linux/macOS. It runs locally (not in xv6) and uses `mmap(MAP_PRIVATE | MAP_ANONYMOUS)` to request a large memory region (32 MB / 8192 pages), then observes that physical memory is only allocated when pages are actually accessed.

The program consists of four demonstrations:

1. **Lazy Allocation Basic Behavior** -- Allocates 32 MB via `mmap` and shows that no physical memory is consumed until the pages are touched. As pages are accessed in increments of 512, the program prints a table showing the cumulative and per-interval minor page fault counts, confirming that each first access triggers a page fault.
2. **Sparse Access Pattern** -- Accesses only 10% of pages (every 10th page) and shows that the remaining 90% consume no physical memory, highlighting the efficiency of lazy allocation for sparse data structures.
3. **Allocation Time vs. Access Time** -- Measures that `mmap()` itself completes in microseconds (only sets up virtual address space), while touching all pages takes significantly longer due to page fault handling. A second pass over already-allocated pages is much faster, confirming the one-time cost of demand paging.
4. **Zero-Fill on Demand** -- Verifies that all newly allocated pages are initialized to zero, which is a security requirement to prevent leaking data from previously freed pages.

## Build & Run

```bash
gcc -Wall -o lab3_lazy_concept lab3_lazy_concept.c
./lab3_lazy_concept
```

No special libraries or flags are required. Works on both Linux and macOS.

**Platform note:** On Linux, RSS is measured via `/proc/self/statm` (current RSS). On macOS, `getrusage()` reports `ru_maxrss` (peak RSS in bytes), so you may not observe RSS decreasing. Focus on the minor page fault counts on macOS.

## What to Observe

- **After `mmap()`, RSS barely increases** -- the 32 MB exists only in virtual address space.
- **Minor page faults increase linearly** as pages are touched, with roughly one fault per system page accessed.
- **Sparse access (10% of pages) produces ~10% of the faults** compared to touching all pages, confirming that unaccessed pages consume no physical memory.
- **First touch is significantly slower than re-access** -- the ratio demonstrates the overhead of page fault handling (trap entry, `kalloc()`, page table update, TLB flush).
- **All new pages read as zero** -- the OS zero-fills pages on allocation for security.

## Discussion Questions

1. Why does `mmap()` return almost instantly even for a 32 MB request? What kernel data structure is modified at `mmap()` time vs. at page fault time?
2. In the sparse access demo, what would happen if eager allocation were used instead? How much memory would be wasted?
3. The timing demo shows that first access is slower than re-access. What specific operations does the kernel perform during a minor page fault that account for this overhead?
4. Why is zero-filling new pages a security requirement? What could go wrong if the OS skipped this step?
5. How does this user-space behavior relate to xv6's `vmfault()` implementation? Compare the steps: `kalloc()` + `memset(0)` + `mappages()` in xv6 with what the Linux/macOS kernel does behind the scenes.
