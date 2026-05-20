/*
 * lab2_parallel_sum.c - Data Parallel Array Summation
 *
 * [Key Concept]
 * Data parallelism means distributing subsets of data across
 * multiple threads, where each thread performs the same operation
 * on its own portion. This is one of the two main types of
 * parallelism (the other being task parallelism).
 *
 * This program splits an array of integers across N threads.
 * Each thread computes a partial sum of its assigned chunk,
 * and the main thread combines the partial results.
 *
 * Array: [  0~249  |  250~499  |  500~749  |  750~999  ]
 *            |          |           |            |
 *        Thread 0   Thread 1   Thread 2     Thread 3
 *        partial[0] partial[1] partial[2]   partial[3]
 *            \          |           |          /
 *                      Total Sum
 *
 * Compile: gcc -Wall -pthread -o lab2_parallel_sum lab2_parallel_sum.c
 * Run:     ./lab2_parallel_sum [num_threads]
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Array size and default thread count */
#define ARRAY_SIZE      1000
#define DEFAULT_THREADS 4

/* Global shared array: array[i] = i + 1, so sum = 1+2+...+1000 = 500500 */
int array[ARRAY_SIZE];

/* Each thread stores its result in partial_sum[id] (no sharing conflict) */
int partial_sum[DEFAULT_THREADS];

/* Number of threads (set in main, read by all threads) */
int nthreads;

/*
 * Structure to pass multiple arguments to a thread.
 * Each thread receives its own copy with a unique id.
 */
struct thread_arg {
    int id;     /* thread index (0, 1, ..., nthreads-1) */
};

/*
 * Function executed by each thread.
 * Computes the sum of its assigned chunk of the array.
 */
void *sum_array(void *arg)
{
    struct thread_arg *targ = (struct thread_arg *)arg;
    int id = targ->id;

    /* Calculate the start and end indices for this thread's chunk */
    int chunk = ARRAY_SIZE / nthreads;
    int start = id * chunk;
    int end   = (id == nthreads - 1) ? ARRAY_SIZE : start + chunk;

    /*
     * TODO: Compute the partial sum for this thread's chunk.
     *
     * Steps:
     *   1. Initialize partial_sum[id] to 0
     *   2. Loop from index = start to index < end
     *   3. Add array[index] to partial_sum[id]
     *   4. Print the result:
     *      printf("[Thread %d] range [%d, %d), partial_sum = %d\n",
     *             id, start, end, partial_sum[id]);
     *
     * Note: Each thread writes to partial_sum[id] using its own
     *       unique index, so no synchronization is needed here.
     */

    return NULL;
}

int main(int argc, char *argv[])
{
    nthreads = (argc > 1) ? atoi(argv[1]) : DEFAULT_THREADS;

    if (nthreads <= 0 || nthreads > DEFAULT_THREADS) {
        fprintf(stderr, "Usage: %s [num_threads] (max %d)\n",
                argv[0], DEFAULT_THREADS);
        return 1;
    }

    /* Initialize array: array[i] = i + 1 */
    for (int i = 0; i < ARRAY_SIZE; i++)
        array[i] = i + 1;

    printf("=== Data Parallel Sum Demo ===\n");
    printf("Array size: %d, Threads: %d\n", ARRAY_SIZE, nthreads);
    printf("Expected total: 500500\n\n");

    pthread_t threads[DEFAULT_THREADS];
    struct thread_arg targs[DEFAULT_THREADS];

    /* Create threads: each thread runs sum_array on its chunk */
    for (int i = 0; i < nthreads; i++) {
        targs[i].id = i;
        pthread_create(&threads[i], NULL, sum_array, &targs[i]);
    }

    /*
     * TODO: Join all threads and compute the total sum.
     *
     * Steps:
     *   1. Declare: int total = 0;
     *   2. For each thread i (from 0 to nthreads - 1):
     *      a. Call pthread_join(threads[i], NULL) to wait for it
     *      b. Add partial_sum[i] to total
     *   3. Print the total:
     *      printf("Total sum = %d\n", total);
     *   4. Verify:
     *      if (total == 500500) printf("SUCCESS!\n");
     *      else printf("ERROR: expected 500500\n");
     */

    return 0;
}
