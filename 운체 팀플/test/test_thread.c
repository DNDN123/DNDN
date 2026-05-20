#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "../src/thread/thread.h"
#include "../src/thread/mutex.h"
#include "../src/thread/condvar.h"
#include "../src/worker/worker_pool.h"
#include "../src/diag/diag_collector.h"

/* ── Test 1: basic thread create / join ─────────────────────────────────── */

static void *counter_fn(void *arg)
{
    int *p = (int *)arg;
    for (int i = 0; i < 100000; i++) __sync_fetch_and_add(p, 1);
    thread_exit(NULL);
}

static void test_thread_basic(void)
{
    printf("[TEST] thread create/join ... ");
    fflush(stdout);

    int shared = 0;
    thread_t t1, t2;
    assert(thread_create(&t1, counter_fn, &shared) == 0);
    assert(thread_create(&t2, counter_fn, &shared) == 0);
    thread_join(&t1, NULL);
    thread_join(&t2, NULL);

    /* Both threads share the same VM (CLONE_VM), so shared should be 200000.
     * If clone() didn't share memory they'd each operate on private copies. */
    printf("shared = %d (expected 200000) %s\n",
           shared, shared == 200000 ? "OK" : "FAIL (race w/o atomic?)");
}

/* ── Test 2: mutex mutual exclusion ─────────────────────────────────────── */

static mutex_t g_mutex = MUTEX_INITIALIZER;
static int     g_counter = 0;

static void *mutex_worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < 50000; i++) {
        mutex_lock(&g_mutex);
        g_counter++;
        mutex_unlock(&g_mutex);
    }
    thread_exit(NULL);
}

static void test_mutex(void)
{
    printf("[TEST] mutex exclusion ... ");
    fflush(stdout);

    g_counter = 0;
    thread_t t[4];
    for (int i = 0; i < 4; i++) thread_create(&t[i], mutex_worker, NULL);
    for (int i = 0; i < 4; i++) thread_join(&t[i], NULL);

    printf("counter = %d (expected 200000) %s\n",
           g_counter, g_counter == 200000 ? "OK" : "FAIL");
}

/* ── Test 3: condvar producer / consumer ────────────────────────────────── */

#define QUEUE_SIZE 8

static mutex_t   q_lock   = MUTEX_INITIALIZER;
static condvar_t q_notempty = CONDVAR_INITIALIZER;
static condvar_t q_notfull  = CONDVAR_INITIALIZER;
static int       q_buf[QUEUE_SIZE];
static int       q_head = 0, q_tail = 0, q_count = 0;
static int       producer_done = 0;

static void *producer(void *arg)
{
    int n = *(int *)arg;
    for (int i = 0; i < n; i++) {
        mutex_lock(&q_lock);
        while (q_count == QUEUE_SIZE)
            condvar_wait(&q_notfull, &q_lock);
        q_buf[q_tail] = i;
        q_tail = (q_tail + 1) % QUEUE_SIZE;
        q_count++;
        condvar_signal(&q_notempty);
        mutex_unlock(&q_lock);
    }
    mutex_lock(&q_lock);
    producer_done = 1;
    condvar_broadcast(&q_notempty);
    mutex_unlock(&q_lock);
    thread_exit(NULL);
}

static void *consumer(void *arg)
{
    int *sum = (int *)arg;
    for (;;) {
        mutex_lock(&q_lock);
        while (q_count == 0 && !producer_done)
            condvar_wait(&q_notempty, &q_lock);
        if (q_count == 0 && producer_done) {
            mutex_unlock(&q_lock);
            break;
        }
        *sum += q_buf[q_head];
        q_head = (q_head + 1) % QUEUE_SIZE;
        q_count--;
        condvar_signal(&q_notfull);
        mutex_unlock(&q_lock);
    }
    thread_exit(NULL);
}

static void test_condvar(void)
{
    printf("[TEST] condvar producer/consumer ... ");
    fflush(stdout);

    int n = 10000;
    int sum = 0;
    thread_t prod, cons;
    thread_create(&prod, producer, &n);
    thread_create(&cons, consumer, &sum);
    thread_join(&prod, NULL);
    thread_join(&cons, NULL);

    int expected = n * (n - 1) / 2;
    printf("sum = %d (expected %d) %s\n",
           sum, expected, sum == expected ? "OK" : "FAIL");
}

/* ── Test 4: worker pool + priority ordering ────────────────────────────── */

static mutex_t  order_lock = MUTEX_INITIALIZER;
static int      order_log[16];
static int      order_idx  = 0;

/*
 * gate: gatekeeper task spins here until the main thread opens the gate.
 * This queues all 6 tasks before any of them execute, making the priority
 * order deterministic without holding the pool's internal lock (which would
 * deadlock since pool_submit also acquires it).
 */
static volatile int gate_open = 0;

static void gatekeeper_task(void *arg)
{
    (void)arg;
    while (!gate_open)
        ;  /* spin until all tasks are queued */
}

static void priority_task(void *arg)
{
    int id = *(int *)arg;
    mutex_lock(&order_lock);
    order_log[order_idx++] = id;
    mutex_unlock(&order_lock);
}

static void test_worker_pool(void)
{
    printf("[TEST] worker pool priority ordering ... ");
    fflush(stdout);

    order_idx = 0;
    gate_open = 0;

    worker_pool_t pool;
    pool_init(&pool, 1);  /* single worker → dequeue order is deterministic */

    /*
     * Submit gatekeeper first (NORMAL priority).  The single worker picks it
     * up immediately but spins until gate_open=1, so all subsequent tasks
     * accumulate in the queue before any execute.
     */
    pool_submit(&pool, gatekeeper_task, NULL, PRIORITY_NORMAL);
    usleep(2000);  /* let worker start the gatekeeper before we submit rest */

    int ids[6] = {10, 20, 30, 40, 50, 60};
    pool_submit(&pool, priority_task, &ids[0], PRIORITY_LOW);
    pool_submit(&pool, priority_task, &ids[1], PRIORITY_NORMAL);
    pool_submit(&pool, priority_task, &ids[2], PRIORITY_HIGH);
    pool_submit(&pool, priority_task, &ids[3], PRIORITY_LOW);
    pool_submit(&pool, priority_task, &ids[4], PRIORITY_HIGH);
    pool_submit(&pool, priority_task, &ids[5], PRIORITY_NORMAL);

    gate_open = 1;   /* release gatekeeper → worker processes rest in order */
    pool_drain(&pool);

    printf("order: ");
    for (int i = 0; i < order_idx; i++) printf("%d ", order_log[i]);
    printf("(expected HIGH 30/50 before NORMAL 20/60 before LOW 10/40)\n");

    pool_shutdown(&pool);
}

/* ── Test 5: parallel diagnostic collection ─────────────────────────────── */

static void test_diag(void)
{
    printf("[TEST] parallel diagnostic collection ... ");
    fflush(stdout);

    worker_pool_t pool;
    pool_init(&pool, 4);

    diag_result_t result;
    int rc = diag_collect(&pool, &result, 4);

    printf("rc=%d, collected=0x%x, elapsed=%ldms\n",
           rc, result.collected, result.elapsed_ms);

    char summary[DIAG_SUMMARY_LEN];
    diag_format_summary(&result, summary, sizeof(summary));
    printf("--- Summary (first 400 chars) ---\n%.400s\n---\n", summary);

    pool_shutdown(&pool);
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== Thread / Sync / Worker Pool / Diag Tests ===\n\n");

    test_thread_basic();
    test_mutex();
    test_condvar();
    test_worker_pool();
    test_diag();

    printf("\nAll tests done.\n");
    return 0;
}
