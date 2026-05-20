/* ============================================================
 * LINUX ONLY — uses clone(2), mmap(2), futex(2).
 * Not compiled for xv6. See xv6-riscv/.../user/uthread.h
 * ============================================================ */
#ifndef __linux__
#error "thread.c is Linux-only. For xv6, use user/uthread.h"
#endif

#include <sched.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "thread.h"

/*
 * clone() flags: the new thread shares the caller's VM, filesystem context,
 * open file table, and signal dispositions — just like POSIX threads —
 * but is created directly through the kernel interface, not libc.
 *
 * SIGCHLD makes waitpid() work for joining (the thread sends SIGCHLD on exit).
 */
#define CLONE_FLAGS \
    (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD)

/*
 * thread_arg_t is embedded at the TOP of the child's own mmap'd stack.
 * Because the stack grows downward, it sits above the initial SP and is
 * never overwritten by the thread's own function calls.  This avoids the
 * CLONE_VM race where the parent's stack frame (containing a local targ)
 * could be reused by the next thread_create call before the child copies it.
 */
typedef struct {
    void *(*fn)(void *);
    void *arg;
    thread_t *thread;
} thread_arg_t;

static int thread_trampoline(void *raw_arg)
{
    thread_arg_t *targ = (thread_arg_t *)raw_arg;
    void *(*fn)(void *) = targ->fn;
    void *arg           = targ->arg;
    /* targ lives above our SP in the mmap region — no copy needed. */
    void *ret = fn(arg);
    thread_exit(ret);
}

int thread_create(thread_t *t, void *(*fn)(void *), void *arg)
{
    /* Allocate stack with mmap — anonymous private mapping, stack-grows-down. */
    void *stack = mmap(NULL, THREAD_STACK_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                       -1, 0);
    if (stack == MAP_FAILED)
        return -1;

    t->stack      = stack;
    t->stack_size = THREAD_STACK_SIZE;
    t->state      = THREAD_STATE_RUNNING;
    t->retval     = NULL;

    /*
     * Place targ just below the top of the mmap region.
     * Initial SP is set to just below targ, 16-byte aligned (x86-64 ABI).
     */
    void *stack_end   = (char *)stack + THREAD_STACK_SIZE;
    thread_arg_t *targ = (thread_arg_t *)stack_end - 1;
    targ->fn     = fn;
    targ->arg    = arg;
    targ->thread = t;

    void *stack_top = (void *)((uintptr_t)targ & ~15UL);

    pid_t tid = clone(thread_trampoline, stack_top, CLONE_FLAGS, targ);
    if (tid < 0) {
        munmap(stack, THREAD_STACK_SIZE);
        return -1;
    }

    t->tid = tid;
    return 0;
}

int thread_join(thread_t *t, void **retval)
{
    if (t->state == THREAD_STATE_DETACHED)
        return -1;

    /* waitpid() blocks until the cloned task exits (it sends SIGCHLD). */
    int status;
    if (waitpid(t->tid, &status, 0) < 0)
        return -1;

    t->state = THREAD_STATE_ZOMBIE;

    if (retval)
        *retval = t->retval;

    /* Free the thread stack — analogous to xv6 freeing kstack after wait(). */
    munmap(t->stack, t->stack_size);
    t->stack = NULL;
    return 0;
}

void thread_detach(thread_t *t)
{
    t->state = THREAD_STATE_DETACHED;
    /* Detached threads free their own stack; handled in thread_exit. */
}

void thread_exit(void *retval)
{
    /* The running thread cannot access its own thread_t here safely
     * (it may have been freed by a racing join).  We just call _exit()
     * so the kernel cleans up the task; the joiner's waitpid() unblocks. */
    (void)retval;
    _exit(0);
}
