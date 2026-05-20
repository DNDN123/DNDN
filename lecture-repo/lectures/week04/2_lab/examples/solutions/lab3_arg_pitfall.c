/*
 * lab3_arg_pitfall.c - Thread Argument Pitfall Demonstration
 *
 * [Key Concept]
 * When passing arguments to threads, a common mistake is passing
 * the address of the loop variable (&i) directly. Since all threads
 * share the same address, the value of i may change before a thread
 * reads it, causing multiple threads to receive the wrong ID.
 *
 * This program demonstrates both the BUGGY and CORRECT approaches
 * side by side so you can observe the difference.
 *
 * Compile: gcc -Wall -pthread -o lab3_arg_pitfall lab3_arg_pitfall.c
 * Run:     ./lab3_arg_pitfall
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 4

/*
 * Thread function: prints the ID it received.
 */
void *print_id(void *arg)
{
    int id = *(int *)arg;
    printf("  Thread received id = %d\n", id);
    return NULL;
}

/*
 * buggy_version: Passes &i directly to each thread.
 *
 * Problem: All threads receive a pointer to the SAME variable i.
 * By the time a thread dereferences the pointer, the loop may
 * have already incremented i.
 */
void buggy_version(void)
{
    printf("--- Buggy Version (passing &i) ---\n");

    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, print_id, &i);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}

/*
 * correct_version: Passes &tids[i] to each thread.
 *
 * Each thread gets a pointer to its own dedicated element
 * in the tids[] array, so the value won't change unexpectedly.
 */
void correct_version(void)
{
    printf("\n--- Correct Version (passing &tids[i]) ---\n");

    pthread_t threads[NUM_THREADS];
    int tids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        tids[i] = i;
        pthread_create(&threads[i], NULL, print_id, &tids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}

int main(void)
{
    printf("=== Thread Argument Pitfall Demo ===\n\n");

    buggy_version();
    correct_version();

    printf("\nNotice: In the buggy version, thread IDs may be duplicated\n");
    printf("or out of range. In the correct version, each thread gets\n");
    printf("a unique ID from 0 to %d.\n", NUM_THREADS - 1);

    return 0;
}
