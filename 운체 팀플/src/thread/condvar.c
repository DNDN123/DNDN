/* LINUX ONLY — futex(2) based. Not for xv6. */
#ifndef __linux__
#error "condvar.c is Linux-only. For xv6, use user/uthread.h"
#endif

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdatomic.h>
#include <limits.h>

#include "condvar.h"
#include "mutex.h"

static inline int futex_wait(atomic_int *addr, int expected)
{
    return (int)syscall(SYS_futex, addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static inline int futex_wake(atomic_int *addr, int n)
{
    return (int)syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

void condvar_init(condvar_t *cv)
{
    atomic_init(&cv->seq, 0);
}

/*
 * condvar_wait:
 *
 * The classic "snapshot before release" pattern ensures no signal is lost:
 *
 *   1. Read seq while holding the mutex (atomically consistent with
 *      any signal that might come through the mutex).
 *   2. Release the mutex.
 *   3. Call FUTEX_WAIT only if seq is still the value we snapshotted —
 *      if a signal already bumped seq, the kernel returns EAGAIN immediately
 *      (no sleep), so we re-check the predicate immediately.
 *   4. Re-acquire the mutex before returning to the caller.
 *
 * This mirrors the monitor invariant: the predicate is always checked
 * while holding the lock.
 */
void condvar_wait(condvar_t *cv, mutex_t *m)
{
    int seq = atomic_load_explicit(&cv->seq, memory_order_acquire);

    mutex_unlock(m);

    /*
     * Sleep until cv->seq != seq.  EAGAIN means the value already changed
     * (a signal arrived between our unlock and this syscall) — that's fine,
     * we just skip the sleep and re-acquire.
     */
    futex_wait(&cv->seq, seq);

    mutex_lock(m);
}

/*
 * condvar_signal: bump seq and wake at most one waiter.
 * Must be called with the associated mutex held to prevent the
 * "signal before wait" race (seq change is visible under the lock).
 */
void condvar_signal(condvar_t *cv)
{
    atomic_fetch_add_explicit(&cv->seq, 1, memory_order_release);
    futex_wake(&cv->seq, 1);
}

/*
 * condvar_broadcast: bump seq and wake all waiters.
 * All sleeping threads see seq != saved_seq and return from FUTEX_WAIT.
 */
void condvar_broadcast(condvar_t *cv)
{
    atomic_fetch_add_explicit(&cv->seq, 1, memory_order_release);
    futex_wake(&cv->seq, INT_MAX);
}

void condvar_destroy(condvar_t *cv)
{
    (void)cv;
}
