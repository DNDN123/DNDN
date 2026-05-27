// ps — list active processes via the sys_ps system call.
//
// Output format (stable for the host NL bridge to parse):
//   PID  PPID  STATE    PRIO  SZ      NAME
//   1    0     sleep    10    12288   init
//   ...
//
// Trailing line:  "TOTAL <n>"  (so the parser can know the count cheaply).
#include "kernel/types.h"
#include "kernel/procinfo.h"
#include "user/user.h"

#define MAX_PROCS 64

static char *
state_name(int s)
{
  switch(s){
  case 0: return "unused";
  case 1: return "used";
  case 2: return "sleep";
  case 3: return "runble";
  case 4: return "run";
  case 5: return "zombie";
  default: return "?";
  }
}

int
main(int argc, char *argv[])
{
  struct procinfo info[MAX_PROCS];
  int n = ps(info, MAX_PROCS);

  if(n < 0){
    printf("ps: system call failed\n");
    exit(1);
  }

  printf("PID  PPID  STATE    PRIO  SZ        NAME\n");
  for(int i = 0; i < n; i++){
    printf("%d  %d  %s  %d  %d  %s\n",
           info[i].pid,
           info[i].ppid,
           state_name(info[i].state),
           info[i].priority,
           (int)info[i].sz,
           info[i].name);
  }
  printf("TOTAL %d\n", n);
  exit(0);
}
