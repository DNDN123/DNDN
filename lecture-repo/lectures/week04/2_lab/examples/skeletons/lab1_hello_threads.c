/*
 * lab1_hello_threads.c - Basic Thread Creation and Joining
 *
 * [Key Concept]
 * A thread is a unit of CPU utilization within a process.
 * Threads in the same process share code, data, and files,
 * but each has its own stack, registers, and program counter.
 *
 * This program creates multiple threads using the Pthreads API.
 * Each thread prints a greeting message, and the main thread
 * waits for all of them to finish using pthread_join().
 *
 * Observe that the order of the printed messages is
 * non-deterministic -- it depends on OS scheduling.
 *
 * Compile: gcc -Wall -pthread -o lab1_hello_threads lab1_hello_threads.c
 * Run:     ./lab1_hello_threads [num_threads]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Default number of threads to create */
#define DEFAULT_THREADS 4

/*
 * Function executed by each thread.
 * arg: pointer to an int containing the thread ID
 *
 * Each thread prints a greeting, showing its thread ID.
 * All threads share the same code but run independently.
 */
void *greet(void *arg)
{
    int tid = *(int *)arg;

    /*
     * TODO: Print a greeting message from this thread.
     *
     * Use printf to print a message like:
     *   "[Thread <tid>] Hello from thread <tid>!\n"
     *
     * Then return NULL to indicate the thread is done.
     *
     * Hint:
     *   printf("[Thread %d] Hello from thread %d!\n", tid, tid);
     *   return NULL;
     */

    return NULL;
}

int main(int argc, char *argv[])
{
    /* Parse the number of threads from command-line arguments */
    int nthreads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;

    if (nthreads <= 0) {
        fprintf(stderr, "Usage: %s [num_threads]\n", argv[0]);
        return 1;
    }

    printf("=== Hello Threads Demo ===\n");
    printf("Creating %d threads...\n\n", nthreads);

    /*
     * Dynamically allocate arrays for thread handles and IDs.
     * - threads[]: stores the pthread_t identifier for each thread
     * - tids[]:    stores a unique integer ID for each thread
     *
     * We use a separate tids[] array (not &i) to avoid a data
     * dependency bug. This is explained in Lab 3.
     */
    pthread_t *threads = malloc(sizeof(pthread_t) * nthreads);
    int *tids = malloc(sizeof(int) * nthreads);

    /*
     * TODO: Create threads using pthread_create().
     *
     * For each thread i (from 0 to nthreads - 1):
     *   1. Set tids[i] = i
     *   2. Call pthread_create(&threads[i], NULL, greet, &tids[i])
     *      - &threads[i]: location to store the thread handle
     *      - NULL: use default thread attributes
     *      - greet: function the thread will execute
     *      - &tids[i]: argument passed to the greet function
     */

    /*
     * TODO: Wait for all threads to finish using pthread_join().
     *
     * For each thread i (from 0 to nthreads - 1):
     *   Call pthread_join(threads[i], NULL)
     *   - threads[i]: handle of the thread to wait for
     *   - NULL: we don't need the thread's return value
     *
     * Without pthread_join(), the main thread might exit before
     * the child threads finish, and their output may be lost.
     */

    printf("\nAll %d threads finished.\n", nthreads);

    /* Free dynamically allocated memory */
    free(threads);
    free(tids);
    return 0;
}
