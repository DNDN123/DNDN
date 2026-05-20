/*
 * lab1_thread_pool.c - Simple Thread Pool Implementation
 *
 * [Key Concept]
 * A thread pool pre-creates a fixed number of worker threads that
 * wait for tasks in a shared queue. When a task is submitted, an
 * idle worker picks it up and executes it. This avoids the overhead
 * of creating/destroying threads for each task.
 *
 * Benefits of thread pools:
 *   1. Faster response by reusing existing threads
 *   2. Limits the number of concurrent threads
 *   3. Separates task submission from execution
 *
 * This program implements a minimal thread pool with:
 *   - A fixed-size task queue (circular buffer)
 *   - Worker threads that dequeue and execute tasks
 *   - A mutex + condition variable for synchronization
 *
 * Compile: gcc -Wall -pthread -o lab1_thread_pool lab1_thread_pool.c
 * Run:     ./lab1_thread_pool
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define POOL_SIZE   4     /* number of worker threads */
#define QUEUE_SIZE  16    /* max tasks in queue */

/* A task is simply a function pointer + argument */
struct task {
    void (*function)(int);
    int arg;
};

/* Thread pool structure */
struct thread_pool {
    pthread_t workers[POOL_SIZE];

    struct task queue[QUEUE_SIZE];
    int head;           /* dequeue index */
    int tail;           /* enqueue index */
    int count;          /* current number of tasks */

    pthread_mutex_t lock;
    pthread_cond_t not_empty;   /* signaled when a task is added */
    pthread_cond_t not_full;    /* signaled when a task is removed */
    int shutdown;               /* 1 = pool is shutting down */
};

struct thread_pool pool;

/*
 * worker_thread: Each worker loops forever, waiting for tasks.
 *
 * Algorithm:
 *   1. Lock the mutex
 *   2. While queue is empty AND not shutting down, wait on not_empty
 *   3. If shutting down and queue is empty, unlock and exit
 *   4. Dequeue a task (head++, count--)
 *   5. Signal not_full (a slot freed up)
 *   6. Unlock the mutex
 *   7. Execute the task
 *   8. Repeat
 */
void *worker_thread(void *arg)
{
    int id = *(int *)arg;
    printf("[Worker %d] started\n", id);

    while (1) {
        struct task t;

        pthread_mutex_lock(&pool.lock);

        /*
         * TODO: Wait while the queue is empty and pool is not shutting down.
         *
         * Use a while loop (not if!) to handle spurious wakeups:
         *   while (pool.count == 0 && !pool.shutdown)
         *       pthread_cond_wait(&pool.not_empty, &pool.lock);
         */

        /* If shutting down and no more tasks, exit */
        if (pool.shutdown && pool.count == 0) {
            pthread_mutex_unlock(&pool.lock);
            break;
        }

        /*
         * TODO: Dequeue a task from the queue.
         *
         * Steps:
         *   1. Copy pool.queue[pool.head] into local variable t
         *   2. Advance head: pool.head = (pool.head + 1) % QUEUE_SIZE
         *   3. Decrement pool.count
         *   4. Signal not_full: pthread_cond_signal(&pool.not_full)
         */

        pthread_mutex_unlock(&pool.lock);

        /* Execute the task outside the lock */
        printf("[Worker %d] executing task(%d)\n", id, t.arg);
        t.function(t.arg);
    }

    printf("[Worker %d] exiting\n", id);
    return NULL;
}

/*
 * pool_submit: Add a task to the thread pool's queue.
 */
void pool_submit(void (*function)(int), int arg)
{
    pthread_mutex_lock(&pool.lock);

    /*
     * TODO: Wait while the queue is full.
     *
     *   while (pool.count == QUEUE_SIZE)
     *       pthread_cond_wait(&pool.not_full, &pool.lock);
     */

    /*
     * TODO: Enqueue the task.
     *
     * Steps:
     *   1. Set pool.queue[pool.tail].function = function
     *   2. Set pool.queue[pool.tail].arg = arg
     *   3. Advance tail: pool.tail = (pool.tail + 1) % QUEUE_SIZE
     *   4. Increment pool.count
     *   5. Signal not_empty: pthread_cond_signal(&pool.not_empty)
     */

    pthread_mutex_unlock(&pool.lock);
}

/*
 * pool_init: Initialize the thread pool.
 */
void pool_init(void)
{
    pool.head = pool.tail = pool.count = 0;
    pool.shutdown = 0;
    pthread_mutex_init(&pool.lock, NULL);
    pthread_cond_init(&pool.not_empty, NULL);
    pthread_cond_init(&pool.not_full, NULL);

    static int ids[POOL_SIZE];
    for (int i = 0; i < POOL_SIZE; i++) {
        ids[i] = i;
        pthread_create(&pool.workers[i], NULL, worker_thread, &ids[i]);
    }
}

/*
 * pool_shutdown: Signal workers to finish and join them.
 */
void pool_shutdown(void)
{
    pthread_mutex_lock(&pool.lock);
    pool.shutdown = 1;
    pthread_cond_broadcast(&pool.not_empty);  /* wake all waiting workers */
    pthread_mutex_unlock(&pool.lock);

    for (int i = 0; i < POOL_SIZE; i++)
        pthread_join(pool.workers[i], NULL);

    pthread_mutex_destroy(&pool.lock);
    pthread_cond_destroy(&pool.not_empty);
    pthread_cond_destroy(&pool.not_full);
}

/* --- Example tasks --- */

void example_task(int n)
{
    printf("  Task %d: computing on thread %lu\n", n, (unsigned long)pthread_self());
    usleep(100000);  /* simulate work (100ms) */
}

int main(void)
{
    printf("=== Thread Pool Demo ===\n\n");

    pool_init();

    /* Submit 12 tasks to a pool of 4 workers */
    for (int i = 1; i <= 12; i++) {
        printf("[Main] submitting task %d\n", i);
        pool_submit(example_task, i);
    }

    /* Wait for tasks to complete, then shut down */
    sleep(2);
    pool_shutdown();

    printf("\nAll tasks completed. Pool shut down.\n");
    return 0;
}
