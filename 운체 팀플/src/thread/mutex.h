#ifndef MUTEX_H
#define MUTEX_H

#include <stdatomic.h>

/*
 * Futex-based mutex.
 *
 * State encoding (matches Linux kernel's mutex convention):
 *   0 = UNLOCKED
 *   1 = LOCKED, no waiters
 *   2 = LOCKED, one or more waiters sleeping in the kernel
 *
 * Fast path (uncontended) uses only an atomic CAS — no syscall.
 * Slow path calls futex(FUTEX_WAIT) to sleep in the kernel until woken.
 */
typedef struct {
    atomic_int state;
} mutex_t;

#define MUTEX_INITIALIZER { .state = ATOMIC_VAR_INIT(0) }

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
int  mutex_trylock(mutex_t *m);   /* returns 0 on success, -1 if already locked */
void mutex_unlock(mutex_t *m);
void mutex_destroy(mutex_t *m);

#endif /* MUTEX_H */
