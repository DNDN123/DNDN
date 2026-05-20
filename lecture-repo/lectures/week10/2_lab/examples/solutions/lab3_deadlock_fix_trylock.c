/*
 * lab3_deadlock_fix_trylock.c - Avoiding Deadlock with pthread_mutex_trylock
 *
 * [Overview]
 *   This program avoids deadlock using a trylock + back-off strategy.
 *   If lock acquisition fails, all currently held locks are released (back-off)
 *   and the thread retries after a short delay.
 *   This is the second method to resolve the deadlock from lab1_deadlock_demo.c.
 *
 * [Which Coffman condition does this break?]
 *   It breaks condition 2: "Hold and Wait".
 *   - A regular pthread_mutex_lock() blocks while holding a lock
 *     and waiting for another (Hold and Wait holds).
 *   - trylock returns failure immediately if the lock cannot be acquired.
 *   - On failure, all held locks are released, so the situation of
 *     "waiting while holding a resource" never occurs.
 *
 * [trylock vs lock comparison]
 *   pthread_mutex_lock():
 *     - Blocks until the lock can be acquired (thread is suspended)
 *     - Always succeeds, but risks deadlock
 *   pthread_mutex_trylock():
 *     - Tries to acquire the lock immediately; returns EBUSY on failure (non-blocking)
 *     - May fail, but can avoid deadlock
 *     - Return value: 0 on success, EBUSY if another thread already holds it
 *
 * [Back-off strategy]
 *   1. Acquire the first lock with a regular lock().
 *   2. Try the second lock with trylock().
 *   3. If trylock fails:
 *      a. Release the first lock that is currently held (back-off)
 *      b. Wait for a random duration (to reduce contention)
 *      c. Retry from the beginning
 *
 * [Livelock warning]
 *   A drawback of the trylock + back-off approach: livelock can occur.
 *   Livelock: Two threads repeatedly acquire and release locks
 *   without making any actual progress.
 *   Using random wait times greatly reduces the probability of livelock.
 *
 * [Compile]
 *   gcc -Wall -pthread -o lab3_deadlock_fix_trylock lab3_deadlock_fix_trylock.c
 *
 * [Run]
 *   ./lab3_deadlock_fix_trylock
 */

#include <stdio.h>      /* printf(), perror() */
#include <stdlib.h>     /* exit(), srand(), rand() */
#include <pthread.h>    /* pthread_mutex_t, pthread_mutex_trylock(), pthread_create(), etc. */
#include <unistd.h>     /* usleep() */

/* Statically initialize two mutexes.
 * These two mutexes are acquired in opposite order,
 * but the trylock + back-off strategy avoids deadlock. */
pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread1_func: Thread 1's execution function
 *
 * Lock order: mutex_A (lock) -> mutex_B (trylock)
 * If trylock fails, releases mutex_A and retries (back-off).
 */
void *thread1_func(void *arg)
{
    (void)arg;  /* Suppress unused parameter warning */
    int success = 0;   /* Whether both locks were successfully acquired */
    int attempts = 0;  /* Attempt counter */

    /* Back-off loop: Retry until both locks are acquired */
    while (!success) {
        attempts++;

        printf("[Thread 1] Attempt %d: Locking mutex_A...\n", attempts);
        /* pthread_mutex_lock(): Acquires the first lock with a regular blocking lock.
         * If another thread holds it, waits until released. */
        pthread_mutex_lock(&mutex_A);
        printf("[Thread 1] Attempt %d: mutex_A locked\n", attempts);

        /* usleep(): Wait 10ms to give Thread 2 a chance to grab mutex_B.
         * This allows us to observe trylock failures (contention). */
        usleep(10000);  /* 10ms */

        printf("[Thread 1] Attempt %d: Trying trylock on mutex_B...\n", attempts);
        /* pthread_mutex_trylock(): Tries the second lock in non-blocking mode.
         * Return value 0: Lock acquired successfully
         * Return value EBUSY: Another thread already holds it (failure)
         * Unlike regular lock(), it does not block, so deadlock cannot occur. */
        if (pthread_mutex_trylock(&mutex_B) == 0) {
            /* trylock succeeded: Both locks have been acquired */
            printf("[Thread 1] Attempt %d: mutex_B trylock succeeded!\n", attempts);
            success = 1;

            /* Critical section: Perform work while holding both locks */
            printf("[Thread 1] === Entered critical section (holding A and B) ===\n");
            usleep(5000);  /* Simulate work for 5ms */

            /* pthread_mutex_unlock(): Releases the mutex.
             * If any threads are waiting, one of them will be woken up. */
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 1] mutex_B released\n");
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 1] mutex_A released\n");
        } else {
            /* trylock failed: Perform back-off
             * Key point: Immediately release the held mutex_A.
             * This breaks the "Hold and Wait" condition.
             * Thread 2 can now acquire mutex_A and make progress. */
            printf("[Thread 1] Attempt %d: mutex_B trylock failed! -> back-off\n", attempts);
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 1] Attempt %d: mutex_A released (back-off)\n", attempts);

            /* usleep(): Wait a random time (0~50ms) before retrying.
             * Why random wait is important:
             *   - With a fixed wait time, both threads may wake up simultaneously
             *     and try to acquire locks in the same order again, causing livelock.
             *   - A random wait time staggers the execution timing of the two threads,
             *     increasing the probability that one thread acquires both locks first.
             * This concept is similar to the exponential back-off in
             * the Ethernet CSMA/CD protocol. */
            usleep((unsigned)(rand() % 50000));  /* Random wait 0~50ms */
        }
    }

    printf("[Thread 1] Done (total %d attempts)\n\n", attempts);
    return NULL;
}

/*
 * thread2_func: Thread 2's execution function
 *
 * Lock order: mutex_B (lock) -> mutex_A (trylock)
 * Opposite order from Thread 1, but trylock + back-off avoids deadlock.
 * If trylock fails, releases mutex_B and retries (back-off).
 */
void *thread2_func(void *arg)
{
    (void)arg;  /* Suppress unused parameter warning */
    int success = 0;   /* Whether both locks were successfully acquired */
    int attempts = 0;  /* Attempt counter */

    /* Back-off loop: Retry until both locks are acquired */
    while (!success) {
        attempts++;

        printf("[Thread 2] Attempt %d: Locking mutex_B...\n", attempts);
        /* pthread_mutex_lock(): Acquires the first lock (mutex_B) with blocking.
         * Although the order is opposite from Thread 1, since the second lock
         * is attempted with trylock, deadlock cannot occur despite the different order. */
        pthread_mutex_lock(&mutex_B);
        printf("[Thread 2] Attempt %d: mutex_B locked\n", attempts);

        usleep(10000);  /* 10ms wait - induce contention */

        printf("[Thread 2] Attempt %d: Trying trylock on mutex_A...\n", attempts);
        /* pthread_mutex_trylock(): Tries mutex_A in non-blocking mode.
         * If Thread 1 already holds mutex_A, returns EBUSY immediately.
         * Since it does not block, a deadlock chain cannot form. */
        if (pthread_mutex_trylock(&mutex_A) == 0) {
            /* trylock succeeded: Both locks have been acquired */
            printf("[Thread 2] Attempt %d: mutex_A trylock succeeded!\n", attempts);
            success = 1;

            /* Critical section */
            printf("[Thread 2] === Entered critical section (holding A and B) ===\n");
            usleep(5000);  /* Simulate work for 5ms */

            /* Release mutexes */
            pthread_mutex_unlock(&mutex_A);
            printf("[Thread 2] mutex_A released\n");
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 2] mutex_B released\n");
        } else {
            /* trylock failed: Perform back-off
             * Release the held mutex_B to break the Hold and Wait condition.
             * Thread 1 can now acquire mutex_B. */
            printf("[Thread 2] Attempt %d: mutex_A trylock failed! -> back-off\n", attempts);
            pthread_mutex_unlock(&mutex_B);
            printf("[Thread 2] Attempt %d: mutex_B released (back-off)\n", attempts);

            /* Random wait (0~50ms) before retrying - prevents livelock */
            usleep((unsigned)(rand() % 50000));  /* Random wait 0~50ms */
        }
    }

    printf("[Thread 2] Done (total %d attempts)\n\n", attempts);
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;  /* Thread identifiers */

    /* srand(): Sets the seed for the random number generator.
     * Used to randomize the back-off wait times.
     * A fixed seed (42) is used to make execution results reproducible.
     * In real systems, time(NULL) or similar is used as the seed. */
    srand(42);

    printf("==============================================\n");
    printf("  Avoiding Deadlock with trylock + Back-off Demo\n");
    printf("==============================================\n");
    printf("\n");
    printf("Thread 1: mutex_A -> trylock(mutex_B)\n");
    printf("Thread 2: mutex_B -> trylock(mutex_A)\n");
    printf("\n");
    printf("The two threads try to acquire locks in opposite order,\n");
    printf("but if trylock fails, the held lock is released\n");
    printf("and retried after a short delay (back-off strategy).\n");
    printf("\n");
    printf("----------------------------------------------\n\n");

    /* pthread_create(): Creates two threads to run concurrently.
     * The two threads try to acquire locks in opposite order,
     * but thanks to trylock + back-off, both complete without deadlock. */
    if (pthread_create(&t1, NULL, thread1_func, NULL) != 0) {
        perror("pthread_create (t1)");  /* Print system error message */
        exit(1);
    }
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create (t2)");
        exit(1);
    }

    /* pthread_join(): Main thread waits until each thread terminates.
     * Thanks to the trylock + back-off strategy, both threads terminate normally. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("----------------------------------------------\n");
    printf("Both threads terminated normally! No deadlock occurred.\n");
    printf("\n");
    printf("[Key Principle]\n");
    printf("  trylock returns failure immediately instead of blocking when it cannot acquire a lock.\n");
    printf("  Releasing held locks on failure (back-off)\n");
    printf("  breaks the Hold & Wait condition and prevents deadlock.\n");
    printf("\n");
    printf("[Caution]\n");
    printf("  The trylock + back-off approach can cause livelock (endless retries),\n");
    printf("  so it is important to use random wait times.\n");

    /* pthread_mutex_destroy(): Releases the mutex resources.
     * Called before program termination to clean up resources. */
    pthread_mutex_destroy(&mutex_A);
    pthread_mutex_destroy(&mutex_B);

    return 0;
}
