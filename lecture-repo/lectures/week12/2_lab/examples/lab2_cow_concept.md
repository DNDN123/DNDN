# Lab 2: COW Fork Concept Demonstration

## Description

This program demonstrates **Copy-On-Write (COW)** behavior using `fork()` on Linux/macOS. It runs locally (not in xv6) and uses `mmap(MAP_PRIVATE | MAP_ANONYMOUS)` to allocate memory, then observes how the OS handles page sharing and copying after `fork()`.

The program consists of three demonstrations:

1. **COW Basic Behavior** -- Allocates 4 MB (1024 pages), forks, and compares minor page fault counts when the child performs read-only access vs. write access. Read-only access triggers no COW copies, while writing to each page triggers a minor page fault per page as the OS copies pages on demand.
2. **COW Performance** -- Measures `fork()` latency to show that COW avoids copying all pages upfront, making fork fast even with large memory regions.
3. **COW Process Isolation** -- Confirms that after fork, a write by the child does not affect the parent's data, demonstrating that COW preserves process isolation.

## Build & Run

```bash
gcc -Wall -o lab2_cow_concept lab2_cow_concept.c
./lab2_cow_concept
```

No special libraries or flags are required. Works on both Linux and macOS.

## What to Observe

- **After fork, reading does not cause additional minor page faults.** The child shares the parent's physical pages in read-only mode.
- **Writing triggers ~1024 minor page faults** (one per page), confirming that the OS copies each page only when it is written to.
- **The parent's data remains unchanged** after the child modifies its copy, proving process isolation through COW.
- **Fork latency is very low** despite the 4 MB allocation, because no data is actually copied at fork time.

## Discussion Questions

1. Why does the OS set shared pages to read-only after `fork()`, even though the parent originally had write permission?
2. If a child calls `exec()` immediately after `fork()`, how many COW page copies actually occur? Why does this make the fork-exec pattern efficient?
3. In the isolation demo, the memory is mapped with `MAP_PRIVATE`. What would happen if `MAP_SHARED` were used instead? Would COW still apply?
4. How does this user-space observation relate to the xv6 COW implementation discussed in the lab (PTE_COW bit, reference counting, `cowfault()`)?
5. On a system with huge pages (e.g., 2 MB pages), how would COW behavior change in terms of copy granularity and fault counts?
