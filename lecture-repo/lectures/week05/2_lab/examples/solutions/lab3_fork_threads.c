/*
 * lab3_fork_threads.c - fork() in a Multithreaded Program
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

volatile int counter = 0;

void *background_work(void *arg)
{
    int tid = *(int *)arg;
    printf("[Parent] Thread %d started (pid=%d)\n", tid, getpid());

    while (1) {
        counter++;
        usleep(200000);
    }
    return NULL;
}

void demo1_fork_only_caller(void)
{
    printf("--- Demo 1: fork() copies only the calling thread ---\n\n");

    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, background_work, &tids[i]);
    }

    usleep(500000);
    printf("[Parent] counter = %d (pid=%d, %d threads running)\n\n",
           counter, getpid(), NUM_THREADS);

    pid_t pid = fork();

    if (pid == 0) {
        /* Child process */
        printf("[Child] I am the child (pid=%d)\n", getpid());
        printf("[Child] counter = %d\n", counter);
        usleep(500000);
        printf("[Child] counter after 500ms = %d\n", counter);
        printf("[Child] counter did NOT change — background threads are gone!\n");
        exit(0);
    } else {
        /* Parent process */
        printf("[Parent] forked child pid=%d\n", pid);
        usleep(500000);
        printf("[Parent] counter = %d (still incrementing)\n", counter);
        waitpid(pid, NULL, 0);
    }
}

void demo2_fork_then_exec(void)
{
    printf("\n--- Demo 2: fork() + exec() — the safe pattern ---\n\n");

    pid_t pid = fork();

    if (pid == 0) {
        printf("[Child] About to exec 'echo'\n");
        execlp("echo", "echo", "Hello from exec!", NULL);
        perror("exec failed");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
        printf("[Parent] Child finished exec.\n");
    }
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
