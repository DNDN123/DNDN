/*
 * lab2_openmp_parallel.c - OpenMP Parallel For and Reduction
 *
 * Compile: gcc -Wall -fopenmp -o lab2_openmp_parallel lab2_openmp_parallel.c
 * Run:     ./lab2_openmp_parallel
 */

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

#define ARRAY_SIZE 50000000

double array[ARRAY_SIZE];

void demo1_parallel_hello(void)
{
    printf("--- Demo 1: Parallel Hello ---\n");

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int total = omp_get_num_threads();
        printf("[Thread %d/%d] Hello from OpenMP!\n", tid, total);
    }

    printf("\n");
}

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

    /* Parallel version */
    t_start = omp_get_wtime();
    #pragma omp parallel for
    for (int i = 0; i < ARRAY_SIZE; i++)
        array[i] = (double)i * 1.5;
    t_end = omp_get_wtime();
    printf("Parallel:   %.4f sec\n", t_end - t_start);

    printf("\n");
}

void demo3_reduction(void)
{
    printf("--- Demo 3: Reduction ---\n");
    double t_start, t_end;

    for (int i = 0; i < ARRAY_SIZE; i++)
        array[i] = 1.0;

    /* Sequential sum */
    double sum_seq = 0.0;
    t_start = omp_get_wtime();
    for (int i = 0; i < ARRAY_SIZE; i++)
        sum_seq += array[i];
    t_end = omp_get_wtime();
    printf("Sequential sum: %.0f, Time: %.4f sec\n", sum_seq, t_end - t_start);

    /* Parallel sum with reduction */
    double sum_par = 0.0;
    t_start = omp_get_wtime();
    #pragma omp parallel for reduction(+:sum_par)
    for (int i = 0; i < ARRAY_SIZE; i++)
        sum_par += array[i];
    t_end = omp_get_wtime();
    printf("Parallel sum:   %.0f, Time: %.4f sec\n", sum_par, t_end - t_start);

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
