// thread_race.c — exercise the path K5 fix protects.
//
// Scenario: main spawns N threads, sleeps briefly, then exits WITHOUT
// thread_join(). On exit the parent shell reaps main via wait() →
// freeproc(main). At that moment the threads may still be alive.
//
// Without the K5 fix: freeproc(main) calls proc_freepagetable which
// kfree()'s the shared physical pages. Live threads then page-fault or
// see scrambled memory — kernel may panic, threadtest after this would
// produce garbage. With K5: freeproc detects live threads of the same
// tgroup, downgrades to thread_freepagetable (unmap only), and the threads
// continue safely until they exit on their own.
//
// Success = process exits cleanly AND a subsequent kernel operation (the
// next shell command) does not panic. We don't try to PROVOKE a panic
// (which would require the absence of the fix); we just exercise the path
// and let sanity_check.sh / the integrator confirm the kernel is healthy
// afterward.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define STACK_SIZE 4096
#define NTHREADS   4

static void
worker(void *arg)
{
  int id = (int)(uint64)arg;
  // Tiny busy loop — long enough to outlive main's exit on at least one CPU.
  volatile int sink = 0;
  for(int i = 0; i < 200000; i++)
    sink += i;
  (void)sink;
  printf("  thread_race tid_arg=%d still alive after work\n", id);
  thread_exit();
}

int
main(int argc, char *argv[])
{
  (void)argc; (void)argv;

  printf("thread_race: spawning %d threads, NOT joining (K5 path)\n",
         NTHREADS);

  char *stacks[NTHREADS];
  for(int i = 0; i < NTHREADS; i++){
    stacks[i] = malloc(STACK_SIZE);
    if(!stacks[i]){
      printf("thread_race: malloc failed for stack %d\n", i);
      exit(1);
    }
    int tid = thread_create(worker, (void*)(uint64)i, stacks[i] + STACK_SIZE);
    if(tid < 0){
      printf("thread_race: thread_create(%d) failed\n", i);
      exit(1);
    }
  }

  // Give threads a moment to start running, but do NOT join — exit while
  // they're still scheduled. The K5 fix guards freeproc(main) on this path.
  // (xv6 user.h has no sleep prototype; busy-spin briefly instead.)
  volatile int delay = 0;
  for(int i = 0; i < 100000; i++) delay += i;
  (void)delay;

  printf("thread_race: main exiting without join (K5 OK if kernel survives)\n");
  // Intentionally leak stacks[] — we're exiting.
  exit(0);
}
