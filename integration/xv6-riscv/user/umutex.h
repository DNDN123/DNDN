#ifndef UMUTEX_H
#define UMUTEX_H

#include "kernel/types.h"
#include "user/user.h"

// ── mutex ────────────────────────────────────────────────────────────────────
//
// State: 0 = unlocked, 1 = locked
//
// Fast path (uncontended): CAS 0→1 succeeds immediately, no syscall.
// Slow path (contended): futex_wait sleeps in the kernel until the holder
//   calls futex_wake, avoiding busy-waiting.
//
// This mirrors the two-level design of Linux's futex-based mutex.

typedef struct {
  volatile int state;   // 0 = unlocked, 1 = locked
} mutex_t;

#define MUTEX_INIT { 0 }

static inline void
mutex_init(mutex_t *m)
{
  m->state = 0;
}

static inline void
mutex_lock(mutex_t *m)
{
  while(__sync_val_compare_and_swap(&m->state, 0, 1) != 0)
    futex_wait(&m->state, 1);  // sleep while still locked
}

static inline int
mutex_trylock(mutex_t *m)
{
  return __sync_val_compare_and_swap(&m->state, 0, 1) == 0 ? 0 : -1;
}

static inline void
mutex_unlock(mutex_t *m)
{
  __sync_lock_release(&m->state);  // atomically write 0
  futex_wake(&m->state);           // wake one (or more) waiters
}

// ── condvar ──────────────────────────────────────────────────────────────────
//
// Uses a monotonically-increasing sequence number as the futex word.
// signal/broadcast bump the sequence; wait() records the sequence before
// releasing the mutex, then calls futex_wait with the old sequence.
// If a signal arrived between unlock and sleep, the sequence will have
// changed and futex_wait returns immediately — no lost wakeup.

typedef struct {
  volatile int seq;
} condvar_t;

#define CONDVAR_INIT { 0 }

static inline void
condvar_init(condvar_t *cv)
{
  cv->seq = 0;
}

// Atomically release *m and sleep until signalled. Reacquires *m before return.
static inline void
condvar_wait(condvar_t *cv, mutex_t *m)
{
  int seq = cv->seq;       // snapshot before releasing the mutex
  mutex_unlock(m);
  futex_wait(&cv->seq, seq);  // returns immediately if seq already changed
  mutex_lock(m);
}

// Wake one waiter.
static inline void
condvar_signal(condvar_t *cv)
{
  __sync_fetch_and_add(&cv->seq, 1);
  futex_wake(&cv->seq);
}

// Wake all waiters.
static inline void
condvar_broadcast(condvar_t *cv)
{
  __sync_fetch_and_add(&cv->seq, 1);
  futex_wake(&cv->seq);
}

#endif // UMUTEX_H
