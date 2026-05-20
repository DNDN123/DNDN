/*
 * lab2_deadlock_fix_ordering.c - Resolving Deadlock with Lock Ordering
 *
 * [Overview]
 *   This program prevents deadlock by having all threads acquire locks in the
 *   same order (mutex_A -> mutex_B).
 *   This is the first method to resolve the deadlock from lab1_deadlock_demo.c.
 *
 * [Which Coffman condition does this break?]
 *   It breaks condition 4: "Circular Wait".
 *   If all threads always acquire locks in the same order,
 *   a circular wait graph cannot form.
 *
 *   Proof (by contradiction):
 *     - Define a global order: mutex_A < mutex_B.
 *     - All threads must acquire the lower-order lock first.
 *     - For circular wait to occur, Thread X must hold mutex_A and wait for mutex_B
 *       while Thread Y holds mutex_B and waits for mutex_A.
 *     - However, for Thread Y to hold mutex_B, it must first acquire mutex_A.
 *     - Since Thread X already holds mutex_A, Thread Y must already be waiting
 *       at the mutex_A step and cannot hold mutex_B. Contradiction!
 *     - Therefore, circular wait cannot occur.
 *
 * [Lock ordering example in the xv6 kernel]
 *   The xv6 kernel uses the same strategy:
 *   - wait_lock must always be acquired before p->lock.
 *   - In the file system: inode lock -> buffer lock order is always followed.
 *   Violating this order can cause deadlock within the kernel as well.
 *
 * [Compile]
 *   gcc -Wall -pthread -o lab2_deadlock_fix_ordering lab2_deadlock_fix_ordering.c
 *
 * [Run]
 *   ./lab2_deadlock_fix_ordering
 */

#include <stdio.h>      /* printf(), perror() */
#include <stdlib.h>     /* exit() */
#include <pthread.h>    /* pthread_mutex_t, pthread_create(), pthread_join(), etc. */
#include <unistd.h>     /* usleep() */

/* Statically initialize two mutexes.
 * PTHREAD_MUTEX_INITIALIZER: A macro that initializes a static mutex without pthread_mutex_init().
 *
 * Lock ordering rule: Always lock mutex_A first, then mutex_B.
 * All threads must follow this order to prevent deadlock. */
pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: Thread 1's execution function
 *
 * Lock order: mutex_A -> mutex_B (follows the global ordering rule)
 * Repeats 5 times to verify that no deadlock occurs.
 */
void *thread1_func(void *arg)
{
    (void)arg;  /* Suppress unused parameter warning */

    for (int i = 0; i < 5; i++) {
        printf("[Thread 1] Iteration %d: Attempting to lock mutex_A...\n", i + 1);
        /*
         * TODO: Acquire both locks following the global ordering rule (mutex_A -> mutex_B).
         *
         * Steps:
         *   1. Lock mutex_A (the lower-order lock first).
         *   2. Print that mutex_A was locked.
         *   3. Sleep for 10ms (usleep(10000)) to induce race conditions.
         *   4. Lock mutex_B (the higher-order lock second).
         *   5. Print that mutex_B was locked.
         *
         * Hint: Both Thread 1 and Thread 2 must use the SAME lock order
         *       (mutex_A first, mutex_B second) to prevent circular wait.
         */

        /* Critical section: Perform work while holding both locks */
        printf("[Thread 1] Iteration %d: === Entered critical section (holding A and B) ===\n", i + 1);
        usleep(5000);  /* Simulate work for 5ms */

        /*
         * TODO: Release both locks in reverse order of acquisition (mutex_B, then mutex_A).
         *
         * Hint: Releasing in reverse order (LIFO, like a stack) is common practice.
         */

        printf("\n");
    }

    return NULL;
}

/*
 * thread2_func: Thread 2's execution function
 *
 * Lock order: mutex_A -> mutex_B (same order as Thread 1!)
 *
 * Key point: In lab1, the order was mutex_B -> mutex_A,
 *            but following the lock ordering rule, it was changed to mutex_A -> mutex_B.
 *            This single change completely eliminates deadlock.
 */
void *thread2_func(void *arg)
{
    (void)arg;  /* Suppress unused parameter warning */

    for (int i = 0; i < 5; i++) {
        /*
         * [Before fix - deadlock occurs (lab1 code)]
         *   pthread_mutex_lock(&mutex_B);  // Lock B first
         *   pthread_mutex_lock(&mutex_A);  // Lock A second
         *
         * [After fix - lock ordering applied]
         *   pthread_mutex_lock(&mutex_A);  // Lock A first (unified order!)
         *   pthread_mutex_lock(&mutex_B);  // Lock B second
         */
        printf("[Thread 2] Iteration %d: Attempting to lock mutex_A...\n", i + 1);
        /*
         * TODO: Acquire both locks following the SAME global ordering rule as Thread 1.
         *
         * Steps:
         *   1. Lock mutex_A first (NOT mutex_B! This is the fix compared to lab1).
         *   2. Print that mutex_A was locked.
         *   3. Sleep for 10ms (usleep(10000)) to induce race conditions.
         *   4. Lock mutex_B second.
         *   5. Print that mutex_B was locked.
         *
         * Hint: The crucial difference from lab1 is that Thread 2 now acquires
         *       mutex_A before mutex_B, matching Thread 1's order.
         *       This breaks the circular wait condition.
         */

        /* Critical section: Perform work while holding both locks */
        printf("[Thread 2] Iteration %d: === Entered critical section (holding A and B) ===\n", i + 1);
        usleep(5000);  /* Simulate work for 5ms */

        /*
         * TODO: Release both locks in reverse order of acquisition (mutex_B, then mutex_A).
         */

        printf("\n");
    }

    return NULL;
}

int main(void)
{
    pthread_t t1, t2;  /* Thread identifiers */

    printf("==============================================\n");
    printf("  Resolving Deadlock with Lock Ordering Demo\n");
    printf("==============================================\n");
    printf("\n");
    printf("Solution: All threads acquire locks in the same order\n");
    printf("  Lock ordering rule: Always mutex_A -> mutex_B\n");
    printf("\n");
    printf("This is the same approach used in the xv6 kernel.\n");
    printf("Example: wait_lock is always acquired before p->lock\n");
    printf("\n");
    printf("----------------------------------------------\n\n");

    /* pthread_create(): Creates a new thread and executes the specified function.
     * Args: (thread ID pointer, attributes (NULL=default), function, function arg)
     * Both threads acquire locks in the same order (A -> B), so no deadlock */
    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
        perror("pthread_create (t1)");  /* Print system error message */
        exit(1);
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create (t2)");
        exit(1);
    }

    /* pthread_join(): Main thread waits until each thread terminates.
     * Thanks to lock ordering, both threads terminate normally without deadlock. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("----------------------------------------------\n");
    printf("Both threads terminated normally! No deadlock occurred.\n");
    printf("\n");
    printf("[Key Principle]\n");
    printf("  If all threads acquire locks in the same order (A -> B),\n");
    printf("  the circular wait condition is broken and deadlock cannot occur.\n");

    /* pthread_mutex_destroy(): Releases resources used by the mutex.
     * Even for statically initialized (PTHREAD_MUTEX_INITIALIZER) mutexes,
     * it is good practice to call this before program termination. */
    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}
