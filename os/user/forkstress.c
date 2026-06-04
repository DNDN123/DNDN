// forkstress — minimal fork+wait stress test for proc-table leak detection.
//
// Forks N times sequentially. Each child exits immediately. Parent
// wait()s for it before the next iteration. If the proc-table is
// leak-free this always succeeds (every fork uses a slot that the
// previous wait() just freed). If a slot leaks per iteration, fork()
// eventually returns -1 once 64 slots fill up.
//
// Used to isolate `usertests -q reparent2` failure (fork failed at
// iteration 27/27 in the integration tree) into a single, focused
// reproducer.
//
// Usage:
//   $ forkstress           # default N=1000
//   $ forkstress 200       # fewer iterations
//
// Output:
//   FORKSTRESS done <ok>/<n>           on success
//   FORKSTRESS fail at <i> succeeded=<s>/<n>   on fork failure

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int n = 1000;
  if (argc > 1) {
    n = atoi(argv[1]);
    if (n <= 0) n = 1000;
  }

  int succeeded = 0;
  for (int i = 0; i < n; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("FORKSTRESS fail at %d succeeded=%d/%d\n", i, succeeded, n);
      exit(1);
    }
    if (pid == 0) {
      exit(0);
    }
    int status = 0;
    int reaped = wait(&status);
    if (reaped != pid) {
      printf("FORKSTRESS wait mismatch at %d: forked=%d reaped=%d\n",
             i, pid, reaped);
      exit(2);
    }
    succeeded++;
  }
  printf("FORKSTRESS done %d/%d\n", succeeded, n);
  exit(0);
}
