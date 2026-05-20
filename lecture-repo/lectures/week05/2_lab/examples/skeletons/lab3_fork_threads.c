/*
 * lab3_fork_threads.c - fork() in a Multithreaded Program
 *
 * [Key Concept]
 * When a multithreaded process calls fork(), only the calling
 * thread is duplicated in the child process. All other threads
 * simply disappear. This can lead to problems if those threads
 * held locks or were doing important work.
 *
 * Two variants of fork() semantics:
 *   1. Duplicate ALL threads (rarely used)
 *   2. Duplicate ONLY the calling thread (POSIX default)
 *
 * Rule of thumb:
 *   - If the child calls exec() immediately → fork only caller (safe)
 *   - If the child continues executing → beware of lost threads/locks
 *
 * This program creates threads, then forks, and shows that only
 * the calling thread exists in the child process.
 *
 * Compile: gcc -Wall -pthread -o lab3_fork_threads lab3_fork_threads.c
 * Run:     ./lab3_fork_threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>

#define NUM_THREADS 3

/* Global counter incremented by background threads */
volatile int counter = 0;

/*
 * background_work: A thread that periodically increments a counter.
 * In the parent process, this runs continuously.
 * In the child process, this thread does NOT exist.
 */
void *background_work(void *arg)
{
    int tid = *(int *)arg;
    printf("[Parent] Thread %d started (pid=%d)\n", tid, getpid());

    while (1) {
        counter++;
        usleep(200000);  /* 200ms */
    }
    return NULL;
}

/*
 * demo1_fork_only_caller: Show that fork() only copies the calling thread.
 */
void demo1_fork_only_caller(void)
{
    printf("--- Demo 1: fork() copies only the calling thread ---\n\n");

    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];

    /* Create background threads in the parent */
    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, background_work, &tids[i]);
    }

    /* Let threads run for a bit */
    usleep(500000);
    printf("[Parent] counter = %d (pid=%d, %d threads running)\n\n",
           counter, getpid(), NUM_THREADS);

    /*
     * TODO: Call fork() and observe the behavior in parent vs child.
     *
     * Steps:
     *   1. pid_t pid = fork();
     *
     *   2. In the child (pid == 0):
     *      - Print: "[Child] I am the child (pid=%d)\n", getpid()
     *      - Print: "[Child] counter = %d\n", counter
     *      - Sleep 500ms: usleep(500000)
     *      - Print counter again: "[Child] counter after 500ms = %d\n", counter
     *      - Print: "[Child] counter did NOT change — background threads are gone!\n"
     *      - Call exit(0)
     *
     *   3. In the parent (pid > 0):
     *      - Print: "[Parent] forked child pid=%d\n", pid
     *      - Sleep 500ms: usleep(500000)
     *      - Print: "[Parent] counter = %d (still incrementing)\n", counter
     *      - Wait for child: waitpid(pid, NULL, 0)
     *
     * Key observation: In the child, counter stays the same because
     * the background threads were NOT copied by fork().
     */
}

/*
 * demo2_fork_then_exec: The safe pattern — fork + exec.
 */
void demo2_fork_then_exec(void)
{
    printf("\n--- Demo 2: fork() + exec() — the safe pattern ---\n\n");

    /*
     * TODO: Fork and immediately exec a simple command.
     *
     * Steps:
     *   1. pid_t pid = fork();
     *   2. In the child (pid == 0):
     *      - Print: "[Child] About to exec 'echo'\n"
     *      - Call: execlp("echo", "echo", "Hello from exec!", NULL)
     *      - Print error if exec fails: perror("exec failed")
     *      - exit(1)
     *   3. In the parent (pid > 0):
     *      - waitpid(pid, NULL, 0)
     *      - Print: "[Parent] Child finished exec.\n"
     *
     * When exec() is called, the entire address space is replaced,
     * so it doesn't matter that other threads are missing.
     */
}

int main(void)
{
    printf("=== fork() in Multithreaded Programs ===\n");
    printf("Parent pid = %d\n\n", getpid());

    demo1_fork_only_caller();
    demo2_fork_then_exec();

    printf("\nKey takeaway: fork() only copies the calling thread.\n");
    printf("If the child needs to do work, call exec() immediately.\n");
    return 0;
}
