/*
 * lab4_speedup.c - Measuring Speedup with Multiple Threads
 *
 * [Key Concept]
 * Amdahl's Law states that the speedup of a program from
 * parallelization is limited by its serial fraction S:
 *
 *   speedup <= 1 / (S + (1-S)/N)
 *
 * where N is the number of processing cores (threads).
 *
 * This program measures the wall-clock time for a computation
 * (summing a large array) using 1, 2, 4, and 8 threads, and
 * displays the speedup relative to the single-threaded baseline.
 *
 * Compile: gcc -Wall -O2 -pthread -o lab4_speedup lab4_speedup.c
 * Run:     ./lab4_speedup
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

/* Large array size for measurable computation time */
#define ARRAY_SIZE  50000000   /* 50 million */
#define MAX_THREADS 8

/* Global array (dynamically allocated in main) */
double *array;

/* Each thread stores its partial result here */
double partial_sum[MAX_THREADS];

/* Number of threads for the current run */
int nthreads;

/* Thread argument structure */
struct thread_arg {
    int id;
};

/*
 * Thread function: computes the sum of its assigned chunk.
 */
void *sum_chunk(void *arg)
{
    struct thread_arg *targ = (struct thread_arg *)arg;
    int id = targ->id;

    int chunk = ARRAY_SIZE / nthreads;
    int start = id * chunk;
    int end   = (id == nthreads - 1) ? ARRAY_SIZE : start + chunk;

    double local_sum = 0.0;
    for (int i = start; i < end; i++)
        local_sum += array[i];

    partial_sum[id] = local_sum;
    return NULL;
}

/*
 * run_with_threads: Runs the parallel sum with the given number
 * of threads and returns the elapsed time in seconds.
 */
double run_with_threads(int num_threads)
{
    nthreads = num_threads;

    pthread_t threads[MAX_THREADS];
    struct thread_arg targs[MAX_THREADS];

    /*
     * TODO: Measure the elapsed time for the parallel computation.
     *
     * Steps:
     *   1. Record the start time:
     *        struct timespec t_start, t_end;
     *        clock_gettime(CLOCK_MONOTONIC, &t_start);
     *
     *   2. Create num_threads threads:
     *      for (int i = 0; i < num_threads; i++) {
     *          targs[i].id = i;
     *          pthread_create(&threads[i], NULL, sum_chunk, &targs[i]);
     *      }
     *
     *   3. Join all threads and accumulate the total:
     *      double total = 0.0;
     *      for (int i = 0; i < num_threads; i++) {
     *          pthread_join(threads[i], NULL);
     *          total += partial_sum[i];
     *      }
     *
     *   4. Record the end time:
     *        clock_gettime(CLOCK_MONOTONIC, &t_end);
     *
     *   5. Compute elapsed time in seconds:
     *        double elapsed = (t_end.tv_sec - t_start.tv_sec)
     *                       + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
     *
     *   6. Print and return elapsed:
     *        printf("  Threads: %d, Sum: %.0f, Time: %.4f sec\n",
     *               num_threads, total, elapsed);
     *        return elapsed;
     */

    return 0.0;  /* placeholder */
}

int main(void)
{
    printf("=== Speedup Measurement Demo ===\n");
    printf("Array size: %d (%.0f MB)\n\n",
           ARRAY_SIZE, ARRAY_SIZE * sizeof(double) / 1e6);

    /* Allocate and initialize the array: array[i] = 1.0 */
    array = malloc(sizeof(double) * ARRAY_SIZE);
    if (!array) {
        fprintf(stderr, "Failed to allocate array\n");
        return 1;
    }
    for (int i = 0; i < ARRAY_SIZE; i++)
        array[i] = 1.0;

    /* Thread counts to test */
    int thread_counts[] = {1, 2, 4, 8};
    int num_tests = sizeof(thread_counts) / sizeof(thread_counts[0]);
    double times[4];

    /* Run the computation with each thread count */
    for (int t = 0; t < num_tests; t++)
        times[t] = run_with_threads(thread_counts[t]);

    /* Display speedup relative to single-threaded baseline */
    printf("\n--- Speedup Summary ---\n");
    printf("%-10s %-12s %-10s\n", "Threads", "Time (sec)", "Speedup");
    for (int t = 0; t < num_tests; t++) {
        double speedup = (times[0] > 0) ? times[0] / times[t] : 0;
        printf("%-10d %-12.4f %-10.2fx\n",
               thread_counts[t], times[t], speedup);
    }

    printf("\nCompare with Amdahl's Law: speedup <= 1 / (S + (1-S)/N)\n");
    printf("Ideal speedup (S=0): 1x, 2x, 4x, 8x\n");

    free(array);
    return 0;
}
