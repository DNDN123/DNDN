// tracepid <pid> <0|1> — toggle syscall tracing for a specific pid (settrace).
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("usage: tracepid <pid> <0|1>\n");
    exit(1);
  }
  int pid = atoi(argv[1]);
  int on  = atoi(argv[2]);
  if(settrace(pid, on) < 0){
    printf("tracepid: settrace pid=%d failed\n", pid);
    exit(1);
  }
  printf("tracepid: pid %d trace %s\n", pid, on ? "on" : "off");
  exit(0);
}
