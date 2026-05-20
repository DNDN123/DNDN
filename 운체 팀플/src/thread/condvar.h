#ifndef CONDVAR_H
#define CONDVAR_H

#include <stdatomic.h>
#include "mutex.h"

/*
 * Futex-based condition variable.
 *
 * Uses a monotonically-increasing sequence number as the futex word.
 * signal/broadcast bump the sequence; wait() records the sequence before
 * releasing the mutex, so it can detect whether a signal arrived between
 * the unlock and the FUTEX_WAIT syscall (avoiding lost wakeups).
 */
typedef struct {
    atomic_int seq;
} condvar_t;

#define CONDVAR_INITIALIZER { .seq = ATOMIC_VAR_INIT(0) }

void condvar_init(condvar_t *cv);

/*
 * Atomically release *m and sleep until signalled.
 * Reacquires *m before returning — the caller always holds *m after wait().
 */
void condvar_wait(condvar_t *cv, mutex_t *m);

/* Wake one waiter. */
void condvar_signal(condvar_t *cv);

/* Wake all waiters. */
void condvar_broadcast(condvar_t *cv);

void condvar_destroy(condvar_t *cv);

#endif /* CONDVAR_H */
