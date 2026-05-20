/*
 * lab1_fork_basic.c - Example demonstrating basic behavior of fork() and wait()
 *
 * This program covers the fork() system call, which is the core of process creation,
 * and the wait() system call, which allows a parent to wait for a child to terminate.
 *
 * Learning objectives:
 *   1. How to distinguish parent/child based on fork() return value
 *   2. Waiting for child process termination using wait()
 *   3. Checking exit status with WIFEXITED / WEXITSTATUS macros
 *   4. Creating and managing multiple child processes
 *
 * Compile: gcc -Wall -o fork_basic lab1_fork_basic.c
 * Run:     ./fork_basic
 */

#include <stdio.h>      /* printf, perror */
#include <stdlib.h>      /* exit */
#include <unistd.h>      /* fork, getpid, getppid, sleep, usleep */
#include <sys/wait.h>    /* wait, waitpid, WIFEXITED, WEXITSTATUS */

int main(void)
{
    printf("=== fork() + wait() basic example ===\n\n");

    /*
     * fork() duplicates the current process to create a child process.
     * It is a unique system call where one call results in two returns:
     *   - In the parent process, it returns the child's PID (positive)
     *   - In the child process, it returns 0
     *   - On failure, it returns -1 (no child is created)
     *
     * Right after fork(), both parent and child execute the same code,
     * but since the return values differ, we use if-else to branch
     * and perform different tasks in each.
     */
    printf("[Parent PID=%d] Before fork() call\n", getpid());

    /* TODO: Call fork() and store the return value in a pid_t variable called 'pid'.
     *       Then use if/else to handle the three cases:
     *
     *       1. pid < 0: fork failed
     *          - Call perror("fork") and exit(1)
     *
     *       2. pid == 0: child process
     *          - Print: "[Child PID=%d] I am the child process! Parent PID=%d\n"
     *            using getpid() and getppid()
     *          - Print: "[Child PID=%d] Performing work...\n"
     *          - Call sleep(1) to simulate work
     *          - Print: "[Child PID=%d] Work complete, exiting with exit(42)\n"
     *          - Call exit(42) to terminate the child with exit code 42
     *
     *       3. else (pid > 0): parent process
     *          - Print: "[Parent PID=%d] fork() succeeded! Child PID=%d\n"
     *          - Declare int status, then call wait(&status) and store result in pid_t terminated
     *          - If terminated < 0, call perror("wait") and exit(1)
     *          - Print: "[Parent PID=%d] Child (PID=%d) has terminated\n"
     *          - Use WIFEXITED(status) to check normal termination,
     *            then WEXITSTATUS(status) to print the exit code:
     *            "[Parent PID=%d] Child's exit code: %d\n"
     */

    /* ------------------------------------------------------------ */
    /* Additional experiment: Calling fork() in a loop to create     */
    /* multiple children                                             */
    /* ------------------------------------------------------------ */
    printf("\n=== Additional experiment: Creating multiple children with fork() ===\n\n");

    int num_children = 3;  /* Number of child processes to create */

    /* TODO: Create multiple children in a for loop (i = 0 to num_children-1).
     *       For each iteration:
     *
     *       1. Call fork() and store result in pid_t child
     *       2. If child < 0: perror("fork") and exit(1)
     *       3. If child == 0 (child process):
     *          - Print: "  Child #%d (PID=%d) started\n"
     *          - Call usleep((unsigned int)(100000 * (num_children - i)))
     *            to make children terminate in reverse order
     *          - Print: "  Child #%d (PID=%d) exiting\n"
     *          - IMPORTANT: Call exit(i) so the child does not continue the loop!
     *            (Without exit, the child would fork more children -> "fork bomb")
     *       4. Parent continues the loop to fork the next child
     */

    /*
     * Parent: Repeatedly calls wait() until all children have terminated.
     * wait() returns immediately if a terminated child exists,
     * or blocks until one terminates if none have yet.
     */
    for (int i = 0; i < num_children; i++) {
        int status;
        pid_t done = wait(&status);
        if (WIFEXITED(status)) {
            printf("[Parent] Child PID=%d terminated (exit code=%d)\n",
                   done, WEXITSTATUS(status));
        }
    }

    /*
     * The termination order of children may differ from the creation order.
     * This is because the OS scheduler decides which process runs first.
     * In this example, usleep() was used to intentionally induce reverse-order termination.
     */
    printf("\nNote: The termination order of children may differ from the creation order!\n");

    return 0;
}
