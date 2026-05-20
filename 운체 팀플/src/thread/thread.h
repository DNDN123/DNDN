#ifndef THREAD_H
#define THREAD_H

#include <sys/types.h>
#include <stddef.h>

#define THREAD_STACK_SIZE (1 * 1024 * 1024)  /* 1 MB per thread stack */

typedef enum {
    THREAD_STATE_RUNNING,
    THREAD_STATE_ZOMBIE,
    THREAD_STATE_DETACHED,
} thread_state_t;

/*
 * thread_t wraps a kernel-level thread created via clone().
 * The stack is mmap'd separately so we can munmap it on join,
 * mirroring how xv6 frees a process's kernel stack after it exits.
 */
typedef struct {
    pid_t        tid;        /* kernel TID returned by clone() */
    void        *stack;      /* mmap'd stack base (low address) */
    size_t       stack_size;
    thread_state_t state;
    void        *retval;     /* value passed to thread_exit() */
} thread_t;

/*
 * Create a thread that shares the caller's virtual memory, file table,
 * and signal handlers (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND).
 * The kernel thread is born via the clone(2) syscall, not pthread_create.
 */
int  thread_create(thread_t *t, void *(*fn)(void *), void *arg);

/*
 * Block until the target thread finishes, then reclaim its stack.
 * Analogous to wait() in xv6: the parent waits for the child's exit.
 */
int  thread_join(thread_t *t, void **retval);

/* Detach so the thread cleans itself up on exit (no join needed). */
void thread_detach(thread_t *t);

/* Called from within a thread to terminate it. */
void thread_exit(void *retval) __attribute__((noreturn));

#endif /* THREAD_H */
