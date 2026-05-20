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

    printf("[Thread 1] Attempting to lock mutex_A...\n");
    /* pthread_mutex_lock(): Locks the mutex.
     * If another thread already holds the lock, it blocks until released.
     * Returns 0 on success. */
    pthread_mutex_lock(&mutex_A);
    printf("[Thread 1] mutex_A locked successfully!\n");

    /* usleep(): Suspends the current thread for the specified microseconds.
     * Here we intentionally wait 100ms to give Thread 2 time to lock mutex_B
     * first. This makes deadlock almost certain to occur. */
    printf("[Thread 1] Waiting 0.1s (to let the other thread grab mutex_B)...\n");
    usleep(100000);  /* 100ms = 100,000 microseconds */

    /* At this point, Thread 2 has already locked mutex_B.
     * Thread 1 holds mutex_A and requests mutex_B.
     * Thread 2 holds mutex_B and is requesting mutex_A.
     * -> Circular Wait is established -> Deadlock occurs!
     *
     * This pthread_mutex_lock() call will never return. */
    printf("[Thread 1] Attempting to lock mutex_B... (may hang here)\n");
    pthread_mutex_lock(&mutex_B);
    printf("[Thread 1] mutex_B locked successfully!\n");  /* This line will never be printed */

    /* Critical section - unreachable due to deadlock */
    printf("[Thread 1] Both locks acquired - performing work\n");

    /* pthread_mutex_unlock(): Releases the mutex.
     * If any threads are waiting, one of them will be woken up. */
    pthread_mutex_unlock(&mutex_B);
    pthread_mutex_unlock(&mutex_A);
    printf("[Thread 1] Done\n");

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

    printf("[Thread 2] Attempting to lock mutex_B...\n");
    /* pthread_mutex_lock(): Locks mutex_B.
     * Since Thread 1 has not yet locked mutex_B, this succeeds immediately. */
    pthread_mutex_lock(&mutex_B);
    printf("[Thread 2] mutex_B locked successfully!\n");

    /* Wait 100ms to give Thread 1 time to lock mutex_A */
    printf("[Thread 2] Waiting 0.1s (to let the other thread grab mutex_A)...\n");
    usleep(100000);  /* 100ms */

    /* At this point, Thread 1 has already locked mutex_A.
     * Hold and Wait: Thread 2 holds mutex_B and requests mutex_A.
     * Thread 1 holds mutex_A and is requesting mutex_B.
     * -> Both threads wait for each other's resources forever (deadlock) */
    printf("[Thread 2] Attempting to lock mutex_A... (may hang here)\n");
    pthread_mutex_lock(&mutex_A);
    printf("[Thread 2] mutex_A locked successfully!\n");  /* This line will never be printed */

    /* Critical section - unreachable due to deadlock */
    printf("[Thread 2] Both locks acquired - performing work\n");

    pthread_mutex_unlock(&mutex_A);
    pthread_mutex_unlock(&mutex_B);
    printf("[Thread 2] Done\n");

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
