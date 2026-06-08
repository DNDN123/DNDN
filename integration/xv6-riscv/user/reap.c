// reap — clean up abandoned zombie processes.
//
// In xv6 a zombie is reaped by its parent's wait(). If a live parent never
// wait()s, the zombie lingers and holds a proc-table slot. `reap` asks the
// kernel (sys_reap) to reparent every such abandoned zombie to init, which
// then frees it through the normal wait() path. Enables NL "좀비 없애줘".
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int n = reap();
  if(n < 0){
    printf("reap: failed\n");
    exit(1);
  }
  printf("reap: %d zombie(s) cleaned up\n", n);
  exit(0);
}
