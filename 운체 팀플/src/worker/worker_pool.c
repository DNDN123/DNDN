/* LINUX ONLY — depends on Linux thread library. Not for xv6. */
#ifndef __linux__
#error "worker_pool.c is Linux-only. For xv6, use xv6 worker pool."
#endif

#include <string.h>
#include <stdlib.h>
#include "worker_pool.h"

/* ── internal helpers ──────────────────────────────────────────────────── */

/*
 * Find the insertion index for a new task so the queue stays sorted
 * descending by priority (highest at index 0).
 * This is O(n) but n ≤ POOL_MAX_TASKS and simplicity beats complexity here.
 */
static int priority_insert_index(worker_pool_t *p, task_priority_t pri)
{
    for (int i = 0; i < p->queue_len; i++) {
        if (p->queue[i].priority < pri)
            return i;
    }
    return p->queue_len;
}

/* Dequeue and return the front task (highest priority). Caller holds lock. */
static pool_task_t dequeue(worker_pool_t *p)
{
    pool_task_t t = p->queue[0];
    memmove(&p->queue[0], &p->queue[1],
            sizeof(pool_task_t) * (size_t)(p->queue_len - 1));
    p->queue_len--;
    return t;
}

/* ── worker thread body ─────────────────────────────────────────────────── */

static void *worker_body(void *arg)
{
    worker_pool_t *p = (worker_pool_t *)arg;

    for (;;) {
        mutex_lock(&p->lock);

        /*
         * Wait while the queue is empty and we haven't been asked to stop.
         * condvar_wait() atomically releases the lock and sleeps, so no
         * task submitted between our queue-empty check and the sleep is lost.
         */
        while (p->queue_len == 0 && !p->shutdown)
            condvar_wait(&p->task_ready, &p->lock);

        if (p->shutdown && p->queue_len == 0) {
            mutex_unlock(&p->lock);
            break;
        }

        pool_task_t task = dequeue(p);
        p->active++;
        mutex_unlock(&p->lock);

        /* Execute outside the lock so other workers can run concurrently. */
        task.fn(task.arg);

        mutex_lock(&p->lock);
        p->active--;
        /*
         * If nobody is left working and queue is empty, wake pool_drain().
         * Broadcasting is safe — pool_drain() re-checks the predicate.
         */
        if (p->active == 0 && p->queue_len == 0)
            condvar_broadcast(&p->all_idle);
        mutex_unlock(&p->lock);
    }

    thread_exit(NULL);
}

/* ── public API ─────────────────────────────────────────────────────────── */

int pool_init(worker_pool_t *p, int num_workers)
{
    if (num_workers < 1 || num_workers > POOL_MAX_WORKERS)
        return -1;

    memset(p, 0, sizeof(*p));
    p->num_workers = num_workers;
    p->shutdown    = 0;

    mutex_init(&p->lock);
    condvar_init(&p->task_ready);
    condvar_init(&p->all_idle);

    for (int i = 0; i < num_workers; i++) {
        if (thread_create(&p->workers[i], worker_body, p) != 0)
            return -1;
    }
    return 0;
}

int pool_submit(worker_pool_t *p, void (*fn)(void *), void *arg,
                task_priority_t priority)
{
    mutex_lock(&p->lock);

    if (p->queue_len >= POOL_MAX_TASKS || p->shutdown) {
        mutex_unlock(&p->lock);
        return -1;
    }

    int idx = priority_insert_index(p, priority);
    /* Shift lower-priority tasks right to make room. */
    memmove(&p->queue[idx + 1], &p->queue[idx],
            sizeof(pool_task_t) * (size_t)(p->queue_len - idx));

    p->queue[idx] = (pool_task_t){ .fn = fn, .arg = arg, .priority = priority };
    p->queue_len++;

    /*
     * Wake exactly one worker.  If all workers are busy the wakeup is queued
     * in the futex so the next worker to finish will pick this task up without
     * a second signal.
     */
    condvar_signal(&p->task_ready);
    mutex_unlock(&p->lock);
    return 0;
}

void pool_drain(worker_pool_t *p)
{
    mutex_lock(&p->lock);
    while (p->queue_len > 0 || p->active > 0)
        condvar_wait(&p->all_idle, &p->lock);
    mutex_unlock(&p->lock);
}

void pool_shutdown(worker_pool_t *p)
{
    mutex_lock(&p->lock);
    p->shutdown = 1;
    condvar_broadcast(&p->task_ready);  /* wake all workers so they can exit */
    mutex_unlock(&p->lock);

    for (int i = 0; i < p->num_workers; i++)
        thread_join(&p->workers[i], NULL);

    mutex_destroy(&p->lock);
    condvar_destroy(&p->task_ready);
    condvar_destroy(&p->all_idle);
}
