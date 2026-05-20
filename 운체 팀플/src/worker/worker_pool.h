#ifndef WORKER_POOL_H
#define WORKER_POOL_H

#include "../thread/thread.h"
#include "../thread/mutex.h"
#include "../thread/condvar.h"

#define POOL_MAX_WORKERS  16
#define POOL_MAX_TASKS   256

/*
 * Three-level priority matching the shell's scheduling model:
 *   HIGH   — interactive commands (user is waiting)
 *   NORMAL — background tasks
 *   LOW    — periodic diagnostics / prefetch hints
 */
typedef enum {
    PRIORITY_LOW    = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_HIGH   = 2,
} task_priority_t;

typedef struct {
    void              (*fn)(void *arg);
    void               *arg;
    task_priority_t     priority;
} pool_task_t;

/*
 * worker_pool_t — a fixed set of worker threads draining a shared task queue.
 *
 * OS concepts demonstrated:
 *   - Thread lifecycle: workers created with thread_create(), joined on shutdown
 *   - Synchronization: mutex + condvar protect the task queue
 *   - Scheduling hint: high-priority tasks are dequeued first (priority ordering
 *     inside the queue mirrors a simplified multi-level feedback queue)
 */
typedef struct {
    thread_t   workers[POOL_MAX_WORKERS];
    int        num_workers;

    pool_task_t queue[POOL_MAX_TASKS];
    int         queue_len;

    mutex_t    lock;
    condvar_t  task_ready;   /* workers wait here when queue is empty */
    condvar_t  all_idle;     /* pool_drain() waits here */

    int        active;       /* tasks currently executing */
    int        shutdown;     /* set to 1 to stop workers */
} worker_pool_t;

/* Initialise pool and spawn `num_workers` threads. */
int  pool_init(worker_pool_t *p, int num_workers);

/*
 * Submit a task.  Returns 0 on success, -1 if queue is full.
 * High-priority tasks are inserted ahead of lower-priority ones.
 */
int  pool_submit(worker_pool_t *p, void (*fn)(void *), void *arg,
                 task_priority_t priority);

/* Block until all submitted tasks have finished. */
void pool_drain(worker_pool_t *p);

/* Signal workers to stop, then join all threads. */
void pool_shutdown(worker_pool_t *p);

#endif /* WORKER_POOL_H */
