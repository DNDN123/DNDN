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
        /*
         * TODO: Implement the trylock + back-off logic for Thread 1.
         *
         * Use pthread_mutex_trylock(&mutex_B) to attempt to acquire mutex_B.
         *
         * If trylock succeeds (returns 0):
         *   1. Set success = 1
         *   2. Print a message and enter the critical section
         *   3. Simulate work with usleep(5000)
         *   4. Unlock mutex_B, then unlock mutex_A
         *
         * If trylock fails (returns non-zero / EBUSY):
         *   1. Print a back-off message
         *   2. Release mutex_A (this is the key: breaks the "Hold and Wait" condition!)
         *   3. Sleep for a random duration: usleep((unsigned)(rand() % 50000))
         *      (Random wait 0~50ms to prevent livelock)
         *
         * Hint: The structure is an if/else on the return value of trylock.
         *       The back-off (releasing mutex_A on failure) is what prevents deadlock.
         */
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
        /*
         * TODO: Implement the trylock + back-off logic for Thread 2.
         *
         * Use pthread_mutex_trylock(&mutex_A) to attempt to acquire mutex_A.
         *
         * If trylock succeeds (returns 0):
         *   1. Set success = 1
         *   2. Print a message and enter the critical section
         *   3. Simulate work with usleep(5000)
         *   4. Unlock mutex_A, then unlock mutex_B
         *
         * If trylock fails (returns non-zero / EBUSY):
         *   1. Print a back-off message
         *   2. Release mutex_B (back-off: breaks "Hold and Wait")
         *   3. Sleep for a random duration: usleep((unsigned)(rand() % 50000))
         *      (Random wait 0~50ms to prevent livelock)
         *
         * Hint: This is the mirror image of Thread 1's logic,
         *       but with mutex_B as the held lock and mutex_A as the trylock target.
         */
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
