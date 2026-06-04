// setprio <pid> <level>
//
// Integrated edition — original haneol setprio.c used a 0..20 priority
// scheme, but the integrated kernel uses hyunsung's MLFQ queue levels (0..2).
// This wrapper just calls setpri() which the kernel validates.
//
// Output format (kept stable for the host NL bridge to parse):
//   OK pid=<pid> level=<level>     on success
//   ERR ...                         on failure
//
//   level=0  HIGH   (short slice, interactive)
//   level=1  MID    (medium slice)
//   level=2  LOW    (long slice, CPU-bound)
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("ERR usage: setprio <pid> <level 0..2>\n");
    exit(1);
  }
  int pid   = atoi(argv[1]);
  int level = atoi(argv[2]);

  if(level < 0 || level > 2){
    printf("ERR level out of range (0..2): %d\n", level);
    exit(1);
  }

  if(setpri(pid, level) < 0){
    printf("ERR setpri pid=%d level=%d failed\n", pid, level);
    exit(1);
  }

  printf("OK pid=%d level=%d\n", pid, level);
  exit(0);
}
