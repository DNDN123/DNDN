/*
 * lab4_tls.c - Thread-Local Storage (TLS) Demonstration
 *
 * [Key Concept]
 * Thread-Local Storage allows each thread to have its own copy
 * of a variable, even though the variable is declared globally.
 *
 * Without TLS: global variable is shared → race condition possible.
 * With TLS:    each thread has a private copy → no conflict.
 *
 * TLS is useful for:
 *   - Per-thread error codes (like errno)
 *   - Per-thread counters or caches
 *   - Libraries that need thread-safe global state
 *
 * Implementation:
 *   - C11: _Thread_local keyword
 *   - GCC: __thread keyword
 *   - Pthreads: pthread_key_t API
 *
 * Compile: gcc -Wall -pthread -o lab4_tls lab4_tls.c
 * Run:     ./lab4_tls
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 4
#define ITERATIONS  100000

/* --- Demo 1: Shared global vs TLS --- */

/* Regular global variable — shared by all threads */
int shared_var = 0;

/* Thread-local variable — each thread gets its own copy */
__thread int tls_var = 0;

/*
 * demo1_worker: Each thread increments both shared_var and tls_var.
 *
 * After all threads finish:
 *   - shared_var will have a race condition (unpredictable value)
 *   - tls_var in each thread will be exactly ITERATIONS
 */
void *demo1_worker(void *arg)
{
    int tid = *(int *)arg;

    /*
     * TODO: Increment both shared_var and tls_var ITERATIONS times.
     *
     * Steps:
     *   for (int i = 0; i < ITERATIONS; i++) {
     *       shared_var++;    // shared — race condition!
     *       tls_var++;       // thread-local — safe, private copy
     *   }
     *
     *   printf("[Thread %d] tls_var = %d (expected %d)\n",
     *          tid, tls_var, ITERATIONS);
     */

    return NULL;
}

void demo1_shared_vs_tls(void)
{
    printf("--- Demo 1: Shared Global vs Thread-Local ---\n\n");

    shared_var = 0;

    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, demo1_worker, &tids[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    int expected = NUM_THREADS * ITERATIONS;
    printf("\nshared_var = %d (expected %d) %s\n",
           shared_var, expected,
           shared_var == expected ? "OK" : "RACE CONDITION!");
    printf("Each thread's tls_var was %d (always correct)\n\n", ITERATIONS);
}

/* --- Demo 2: TLS for per-thread state (errno-like pattern) --- */

__thread int thread_error = 0;
__thread const char *thread_name = NULL;

void set_error(int code)
{
    thread_error = code;
}

int get_error(void)
{
    return thread_error;
}

/*
 * demo2_worker: Each thread sets its own error code via TLS.
 * Other threads' error codes are not affected.
 */
void *demo2_worker(void *arg)
{
    int tid = *(int *)arg;
    char name_buf[32];

    /*
     * TODO: Use TLS to store per-thread state.
     *
     * Steps:
     *   1. Create a thread name:
     *      sprintf(name_buf, "Worker-%d", tid);
     *      thread_name = name_buf;
     *
     *   2. Set an error code unique to this thread:
     *      set_error((tid + 1) * 100);
     *
     *   3. Sleep briefly so threads overlap:
     *      usleep(50000);
     *
     *   4. Read back and verify:
     *      printf("[%s] error = %d (expected %d)\n",
     *             thread_name, get_error(), (tid + 1) * 100);
     *
     * Key point: set_error/get_error use TLS, so each thread's
     * error code is independent — just like errno.
     */

    return NULL;
}

void demo2_per_thread_state(void)
{
    printf("--- Demo 2: Per-Thread State (errno pattern) ---\n\n");

    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, demo2_worker, &tids[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("\nEach thread maintained its own error code independently.\n\n");
}

int main(void)
{
    printf("=== Thread-Local Storage (TLS) Demo ===\n\n");

    demo1_shared_vs_tls();
    demo2_per_thread_state();

    printf("Key takeaway: TLS (__thread) gives each thread a private\n");
    printf("copy of a global variable — no locks needed.\n");
    return 0;
}
