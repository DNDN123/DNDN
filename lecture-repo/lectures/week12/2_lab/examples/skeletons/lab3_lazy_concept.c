/*
 * lazy_concept.c - A program to observe Lazy Allocation (Demand Paging) behavior
 *
 * [Overview]
 * After allocating a large amount of memory with mmap(MAP_ANONYMOUS),
 * we observe the process of physical memory being allocated each time a page is accessed.
 *
 * [Core Concepts of Lazy Allocation / Demand Paging]
 *
 * 1) Lazy Allocation:
 *    - When a process requests memory (sbrk/mmap), only the virtual address space is expanded
 *      and physical pages are NOT immediately allocated
 *    - Only the virtual address range is recorded; PTEs are not created (or Valid bit is not set)
 *    - When the process actually accesses the address and a page fault occurs, only then
 *      is a physical page allocated and the PTE set up
 *
 * 2) Lazy Allocation implementation locations in xv6:
 *    - sys_sbrk() in kernel/sysproc.c: only increase the size without allocating physical pages
 *      (original: calls growproc() -> modified: only increase p->sz)
 *    - usertrap() in kernel/trap.c: handle page faults where scause == 13 (load) or 15 (store)
 *      a) Check if the page fault address (stval) is within a valid range (0 ~ p->sz)
 *      b) Allocate a new physical page with kalloc()
 *      c) Zero-initialize with memset(mem, 0, PGSIZE) for security
 *      d) Create a PTE with mappages() (VA -> PA mapping)
 *    - uvmunmap() in kernel/vm.c: when unmapping pages that were not mapped due to lazy allocation,
 *      needs to be modified so it does not panic when PTEs are missing
 *
 * 3) Advantages of Lazy Allocation:
 *    - Only pages actually used consume physical pages (memory efficiency)
 *    - sbrk/mmap calls complete immediately (no delay)
 *    - Effective for patterns where a process requests large memory but only uses a portion
 *
 * Compile: gcc -Wall -o lab3_lazy_concept lab3_lazy_concept.c
 * Run:     ./lab3_lazy_concept
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>

/* Memory size settings for the experiment */
#define PAGE_SIZE     4096                                /* Page size (4KB) */
#define TOTAL_PAGES   8192                                /* Number of pages to allocate (8192 pages) */
#define REGION_SIZE   ((size_t)TOTAL_PAGES * PAGE_SIZE)   /* Total 32MB (8192 * 4KB) */
#define TOUCH_STEP    512                                 /* Measurement interval: print stats every 512 pages */

/*
 * get_rss_kb - Returns the current process's RSS (Resident Set Size) in KB.
 *
 * RSS: Total size of pages that actually reside in physical memory.
 * In lazy allocation, only accessed pages are included in RSS.
 *
 * Linux: Read from /proc/self/statm.
 * macOS: Use ru_maxrss from getrusage() (maximum RSS).
 */
static long get_rss_kb(void)
{
#ifdef __linux__
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        long total, rss;
        if (fscanf(f, "%ld %ld", &total, &rss) == 2) {
            fclose(f);
            return rss * (sysconf(_SC_PAGESIZE) / 1024);
        }
        fclose(f);
    }
    return -1;
#else
    /* macOS etc.: use getrusage() */
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    /* On macOS, ru_maxrss is in bytes */
    return usage.ru_maxrss / 1024;
#endif
}

/*
 * get_minor_faults - Returns the number of minor page faults for the current process.
 *
 * Minor page fault: A page fault handled without disk I/O.
 * New page allocations due to lazy allocation are recorded as minor page faults.
 */
static long get_minor_faults(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_minflt;
}

/*
 * demo_lazy_basic - Demonstrates the basic behavior of Lazy Allocation.
 *
 * Allocates a large memory region with mmap and accesses it incrementally,
 * observing how the minor page fault count changes.
 */
static void demo_lazy_basic(void)
{
    char *region;
    int i;

    printf("=== 1. Lazy Allocation Basic Behavior ===\n\n");

    long faults_before = get_minor_faults();
    long rss_before = get_rss_kb();

    /* TODO: Allocate a large region with mmap.
     *
     * Use mmap with the following parameters:
     *   - addr: NULL (let OS choose the address)
     *   - length: REGION_SIZE
     *   - prot: PROT_READ | PROT_WRITE
     *   - flags: MAP_PRIVATE | MAP_ANONYMOUS
     *   - fd: -1 (not file-backed)
     *   - offset: 0
     *
     * At this point, NO physical memory should be used.
     * The kernel only records the virtual address space range (VMA).
     * This is the core of "lazy allocation": separating the allocation
     * request from actual physical page allocation.
     *
     * Check for MAP_FAILED and exit on error.
     */
    region = NULL;  /* Replace with mmap call */

    long faults_after_mmap = get_minor_faults();
    long rss_after_mmap = get_rss_kb();

    printf("[Before mmap call]\n");
    printf("  minor page faults: %ld\n", faults_before);
    printf("  RSS: %ld KB\n\n", rss_before);

    printf("[After mmap(%zu KB) call - not yet accessed]\n",
           REGION_SIZE / 1024);
    printf("  minor page faults: %ld (added: %ld)\n",
           faults_after_mmap, faults_after_mmap - faults_before);
    printf("  RSS: %ld KB\n", rss_after_mmap);
    printf("  -> Only virtual address space allocated. Almost no physical memory used!\n\n");

    /* TODO: Access pages incrementally and observe fault count changes.
     *
     * Print a table header showing "Pages accessed", "Cumulative faults", "Interval faults".
     *
     * Then loop through TOTAL_PAGES pages:
     *   1. Write to the first byte of each page: region[(size_t)i * PAGE_SIZE] = (char)(i & 0xFF)
     *      - This triggers a page fault, causing physical page allocation
     *   2. Every TOUCH_STEP pages (when (i+1) % TOUCH_STEP == 0):
     *      - Get current minor faults
     *      - Print: pages accessed so far, cumulative faults since mmap, interval faults
     *      - Update prev_faults for next interval
     */

    long rss_after_touch = get_rss_kb();
    long faults_after_touch = get_minor_faults();

    printf("\n[All pages accessed]\n");
    printf("  Total minor page faults: %ld\n",
           faults_after_touch - faults_after_mmap);
    printf("  RSS: %ld KB\n", rss_after_touch);
    long sys_pgsize = sysconf(_SC_PAGESIZE);
    long expected = (long)TOTAL_PAGES * PAGE_SIZE / sys_pgsize;
    printf("  -> Expected approximately %ld page faults based on system page size (%ld bytes)\n",
           expected, sys_pgsize);
    printf("     (Program PAGE_SIZE=%d, actual system page=%ld bytes)\n\n",
           PAGE_SIZE, sys_pgsize);

    if (region)
        munmap(region, REGION_SIZE);
}

/*
 * demo_lazy_sparse - Demonstrates the benefits of sparse access patterns.
 *
 * Allocates a large memory region and accesses only some pages,
 * observing that only the accessed pages consume physical memory.
 *
 * This is the greatest advantage of lazy allocation:
 * - If a program requests 32MB but actually uses only 10%,
 *   only about 3.2MB of physical memory is consumed
 * - With eager allocation, the entire 32MB would have been allocated immediately
 * - Very effective for hash tables, sparse arrays, etc.
 */
static void demo_lazy_sparse(void)
{
    char *region;
    int i;
    int accessed = 0;

    printf("=== 2. Lazy Allocation Benefits with Sparse Access Patterns ===\n\n");

    /* TODO: Allocate a large region with mmap (same parameters as demo_lazy_basic).
     * Check for MAP_FAILED and exit on error.
     */
    region = NULL;  /* Replace with mmap call */

    long faults_before = get_minor_faults();

    /* TODO: Access only 10% of all pages (every 10th page).
     *
     * Loop from i=0 to TOTAL_PAGES with step 10:
     *   - Write 'S' to region[(size_t)i * PAGE_SIZE]
     *   - Increment the 'accessed' counter
     */

    long faults_after = get_minor_faults();
    long rss = get_rss_kb();

    printf("  Allocated virtual memory: %zu KB (%d pages)\n",
           REGION_SIZE / 1024, TOTAL_PAGES);
    printf("  Actually accessed pages: %d pages (about %d%% of total)\n",
           accessed, (accessed * 100) / TOTAL_PAGES);
    printf("  minor page faults: %ld\n", faults_after - faults_before);
    printf("  RSS: %ld KB\n", rss);
    printf("  -> The %d%% of pages not accessed do not use physical memory!\n",
           100 - (accessed * 100) / TOTAL_PAGES);
    printf("  -> With eager allocation, all %zu KB would have been allocated\n\n",
           REGION_SIZE / 1024);

    if (region)
        munmap(region, REGION_SIZE);
}

/*
 * demo_lazy_timing - Compares allocation time vs access time.
 *
 * Compares three timings to understand the cost structure of lazy allocation:
 *   1. mmap call time: Very fast (us scale) since it only sets up virtual address space
 *   2. First access time: Each page triggers page fault -> physical allocation -> PTE creation
 *      Page fault handling overhead occurs during this process
 *   3. Re-access time: PTEs are already set up, so access is direct without page faults
 *      Even faster if cached in hardware TLB
 */
static void demo_lazy_timing(void)
{
    char *region;
    int i;
    struct timespec t1, t2;

    printf("=== 3. Allocation Time vs Access Time Comparison ===\n\n");

    /* TODO: Measure mmap allocation time.
     *
     * 1. Record time before mmap (clock_gettime(CLOCK_MONOTONIC, &t1))
     * 2. Call mmap to allocate REGION_SIZE (same parameters as before)
     * 3. Record time after mmap (clock_gettime(CLOCK_MONOTONIC, &t2))
     * 4. Check for MAP_FAILED
     * 5. Calculate elapsed time in microseconds:
     *    mmap_us = (t2.tv_sec - t1.tv_sec) * 1000000LL + (t2.tv_nsec - t1.tv_nsec) / 1000
     * 6. Print mmap time and explain it's fast because only virtual space is set up
     */
    region = NULL;  /* Replace with mmap call */

    /* TODO: Measure time to access all pages (first access).
     *
     * 1. Record time before access loop
     * 2. Loop through TOTAL_PAGES, writing to region[(size_t)i * PAGE_SIZE] = (char)i
     *    - Each page triggers a page fault -> physical allocation
     * 3. Record time after access loop
     * 4. Calculate and print elapsed time
     *    - Explain that page faults occur on each access
     */

    /* TODO: Measure time to re-access already allocated pages.
     *
     * 1. Record time before re-access loop
     * 2. Loop through TOTAL_PAGES, writing to region[(size_t)i * PAGE_SIZE] = (char)(i+1)
     *    - No page faults this time (PTEs already set up)
     * 3. Record time after re-access loop
     * 4. Calculate and print elapsed time
     *    - Explain it's faster because no page faults occur
     * 5. Print the ratio of first access time to re-access time
     */

    /* Suppress unused variable warnings - remove when implementing */
    (void)region;
    (void)i;
    (void)t1;
    (void)t2;

    if (region)
        munmap(region, REGION_SIZE);
}

/*
 * demo_lazy_zero_fill - Verifies that newly allocated pages are zero-initialized.
 *
 * For security reasons, the OS must zero-initialize newly allocated physical pages.
 * Otherwise, a security vulnerability arises where memory contents from previous
 * processes (passwords, personal data, etc.) could be exposed to new processes.
 *
 * xv6 implementation: calls memset(mem, 0, PGSIZE) after kalloc() in the page fault handler
 * Linux implementation: uses a system-wide "zero page" or zero-initializes upon allocation
 */
static void demo_lazy_zero_fill(void)
{
    char *region;
    int i;
    int non_zero = 0;

    printf("=== 4. Verify Zero-Initialization of New Pages ===\n\n");

    /* TODO: Allocate a large region with mmap (same parameters as before).
     * Check for MAP_FAILED and exit on error.
     */
    region = NULL;  /* Replace with mmap call */

    /* TODO: Read all pages WITHOUT prior writes to verify zero-initialization.
     *
     * For each of TOTAL_PAGES pages:
     *   - Check every byte in the page (j = 0 to PAGE_SIZE-1)
     *   - If any byte at region[(size_t)i * PAGE_SIZE + j] is non-zero,
     *     increment non_zero counter and break out of the inner loop
     */

    printf("  Pages with non-zero data out of %d pages: %d\n",
           TOTAL_PAGES, non_zero);
    if (non_zero == 0) {
        printf("  -> All new pages are zero-initialized (security guaranteed)\n");
    } else {
        printf("  -> Warning: some pages are not zero-initialized\n");
    }
    printf("  -> xv6's vmfault() also initializes with memset(mem, 0, PGSIZE)\n\n");

    if (region)
        munmap(region, REGION_SIZE);
}

int main(void)
{
    printf("=============================================\n");
    printf("  Lazy Allocation Behavior Demonstration\n");
    printf("=============================================\n\n");
    printf("System page size: %d bytes\n", (int)sysconf(_SC_PAGESIZE));
    printf("Allocation region size: %d pages (%zu KB)\n\n",
           TOTAL_PAGES, REGION_SIZE / 1024);
#ifdef __linux__
    printf("Platform: Linux (RSS measured via /proc/self/statm)\n\n");
#elif defined(__APPLE__)
    printf("Platform: macOS (memory usage measured via getrusage())\n");
    printf("Note: macOS ru_maxrss is maximum RSS, so decreases cannot be observed.\n");
    printf("      Verify lazy allocation behavior via minor page fault counts.\n\n");
#else
    printf("Platform: Other (measured via getrusage())\n\n");
#endif

    demo_lazy_basic();
    demo_lazy_sparse();
    demo_lazy_timing();
    demo_lazy_zero_fill();

    printf("=============================================\n");
    printf("  Summary\n");
    printf("=============================================\n");
    printf("1. mmap() only allocates virtual address space; physical memory is allocated upon access\n");
    printf("2. Pages not accessed do not consume physical memory\n");
    printf("3. A page fault occurs on first access, and a physical page is allocated\n");
    printf("4. Newly allocated pages are zero-initialized for security\n");
    printf("5. xv6's vmfault() follows the same principle: kalloc() + memset(0) + mappages()\n");

    return 0;
}
