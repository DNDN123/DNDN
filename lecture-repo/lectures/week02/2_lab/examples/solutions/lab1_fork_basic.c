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

    /* fork() call - at this point the process splits into two */
    pid_t pid = fork();

    if (pid < 0) {
        /* fork failed: child creation failed due to insufficient system resources, etc. */
        perror("fork");  /* Print error message stored in errno */
        exit(1);
    } else if (pid == 0) {
        /*
         * Child process section
         * fork() returned 0, so this code runs only in the child process.
         *
         * getpid()  - Returns its own PID
         * getppid() - Returns the parent process's PID
         */
        printf("[Child PID=%d] I am the child process! Parent PID=%d\n",
               getpid(), getppid());
        printf("[Child PID=%d] Performing work...\n", getpid());

        /* sleep(1): Wait 1 second to simulate actual work */
        sleep(1);

        /*
         * exit(42): Terminate the child process with exit code 42.
         * The parent can read this value via WEXITSTATUS() through wait().
         * Exit codes range from 0 to 255, where 0 typically means normal termination.
         */
        printf("[Child PID=%d] Work complete, exiting with exit(42)\n", getpid());
        exit(42);
    } else {
        /*
         * Parent process section
         * fork() returned the child's PID (positive), so this runs only in the parent.
         * The pid variable contains the PID of the child just created.
         */
        printf("[Parent PID=%d] fork() succeeded! Child PID=%d\n", getpid(), pid);

        /*
         * wait(&status): Blocks the parent until one of its children terminates.
         *   - When a child terminates, it returns the terminated child's PID.
         *   - The status variable stores the child's termination status information.
         *   - Returns -1 if there are no children.
         *
         * Without wait(), the parent may finish first, leaving the child
         * as an orphan process.
         */
        int status;
        pid_t terminated = wait(&status);

        if (terminated < 0) {
            perror("wait");
            exit(1);
        }

        printf("[Parent PID=%d] Child (PID=%d) has terminated\n", getpid(), terminated);

        /*
         * WIFEXITED(status): Checks if the child terminated normally by calling exit().
         *   - If true, WEXITSTATUS(status) can be used to retrieve the exit code.
         *   - If the child was terminated abnormally by a signal, use WIFSIGNALED().
         */
        if (WIFEXITED(status)) {
            printf("[Parent PID=%d] Child's exit code: %d\n",
                   getpid(), WEXITSTATUS(status));
        }
    }

    /* ------------------------------------------------------------ */
    /* Additional experiment: Calling fork() in a loop to create     */
    /* multiple children                                             */
    /* ------------------------------------------------------------ */
    printf("\n=== Additional experiment: Creating multiple children with fork() ===\n\n");

    int num_children = 3;  /* Number of child processes to create */

    for (int i = 0; i < num_children; i++) {
        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            exit(1);
        } else if (child == 0) {
            /*
             * Child process: Each child waits for a different amount of time.
             * usleep() waits in microseconds (1/1,000,000 second).
             * Children terminate in reverse order: smaller i waits longer.
             */
            printf("  Child #%d (PID=%d) started\n", i, getpid());
            usleep((unsigned int)(100000 * (num_children - i)));  /* Terminate in reverse order */
            printf("  Child #%d (PID=%d) exiting\n", i, getpid());

            /*
             * Important: The child must call exit().
             * Without exit(), the child would continue the for loop and call fork() again,
             * causing the number of processes to grow exponentially ("fork bomb").
             */
            exit(i);
        }
        /* Parent continues the for loop to fork() the next child */
    }

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
