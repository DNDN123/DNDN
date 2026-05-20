// setprio <pid> <prio>
//
// Change the scheduling priority of process <pid> to <prio> (0..20).
// Lower number = higher priority. Default is 10.
//
// Output is intentionally compact and stable so the host NL bridge can
// parse a one-shot success/error line:
//   "OK pid=<pid> prio=<prio>"   on success
//   "ERR ..."                     on failure
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("ERR usage: setprio <pid> <prio 0..20>\n");
    exit(1);
  }
  int pid  = atoi(argv[1]);
  int prio = atoi(argv[2]);
  if(prio < 0 || prio > 20){
    printf("ERR prio out of range (got %d, must be 0..20)\n", prio);
    exit(1);
  }
  if(setpriority(pid, prio) < 0){
    printf("ERR setpriority(%d,%d) failed (no such pid?)\n", pid, prio);
    exit(1);
  }
  printf("OK pid=%d prio=%d\n", pid, prio);
  exit(0);
}
