/*
 * lab1_deadlock_demo.c - Deadlock demonstration program
 *
 * [Overview]
 *   This program demonstrates a deadlock where two threads acquire two mutexes
 *   in opposite order. When executed, the program will hang, so you must
 *   terminate it with Ctrl+C.
 *
 * [4 Necessary Conditions for Deadlock (Coffman Conditions)]
 *   All 4 of the following conditions must hold simultaneously for deadlock:
 *   1. Mutual Exclusion:
 *      - A resource (mutex) can only be used by one thread at a time.
 *      - In this program: mutex_A and mutex_B can each be locked by only one thread.
 *   2. Hold and Wait:
 *      - A thread holds one resource while requesting another.
 *      - In this program: Thread 1 holds mutex_A and requests mutex_B,
 *        Thread 2 holds mutex_B and requests mutex_A.
 *   3. No Preemption:
 *      - A held resource cannot be forcibly taken away.
 *      - In this program: pthread_mutex_lock() is blocking, so it cannot be cancelled midway.
 *   4. Circular Wait:
 *      - Threads wait for each other's resources in a circular chain.
 *      - In this program: Thread 1 -> mutex_B (held by Thread 2) -> mutex_A (held by Thread 1)
 *        forms a circular chain.
 *
 * [Deadlock scenario in this program]
 *   Timeline:
 *     t=0ms:  Thread 1 successfully locks mutex_A
 *     t=0ms:  Thread 2 successfully locks mutex_B
 *     t=100ms: Thread 1 requests mutex_B -> held by Thread 2 -> waits!
 *     t=100ms: Thread 2 requests mutex_A -> held by Thread 1 -> waits!
 *     => Both wait forever (deadlock)
 *
 * [Compile]
 *   gcc -Wall -pthread -o lab1_deadlock_demo lab1_deadlock_demo.c
 *
 * [Run]
 *   ./lab1_deadlock_demo
 *   (If the program hangs, terminate with Ctrl+C)
 */

#include <stdio.h>      /* printf(), perror() */
#include <stdlib.h>     /* exit() */
#include <pthread.h>    /* pthread_mutex_t, pthread_create(), pthread_join(), etc. */
#include <unistd.h>     /* usleep() */

/* Statically initialize two mutexes.
 * PTHREAD_MUTEX_INITIALIZER is a macro that initializes a statically
 * allocated mutex without calling pthread_mutex_init(). */
pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: Thread 1's execution function
 *
 * Lock order: mutex_A -> mutex_B
 * Since this is the opposite order from Thread 2, circular wait can occur.
 */
void *thread1_func(void *arg)
{
    (void)arg;  /* Suppress unused parameter warning */

    /*
     * TODO: Implement Thread 1's lock acquisition pattern to cause deadlock.
     *
     * Steps:
     *   1. Lock mutex_A and print a message indicating success.
     *   2. Sleep for 100ms (usleep(100000)) to give Thread 2 time to lock mutex_B.
     *   3. Attempt to lock mutex_B (this will block forever due to deadlock).
     *   4. Print a message for the critical section (this code is unreachable).
     *   5. Unlock both mutexes in reverse order (mutex_B, then mutex_A).
     *
     * Hint: Use pthread_mutex_lock() for both locks.
     *       The key is that Thread 1 locks A first, then B,
     *       while Thread 2 (below) locks B first, then A.
     */

    return NULL;
}

/*
 * thread2_func: Thread 2's execution function
 *
 * Lock order: mutex_B -> mutex_A  (opposite of Thread 1!)
 * This reversed order creates the circular wait that causes deadlock.
 */
void *thread2_func(void *arg)
{
    (void)arg;  /* Suppress unused parameter warning */

    /*
     * TODO: Implement Thread 2's lock acquisition pattern to cause deadlock.
     *
     * Steps:
     *   1. Lock mutex_B and print a message indicating success.
     *   2. Sleep for 100ms (usleep(100000)) to give Thread 1 time to lock mutex_A.
     *   3. Attempt to lock mutex_A (this will block forever due to deadlock).
     *   4. Print a message for the critical section (this code is unreachable).
     *   5. Unlock both mutexes in reverse order (mutex_A, then mutex_B).
     *
     * Hint: The lock order here must be OPPOSITE to Thread 1.
     *       Thread 2 locks B first, then A.
     *       This creates the circular wait condition.
     */

    return NULL;
}

int main(void)
{
    pthread_t t1, t2;  /* Thread identifiers */

    printf("==============================================\n");
    printf("  Deadlock Demonstration\n");
    printf("==============================================\n");
    printf("\n");
    printf("Thread 1: locks in order mutex_A -> mutex_B\n");
    printf("Thread 2: locks in order mutex_B -> mutex_A (opposite!)\n");
    printf("\n");
    printf("If deadlock occurs, the program will hang.\n");
    printf("Terminate with Ctrl+C.\n");
    printf("\n");
    printf("----------------------------------------------\n");

    /* pthread_create(): Creates a new thread and starts execution.
     * Args: (thread ID pointer, attributes (NULL=default), function, function arg)
     * Returns 0 on success, and the new thread starts executing immediately. */
    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
        perror("pthread_create (t1)");  /* Print error message */
        exit(1);
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create (t2)");
        exit(1);
    }

    /* pthread_join(): Blocks the calling thread (main) until the specified thread terminates.
     * If deadlock occurs, neither t1 nor t2 will ever terminate,
     * so the main thread will also wait here forever. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    /* The code below will never execute if deadlock occurs */
    printf("\n----------------------------------------------\n");
    printf("Both threads terminated normally (no deadlock occurred)\n");

    /* pthread_mutex_destroy(): Releases the mutex resources.
     * It is good practice to call this before program termination. */
    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}
