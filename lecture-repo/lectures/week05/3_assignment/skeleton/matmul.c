/*
 * matmul.c - OpenMP Matrix Multiplication (Bonus)
 *
 * Compares three versions of matrix multiplication:
 *   1. Sequential (baseline)
 *   2. OpenMP basic (#pragma omp parallel for)
 *   3. OpenMP optimized (loop reordering i-k-j + collapse)
 *
 * Compile: gcc -Wall -fopenmp -O2 -o matmul matmul.c
 * Run:     ./matmul [matrix_size]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define DEFAULT_N 1024

/* Matrices stored as flat arrays for cache friendliness */
double *A, *B, *C;
int N;

#define IDX(i, j) ((i) * N + (j))

/* Initialize matrices: A and B with random values, C with zeros */
void init_matrices(void)
{
    srand(42);
    for (int i = 0; i < N * N; i++) {
        A[i] = (double)(rand() % 100) / 10.0;
        B[i] = (double)(rand() % 100) / 10.0;
    }
}

void zero_C(void)
{
    memset(C, 0, N * N * sizeof(double));
}

/*
 * matmul_sequential: Standard i-j-k triple loop.
 */
void matmul_sequential(void)
{
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[IDX(i, j)] += A[IDX(i, k)] * B[IDX(k, j)];
}

/*
 * matmul_openmp_basic: Add #pragma omp parallel for to outer loop.
 */
void matmul_openmp_basic(void)
{
    /*
     * TODO: Parallelize the outer loop with OpenMP.
     *
     * Add #pragma omp parallel for before the outermost for loop.
     * The i-j-k loop body is the same as sequential.
     *
     *   #pragma omp parallel for
     *   for (int i = 0; i < N; i++)
     *       for (int j = 0; j < N; j++)
     *           for (int k = 0; k < N; k++)
     *               C[IDX(i, j)] += A[IDX(i, k)] * B[IDX(k, j)];
     */
}

/*
 * matmul_openmp_optimized: Loop reordering (i-k-j) + OpenMP.
 *
 * Why i-k-j? In the innermost loop (j), both C[i][j] and B[k][j]
 * are accessed with stride 1 (sequential memory access).
 * In i-j-k order, B[k][j] has stride N (cache-unfriendly).
 */
void matmul_openmp_optimized(void)
{
    /*
     * TODO: Implement the i-k-j loop order with OpenMP.
     *
     * Steps:
     *   1. Add: #pragma omp parallel for
     *   2. Outer loop: for i
     *   3. Middle loop: for k
     *   4. Inner loop: for j
     *      C[IDX(i, j)] += A[IDX(i, k)] * B[IDX(k, j)];
     *
     * This reordering makes B access sequential in the innermost loop,
     * which dramatically improves cache hit rate.
     */
}

/* Verify two results match (within floating-point tolerance) */
int verify(double *ref, double *test, int size)
{
    for (int i = 0; i < size; i++) {
        double diff = ref[i] - test[i];
        if (diff > 1e-6 || diff < -1e-6)
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    N = (argc > 1) ? atoi(argv[1]) : DEFAULT_N;

    printf("=== OpenMP Matrix Multiplication ===\n");
    printf("Matrix size: %d x %d\n", N, N);
    printf("Threads: %d\n\n", omp_get_max_threads());

    A = malloc(N * N * sizeof(double));
    B = malloc(N * N * sizeof(double));
    C = malloc(N * N * sizeof(double));
    double *C_ref = malloc(N * N * sizeof(double));

    init_matrices();
    double t0, t1;

    /* Sequential */
    zero_C();
    t0 = omp_get_wtime();
    matmul_sequential();
    t1 = omp_get_wtime();
    double seq_time = t1 - t0;
    printf("Sequential (i-j-k):       %.4f sec\n", seq_time);
    memcpy(C_ref, C, N * N * sizeof(double));

    /* OpenMP basic */
    zero_C();
    t0 = omp_get_wtime();
    matmul_openmp_basic();
    t1 = omp_get_wtime();
    double omp_basic_time = t1 - t0;
    int ok1 = verify(C_ref, C, N * N);
    printf("OpenMP basic:             %.4f sec  (%.2fx) %s\n",
           omp_basic_time, seq_time / omp_basic_time,
           ok1 ? "" : "MISMATCH!");

    /* OpenMP optimized */
    zero_C();
    t0 = omp_get_wtime();
    matmul_openmp_optimized();
    t1 = omp_get_wtime();
    double omp_opt_time = t1 - t0;
    int ok2 = verify(C_ref, C, N * N);
    printf("OpenMP optimized (i-k-j): %.4f sec  (%.2fx) %s\n",
           omp_opt_time, seq_time / omp_opt_time,
           ok2 ? "" : "MISMATCH!");

    printf("\nVerified: %s\n", (ok1 && ok2) ? "CORRECT" : "ERRORS FOUND");

    free(A);
    free(B);
    free(C);
    free(C_ref);
    return (ok1 && ok2) ? 0 : 1;
}
