/*
 * histogram.c - Parallel Histogram using Pthreads
 *
 * Splits a large dataset across multiple threads. Each thread
 * builds a local histogram over its chunk, then the main thread
 * merges the results. No synchronization needed during counting.
 *
 * Compile: gcc -Wall -pthread -O2 -o histogram histogram.c
 * Run:     ./histogram [data_size] [num_threads]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define NUM_BINS     100       /* histogram bins (values 0 to 99) */
#define DEFAULT_SIZE 10000000  /* 10 million */
#define MAX_THREADS  8

/* Global data array (read-only by threads) */
int *data;
int data_size;

/* Thread argument structure */
struct hist_arg {
    int id;
    int start;
    int end;
    int local_hist[NUM_BINS];  /* each thread has its own histogram */
};

/*
 * hist_worker: Count values in data[start..end) into local_hist.
 */
void *hist_worker(void *arg)
{
    struct hist_arg *ha = (struct hist_arg *)arg;

    /*
     * TODO: Build the local histogram for this thread's chunk.
     *
     * Steps:
     *   1. Zero out the local histogram:
     *      memset(ha->local_hist, 0, NUM_BINS * sizeof(int));
     *
     *   2. Loop from i = ha->start to i < ha->end:
     *      ha->local_hist[data[i]]++;
     *
     * Note: Each thread writes ONLY to its own local_hist array,
     *       so no mutex or synchronization is needed.
     */

    return NULL;
}

/*
 * histogram_sequential: Single-threaded baseline.
 */
void histogram_sequential(int *hist)
{
    memset(hist, 0, NUM_BINS * sizeof(int));
    for (int i = 0; i < data_size; i++)
        hist[data[i]]++;
}

/*
 * histogram_parallel: Multi-threaded version.
 * Returns elapsed time in seconds.
 */
double histogram_parallel(int nthreads, int *hist)
{
    pthread_t threads[MAX_THREADS];
    struct hist_arg args[MAX_THREADS];

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /*
     * TODO: Create threads, each processing a chunk of the data.
     *
     * Steps:
     *   1. Calculate chunk size: int chunk = data_size / nthreads;
     *
     *   2. For each thread i (0 to nthreads - 1):
     *      a. Set args[i].id = i
     *      b. Set args[i].start = i * chunk
     *      c. Set args[i].end = (i == nthreads - 1) ? data_size : (i + 1) * chunk
     *      d. pthread_create(&threads[i], NULL, hist_worker, &args[i])
     */

    /*
     * TODO: Join all threads and merge local histograms.
     *
     * Steps:
     *   1. memset(hist, 0, NUM_BINS * sizeof(int));
     *
     *   2. For each thread i:
     *      a. pthread_join(threads[i], NULL)
     *      b. For each bin b (0 to NUM_BINS - 1):
     *         hist[b] += args[i].local_hist[b];
     */

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    return (t_end.tv_sec - t_start.tv_sec)
         + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
}

/* Helper: get wall-clock time */
double get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Verify two histograms match */
int verify(int *expected, int *actual)
{
    for (int i = 0; i < NUM_BINS; i++)
        if (expected[i] != actual[i])
            return 0;
    return 1;
}

int main(int argc, char *argv[])
{
    data_size = (argc > 1) ? atoi(argv[1]) : DEFAULT_SIZE;
    int user_threads = (argc > 2) ? atoi(argv[2]) : 4;

    printf("=== Parallel Histogram ===\n");
    printf("Data size: %d, Bins: %d\n\n", data_size, NUM_BINS);

    /* Generate random data */
    data = malloc(data_size * sizeof(int));
    srand(42);
    for (int i = 0; i < data_size; i++)
        data[i] = rand() % NUM_BINS;

    /* Sequential baseline */
    int hist_seq[NUM_BINS];
    double t0 = get_time();
    histogram_sequential(hist_seq);
    double t1 = get_time();
    double seq_time = t1 - t0;
    printf("Sequential:           %.4f sec\n\n", seq_time);

    /* Parallel with varying thread counts */
    int thread_counts[] = {1, 2, 4, 8};
    int num_tests = 4;
    int hist_par[NUM_BINS];
    int all_correct = 1;

    for (int t = 0; t < num_tests; t++) {
        int nt = thread_counts[t];
        if (nt > MAX_THREADS) continue;

        double elapsed = histogram_parallel(nt, hist_par);
        int ok = verify(hist_seq, hist_par);
        if (!ok) all_correct = 0;

        printf("Parallel (%d thread%s): %.4f sec  Speedup: %.2fx  %s\n",
               nt, nt > 1 ? "s" : " ",
               elapsed, seq_time / elapsed,
               ok ? "" : "MISMATCH!");
    }

    printf("\nVerification: %s\n", all_correct ? "ALL CORRECT" : "ERRORS FOUND");

    /* Print top 5 most frequent values */
    printf("\n--- Top 5 most frequent values ---\n");
    for (int rank = 0; rank < 5; rank++) {
        int max_val = -1, max_count = -1;
        for (int i = 0; i < NUM_BINS; i++) {
            if (hist_seq[i] > max_count) {
                /* skip already-printed values */
                int skip = 0;
                /* simple check: already used values have been zeroed */
                max_count = hist_seq[i];
                max_val = i;
            }
        }
        if (max_val >= 0) {
            printf("Value %2d: %d occurrences\n", max_val, max_count);
            hist_seq[max_val] = -1;  /* mark as printed */
        }
    }

    free(data);
    return all_correct ? 0 : 1;
}
