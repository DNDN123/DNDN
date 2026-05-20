/* LINUX ONLY — futex(2) based. Not for xv6. */
#ifndef __linux__
#error "mutex.c is Linux-only. For xv6, use user/uthread.h"
#endif

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdatomic.h>
#include <limits.h>
#include <string.h>

#include "mutex.h"

/* Thin wrappers around the futex(2) syscall. */
static inline int futex_wait(atomic_int *addr, int expected)
{
    return (int)syscall(SYS_futex, addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static inline int futex_wake(atomic_int *addr, int n)
{
    return (int)syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

void mutex_init(mutex_t *m)
{
    atomic_init(&m->state, 0);
}

/*
 * mutex_lock — Drepper's "futex-based locking" (from "Futexes are Tricky"):
 *
 * 1. Attempt 0→1 CAS (fast path, no syscall if uncontended).
 * 2. If already locked, set state to 2 (signal "there's a waiter") and
 *    sleep via FUTEX_WAIT until state is no longer 2.
 * 3. On wake-up, retry the 0→2 CAS (claim the lock, keep "has waiters" bit).
 *
 * Setting state=2 before sleeping is critical: the unlock path only calls
 * FUTEX_WAKE when it sees a 2, preventing a lost-wakeup race.
 */
void mutex_lock(mutex_t *m)
{
    int c = 0;

    /* Fast path: uncontended acquire. */
    if (atomic_compare_exchange_strong_explicit(
            &m->state, &c, 1,
            memory_order_acquire, memory_order_relaxed))
        return;

    /* Slow path: mark contended, then sleep. */
    do {
        /*
         * If already contended (c==2) OR we successfully mark it contended,
         * sleep.  We pass the expected value 2 so the kernel wakes us only
         * when the state transitions away from 2 (i.e., after unlock).
         */
        if (c == 2 || atomic_compare_exchange_strong_explicit(
                &m->state, &c, 2,
                memory_order_acquire, memory_order_relaxed)) {
            futex_wait(&m->state, 2);
        }
        c = 0;
    } while (!atomic_compare_exchange_strong_explicit(
                &m->state, &c, 2,
                memory_order_acquire, memory_order_relaxed));
}

int mutex_trylock(mutex_t *m)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(
            &m->state, &expected, 1,
            memory_order_acquire, memory_order_relaxed))
        return 0;
    return -1;
}

/*
 * mutex_unlock:
 *
 * Atomically subtract 1.  If the old value was 1 (no waiters), we're done.
 * If it was 2 (waiters sleeping), zero the state and wake one waiter.
 *
 * The store+wake are two separate operations, but this is safe: any new
 * locker that sneaks in between sets state back to 2 before sleeping,
 * so no wakeup is lost.
 */
void mutex_unlock(mutex_t *m)
{
    int old = atomic_fetch_sub_explicit(&m->state, 1, memory_order_release);
    if (old != 1) {
        /* There were waiters — zero the lock and wake one. */
        atomic_store_explicit(&m->state, 0, memory_order_release);
        futex_wake(&m->state, 1);
    }
}

void mutex_destroy(mutex_t *m)
{
    (void)m;  /* nothing to free; state is embedded */
}
