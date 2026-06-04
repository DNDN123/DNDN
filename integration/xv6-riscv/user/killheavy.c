// killheavy [n] — kill the n (default 1) most CPU-heavy processes, i.e. those
// with the highest total_run_ticks. Relieves a loaded system on request.
//
// Tier-1 "cleanup/optimize" helper: pure userspace, built on ps + getstats +
// kill. Enables NL commands like "무거운 거 꺼줘" / "느린 거 정리해줘".
//
// Safety: pid 0/1 (init), the shell, init, and ourselves are never killed.
#include "kernel/types.h"
#include "kernel/procinfo.h"
#include "user/user.h"

#define MAX_PROCS 64
#define ST_UNUSED 0
#define ST_ZOMBIE 5

static int
protected_proc(struct procinfo *p, int self)
{
  if(p->pid <= 1 || p->pid == self) return 1;
  if(p->state == ST_UNUSED || p->state == ST_ZOMBIE) return 1;
  if(strcmp(p->name, "init") == 0) return 1;
  if(strcmp(p->name, "sh") == 0) return 1;
  if(strcmp(p->name, "killheavy") == 0) return 1;
  return 0;
}

int
main(int argc, char *argv[])
{
  int want = (argc >= 2) ? atoi(argv[1]) : 1;
  if(want <= 0) want = 1;

  struct procinfo info[MAX_PROCS];
  int n = ps(info, MAX_PROCS);
  if(n < 0){
    printf("killheavy: ps failed\n");
    exit(1);
  }

  int self = getpid();

  for(int round = 0; round < want; round++){
    int best = -1, best_run = -1;
    struct procstats st;
    for(int i = 0; i < n; i++){
      if(protected_proc(&info[i], self)) continue;
      if(getstats(info[i].pid, &st) < 0) continue;
      if(st.total_run_ticks > best_run){
        best_run = st.total_run_ticks;
        best = i;
      }
    }
    if(best < 0){
      printf("killheavy: no eligible process to kill\n");
      break;
    }
    if(kill(info[best].pid) == 0)
      printf("killed heaviest pid %d (%s, run_ticks=%d)\n",
             info[best].pid, info[best].name, best_run);
    info[best].state = ST_UNUSED;   // consume so the next round skips it
  }

  exit(0);
}
