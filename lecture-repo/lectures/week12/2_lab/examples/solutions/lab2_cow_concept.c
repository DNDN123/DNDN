/*
 * cow_concept.c - Program to observe Copy-On-Write (COW) behavior
 *
 * [Overview]
 * Demonstrates the core concepts of COW fork on Linux/macOS.
 * - Parent creates a memory region with mmap and fills it with data
 * - After fork(), parent and child share the same physical pages
 * - When one side writes, COW causes the page to be copied
 *
 * [Core Concepts of COW Fork]
 *
 * 1) Reference Counting:
 *    - Maintain a reference count for each physical page
 *    - On fork(), child copies parent's PTEs and increments the physical page's reference count
 *    - On page free, decrement the reference count; only actually free physical memory when it reaches 0
 *    - In xv6 COW implementation, managed with a global array: refcnt[pa / PGSIZE]
 *
 * 2) Page Fault Handling:
 *    - On fork(), remove PTE_W (write permission) from both parent's and child's PTEs
 *    - Instead, set the PTE_COW flag (e.g., RSW bit) to mark it as a COW page
 *    - When either side attempts to write to the page, a page fault occurs
 *    - In the page fault handler:
 *      a) If reference count is 1: only this process is using it, so just restore PTE_W
 *      b) If reference count is 2 or more: allocate a new physical page, copy original contents,
 *         update PTE to point to the new page (set PTE_W, clear PTE_COW)
 *      c) Decrement the original page's reference count
 *
 * 3) xv6 COW Implementation Locations:
 *    - kernel/vm.c uvmcopy(): copy PTEs on fork + remove PTE_W + set PTE_COW
 *    - kernel/trap.c usertrap(): handle scause == 15 (store page fault)
 *    - kernel/vm.c or kernel/kalloc.c: reference count management functions
 *
 * Compile: gcc -Wall -o lab2_cow_concept lab2_cow_concept.c
 * Run:     ./lab2_cow_concept
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

/* Memory size settings for the experiment */
#define NUM_PAGES    1024       /* Number of pages to allocate (1024 pages) */
#define PAGE_SIZE    4096       /* Page size (4KB, typical system default) */
#define REGION_SIZE  (NUM_PAGES * PAGE_SIZE)  /* Total 4MB (1024 * 4KB) */

/*
 * get_minor_faults - Returns the current process's minor page fault count.
 *
 * minor page fault: a page fault handled without disk I/O.
 * Page copies caused by COW are recorded as minor page faults.
 */
static long get_minor_faults(void)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_minflt;
}

/*
 * get_time_us - Returns the current time in microseconds.
 */
static long long get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

/*
 * demo_cow_basic - Demonstrates the basic behavior of COW.
 *
 * Observes the difference in minor page fault count between
 * when a child only reads and when it writes after fork().
 */
static void demo_cow_basic(void)
{
    char *region;
    int i;

    printf("=== 1. COW Fork Basic Behavior ===\n\n");

    /* Allocate memory region
     * mmap flag explanation:
     *   PROT_READ | PROT_WRITE: set read/write permissions
     *   MAP_PRIVATE: process-private mapping (subject to COW on fork)
     *   MAP_ANONYMOUS: anonymous memory, not file-backed (initialized to 0)
     * MAP_PRIVATE is the key: this flag causes COW copying on write after fork */
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* Write data to all pages (ensure physical pages are allocated) */
    for (i = 0; i < NUM_PAGES; i++) {
        region[i * PAGE_SIZE] = (char)('A' + (i % 26));
    }
    printf("[Parent] Data written to %d pages (%d KB)\n",
           NUM_PAGES, REGION_SIZE / 1024);

    /* Record minor fault count before fork
     * minor page fault: a page fault handled by the kernel without disk I/O
     * COW copies are classified as minor faults (copying pages already in memory) */
    long faults_before_fork = get_minor_faults();
    printf("[Parent] Minor page faults before fork: %ld\n\n", faults_before_fork);

    /* fork() call: at this point the kernel copies all of parent's PTEs to the child,
     * without copying physical pages (COW). Both parent and child have write permission
     * removed, and the COW flag is set. Each physical page's reference count increases to 2 */
    pid_t pid = fork();

    if (pid == 0) {
        /* ---- Child process ---- */
        long faults_after_fork = get_minor_faults();
        printf("[Child] Minor page faults right after fork: %ld\n", faults_after_fork);

        /* Read only: no COW copy should occur
         * Reading does not require write permission, so no page fault occurs
         * Parent and child can read from the same shared physical pages */
        long faults_before_read = get_minor_faults();
        volatile char sum = 0;
        for (i = 0; i < NUM_PAGES; i++) {
            sum += region[i * PAGE_SIZE];
        }
        (void)sum;
        long faults_after_read = get_minor_faults();
        printf("[Child] Minor page faults after read: %ld (additional faults from read: %ld)\n",
               faults_after_read, faults_after_read - faults_before_read);

        /* Write: COW copy should occur
         * On first write to each page:
         *   1. Store page fault occurs due to missing write permission
         *   2. Kernel's page fault handler detects it's a COW page
         *   3. Allocates a new physical page and copies original contents
         *   4. Updates PTE to point to the new page (grants write permission)
         *   5. Decrements the original page's reference count
         * This process is recorded as a minor page fault */
        long faults_before_write = get_minor_faults();
        for (i = 0; i < NUM_PAGES; i++) {
            region[i * PAGE_SIZE] = 'Z';  /* Write to each page -> COW triggered */
        }
        long faults_after_write = get_minor_faults();
        printf("[Child] Minor page faults after write: %ld (additional faults from write: %ld)\n",
               faults_after_write, faults_after_write - faults_before_write);
        long sys_pgsize = sysconf(_SC_PAGESIZE);
        long expected = (long)NUM_PAGES * PAGE_SIZE / sys_pgsize;
        printf("[Child] -> Expected about %ld COW faults based on system page size (actual: %ld)\n",
               expected, faults_after_write - faults_before_write);
        printf("         (system page size = %ld bytes, program PAGE_SIZE = %d bytes)\n\n",
               sys_pgsize, PAGE_SIZE);

        munmap(region, REGION_SIZE);
        exit(0);
    } else if (pid > 0) {
        /* ---- Parent process ---- */
        wait(NULL);

        /* Verify that parent's data has not been modified
         * Key point of COW: child's writes do not affect parent's data
         * Since a new physical page was allocated for the child's writes,
         * parent retains the original physical page unchanged */
        int ok = 1;
        for (i = 0; i < NUM_PAGES; i++) {
            if (region[i * PAGE_SIZE] != (char)('A' + (i % 26))) {
                ok = 0;
                break;
            }
        }
        printf("[Parent] Parent data verification after child's write: %s\n",
               ok ? "Unchanged (PASS)" : "Modified! (FAIL)");
        printf("         -> Thanks to COW, child's writes do not affect the parent\n\n");

        munmap(region, REGION_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

/*
 * demo_cow_performance - Demonstrates the performance advantage of COW fork.
 *
 * Advantage of COW in the fork + exec pattern:
 * - Traditional eager copy fork: physically copies all parent pages
 *   -> If parent uses 4MB, fork incurs the cost of copying 4MB
 * - COW fork: only copies PTEs (no physical page copying)
 *   -> After fork, if exec is called, the child's address space is replaced,
 *      so copied pages would be immediately discarded -> eager copy is wasteful
 *
 * This demo measures the speed of fork itself to demonstrate COW's efficiency.
 */
static void demo_cow_performance(void)
{
    char *region;
    int i;

    printf("=== 2. COW Fork Performance Comparison ===\n\n");

    /* Allocate a large memory region */
    region = mmap(NULL, REGION_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1, 0);
    if (region == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    /* Write data to all pages */
    for (i = 0; i < NUM_PAGES; i++) {
        memset(region + i * PAGE_SIZE, 'X', PAGE_SIZE);
    }

    /* Measure fork time */
    long long t1 = get_time_us();
    pid_t pid = fork();
    long long t2 = get_time_us();

    if (pid == 0) {
        /* Child: just fork and exit immediately (exec simulation) */
        printf("[Child] Fork elapsed time: %lld us\n", t2 - t1);
        printf("[Child] -> Thanks to COW, fork completed instantly without copying %d KB\n",
               REGION_SIZE / 1024);
        printf("[Child] -> With eager copy, all %d pages would have needed to be copied\n\n",
               NUM_PAGES);
        munmap(region, REGION_SIZE);
        exit(0);
    } else if (pid > 0) {
        wait(NULL);
        munmap(region, REGION_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

/*
 * demo_cow_isolation - Verifies that process isolation is maintained with COW.
 *
 * COW is a performance optimization technique, but process isolation is fully maintained.
 * - Right after fork: parent and child share the same physical pages and read the same values
 * - After write: COW copy gives each process its own independent physical page
 * - Result: modifications on one side have no effect on the other
 *
 * This contrasts with MAP_SHARED: MAP_SHARED shares writes too, so
 * modifications on one side are visible on the other (used for IPC).
 */
static void demo_cow_isolation(void)
{
    int *shared_val;

    printf("=== 3. COW Process Isolation Verification ===\n\n");

    /* Allocate memory to store an integer value */
    shared_val = mmap(NULL, PAGE_SIZE,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
    if (shared_val == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    *shared_val = 42;
    printf("[Parent] Value before fork: %d\n", *shared_val);

    pid_t pid = fork();

    if (pid == 0) {
        /* Child process */
        printf("[Child] Value right after fork: %d (sharing the same physical page as parent)\n",
               *shared_val);

        /* MAP_PRIVATE causes COW on write */
        *shared_val = 100;
        printf("[Child] Value after modification: %d (written to a separate physical page due to COW)\n",
               *shared_val);

        munmap(shared_val, PAGE_SIZE);
        exit(0);
    } else if (pid > 0) {
        wait(NULL);
        printf("[Parent] Parent's value after child exits: %d (unchanged!)\n", *shared_val);
        printf("         -> COW guarantees process isolation\n\n");

        munmap(shared_val, PAGE_SIZE);
    } else {
        perror("fork");
        exit(1);
    }
}

int main(void)
{
    printf("=========================================\n");
    printf("  COW (Copy-On-Write) Demonstration\n");
    printf("=========================================\n\n");
    printf("System page size: %d bytes\n", (int)sysconf(_SC_PAGESIZE));
    printf("Allocated region size: %d pages (%d KB)\n\n", NUM_PAGES, REGION_SIZE / 1024);

    demo_cow_basic();
    demo_cow_performance();
    demo_cow_isolation();

    printf("=========================================\n");
    printf("  Summary\n");
    printf("=========================================\n");
    printf("1. After fork(), read-only access does not trigger COW copying\n");
    printf("2. Writing causes a minor page fault and the page is copied\n");
    printf("3. Thanks to COW, parent and child data are fully isolated\n");
    printf("4. In the fork + exec pattern, unnecessary memory copying is avoided\n");

    return 0;
}
