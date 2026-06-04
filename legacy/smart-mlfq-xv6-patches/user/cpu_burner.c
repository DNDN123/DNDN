// cpu_burner: tight CPU loop. No I/O.
// Usage: cpu_burner <iterations>

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int iters = 100000;
  if(argc >= 2){
    iters = atoi(argv[1]);
  }

  volatile unsigned long acc = 1;
  for(int i = 0; i < iters; i++){
    for(int j = 0; j < 100; j++){
      acc = (acc * 1103515245UL + 12345UL) & 0x7fffffff;
    }
  }

  printf("cpu_burner pid=%d done acc=%lu\n", getpid(), acc);
  exit(0);
}
