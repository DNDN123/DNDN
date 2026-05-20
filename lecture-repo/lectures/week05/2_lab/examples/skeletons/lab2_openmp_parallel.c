/*
 * lab2_openmp_parallel.c - OpenMP Parallel For and Reduction
 *
 * [Key Concept]
 * OpenMP is a set of compiler directives that make it easy to
 * add parallelism to existing sequential code. Instead of
 * manually creating threads, you annotate loops or sections
 * with #pragma omp directives.
 *
 * Key directives used in this lab:
 *   #pragma omp parallel         — create a team of threads
 *   #pragma omp parallel for     — parallelize a for loop
 *   #pragma omp parallel for reduction(+:var) — parallel sum
 *
 * Compile: gcc -Wall -fopenmp -o lab2_openmp_parallel lab2_openmp_parallel.c
 * Run:     ./lab2_openmp_parallel
 *
 * Note: The -fopenmp flag is required to enable OpenMP support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

#define ARRAY_SIZE 50000000  /* 50 million */

double array[ARRAY_SIZE];

/*
 * demo1_parallel_hello: Each thread prints its ID.
 * Shows basic thread creation with OpenMP.
 */
void demo1_parallel_hello(void)
{
    printf("--- Demo 1: Parallel Hello ---\n");

    /*
     * TODO: Use #pragma omp parallel to create a team of threads.
     *
     * Inside the parallel block:
     *   int tid = omp_get_thread_num();
     *   int total = omp_get_num_threads();
     *   printf("[Thread %d/%d] Hello from OpenMP!\n", tid, total);
     *
     * Hint:
     *   #pragma omp parallel
     *   {
     *       // code here runs on all threads
     *   }
     */

    printf("\n");
}

/*
 * demo2_parallel_for: Parallelize array initialization.
 * Compare sequential vs parallel performance.
 */
void demo2_parallel_for(void)
{
    printf("--- Demo 2: Parallel For ---\n");
    double t_start, t_end;

    /* Sequential version */
    t_start = omp_get_wtime();
    for (int i = 0; i < ARRAY_SIZE; i++)
        array[i] = (double)i * 1.5;
    t_end = omp_get_wtime();
    printf("Sequential: %.4f sec\n", t_end - t_start);

    /*
     * TODO: Write the parallel version using #pragma omp parallel for.
     *
     * Steps:
     *   1. Record start time: t_start = omp_get_wtime();
     *   2. Add the pragma before the for loop:
     *      #pragma omp parallel for
     *      for (int i = 0; i < ARRAY_SIZE; i++)
     *          array[i] = (double)i * 1.5;
     *   3. Record end time and print.
     *
     * OpenMP automatically splits the loop iterations among threads.
     */

    printf("\n");
}

/*
 * demo3_reduction: Parallel sum using reduction clause.
 * Without reduction, concurrent += would cause a race condition.
 */
void demo3_reduction(void)
{
    printf("--- Demo 3: Reduction ---\n");
    double t_start, t_end;

    /* Initialize array */
    for (int i = 0; i < ARRAY_SIZE; i++)
        array[i] = 1.0;

    /* Sequential sum */
    double sum_seq = 0.0;
    t_start = omp_get_wtime();
    for (int i = 0; i < ARRAY_SIZE; i++)
        sum_seq += array[i];
    t_end = omp_get_wtime();
    printf("Sequential sum: %.0f, Time: %.4f sec\n", sum_seq, t_end - t_start);

    /*
     * TODO: Write the parallel version using reduction(+:sum_par).
     *
     * Steps:
     *   1. Declare: double sum_par = 0.0;
     *   2. Record start time.
     *   3. Use:
     *      #pragma omp parallel for reduction(+:sum_par)
     *      for (int i = 0; i < ARRAY_SIZE; i++)
     *          sum_par += array[i];
     *   4. Record end time and print sum_par and elapsed time.
     *
     * The reduction clause gives each thread a private copy of sum_par,
     * then combines them with + at the end. No race condition!
     */

    printf("\n");
}

int main(void)
{
    printf("=== OpenMP Parallel Demo ===\n");
    printf("Available threads: %d\n\n", omp_get_max_threads());

    demo1_parallel_hello();
    demo2_parallel_for();
    demo3_reduction();

    return 0;
}
