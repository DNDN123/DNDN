/*
 * lab1_thread_pool.c - Simple Thread Pool Implementation
 *
 * [Key Concept]
 * A thread pool pre-creates a fixed number of worker threads that
 * wait for tasks in a shared queue. When a task is submitted, an
 * idle worker picks it up and executes it. This avoids the overhead
 * of creating/destroying threads for each task.
 *
 * Compile: gcc -Wall -pthread -o lab1_thread_pool lab1_thread_pool.c
 * Run:     ./lab1_thread_pool
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define POOL_SIZE   4
#define QUEUE_SIZE  16

struct task {
    void (*function)(int);
    int arg;
};

struct thread_pool {
    pthread_t workers[POOL_SIZE];

    struct task queue[QUEUE_SIZE];
    int head;
    int tail;
    int count;

    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int shutdown;
};

struct thread_pool pool;

void *worker_thread(void *arg)
{
    int id = *(int *)arg;
    printf("[Worker %d] started\n", id);

    while (1) {
        struct task t;

        pthread_mutex_lock(&pool.lock);

        /* Wait while queue is empty and not shutting down */
        while (pool.count == 0 && !pool.shutdown)
            pthread_cond_wait(&pool.not_empty, &pool.lock);

        if (pool.shutdown && pool.count == 0) {
            pthread_mutex_unlock(&pool.lock);
            break;
        }

        /* Dequeue a task */
        t = pool.queue[pool.head];
        pool.head = (pool.head + 1) % QUEUE_SIZE;
        pool.count--;
        pthread_cond_signal(&pool.not_full);

        pthread_mutex_unlock(&pool.lock);

        /* Execute the task outside the lock */
        printf("[Worker %d] executing task(%d)\n", id, t.arg);
        t.function(t.arg);
    }

    printf("[Worker %d] exiting\n", id);
    return NULL;
}

void pool_submit(void (*function)(int), int arg)
{
    pthread_mutex_lock(&pool.lock);

    while (pool.count == QUEUE_SIZE)
        pthread_cond_wait(&pool.not_full, &pool.lock);

    pool.queue[pool.tail].function = function;
    pool.queue[pool.tail].arg = arg;
    pool.tail = (pool.tail + 1) % QUEUE_SIZE;
    pool.count++;
    pthread_cond_signal(&pool.not_empty);

    pthread_mutex_unlock(&pool.lock);
}

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

void pool_shutdown(void)
{
    pthread_mutex_lock(&pool.lock);
    pool.shutdown = 1;
    pthread_cond_broadcast(&pool.not_empty);
    pthread_mutex_unlock(&pool.lock);

    for (int i = 0; i < POOL_SIZE; i++)
        pthread_join(pool.workers[i], NULL);

    pthread_mutex_destroy(&pool.lock);
    pthread_cond_destroy(&pool.not_empty);
    pthread_cond_destroy(&pool.not_full);
}

void example_task(int n)
{
    printf("  Task %d: computing on thread %lu\n", n, (unsigned long)pthread_self());
    usleep(100000);
}

int main(void)
{
    printf("=== Thread Pool Demo ===\n\n");

    pool_init();

    for (int i = 1; i <= 12; i++) {
        printf("[Main] submitting task %d\n", i);
        pool_submit(example_task, i);
    }

    sleep(2);
    pool_shutdown();

    printf("\nAll tasks completed. Pool shut down.\n");
    return 0;
}
