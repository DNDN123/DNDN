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

/* Thread argument structure */
struct thread_arg {
    int id;
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

    /* Compute partial sum for this thread's chunk */
    partial_sum[id] = 0;
    for (int i = start; i < end; i++)
        partial_sum[id] += array[i];

    printf("[Thread %d] range [%d, %d), partial_sum = %d\n",
           id, start, end, partial_sum[id]);

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

    /* Join all threads and compute the total sum */
    int total = 0;
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
        total += partial_sum[i];
    }

    printf("\nTotal sum = %d\n", total);

    if (total == 500500)
        printf("SUCCESS! Data parallel summation is correct.\n");
    else
        printf("ERROR: expected 500500, got %d\n", total);

    return 0;
}
