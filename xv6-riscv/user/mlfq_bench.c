// mlfq_bench — measure MLFQ fairness / throughput
//
// Forks N children with HIGH / NORMAL / LOW priorities, each burns the
// same fixed amount of CPU.  Parent measures wall-clock ticks per child
// from fork() to wait() return.
//
// With a pure priority scheduler, LOW children should starve (very large
// completion ticks). With a working MLFQ + periodic boost, LOW children
// should still complete in bounded time because the boost periodically
// promotes them back to HIGH.
//
// Output is one machine-readable line per child:
//   BENCH name=<HIGH|NORMAL|LOW> prio=<p> ticks=<t>
// followed by a summary:
//   BENCH_DONE total=<sum_ticks>
//
// The host bridge can grep these lines into a table / graph.
#include "kernel/types.h"
#include "user/user.h"

#define BURN_ITERS 200000000  // identical work per child

static void
burn(int iters)
{
  volatile int x = 0;
  for(int i = 0; i < iters; i++)
    x += i * 3 + 1;
}

struct slot {
  const char *label;
  int prio;
};

int
main(int argc, char *argv[])
{
  struct slot slots[3] = {
    {"HIGH",   1},
    {"NORMAL", 10},
    {"LOW",    19},
  };

  printf("BENCH_START burn_iters=%d\n", BURN_ITERS);

  int pids[3];
  int start_ticks[3];

  for(int i = 0; i < 3; i++){
    int t0 = uptime();
    int pid = fork();
    if(pid < 0){
      printf("BENCH_ERR fork failed\n");
      exit(1);
    }
    if(pid == 0){
      // child
      setpriority(getpid(), slots[i].prio);
      burn(BURN_ITERS);
      exit(0);
    }
    pids[i] = pid;
    start_ticks[i] = t0;
  }

  // Reap children and report per-child elapsed ticks.
  // wait() doesn't tell us which pid exited first, but order doesn't matter
  // for our metric — we just need elapsed time per slot.
  // Instead we wait sequentially in slot order; the child that finishes
  // first will have already exited so wait() returns immediately for it.
  // Use simple loop: keep calling wait() and match by pid.
  int done = 0;
  int elapsed[3] = {-1, -1, -1};
  while(done < 3){
    int s;
    int wp = wait(&s);
    int t1 = uptime();
    for(int i = 0; i < 3; i++){
      if(pids[i] == wp){
        elapsed[i] = t1 - start_ticks[i];
        break;
      }
    }
    done++;
  }

  int total = 0;
  for(int i = 0; i < 3; i++){
    printf("BENCH name=%s prio=%d ticks=%d\n",
           slots[i].label, slots[i].prio, elapsed[i]);
    total += elapsed[i];
  }
  printf("BENCH_DONE total=%d\n", total);
  exit(0);
}
