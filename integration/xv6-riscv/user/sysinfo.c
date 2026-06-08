// sysinfo — print free memory and active process count (SYS_sysinfo).
#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

int
main(void)
{
  struct sysinfo info;
  if(sysinfo(&info) < 0){
    printf("sysinfo: failed\n");
    exit(1);
  }
  printf("freemem: %d bytes\n", (int)info.freemem);
  printf("nproc:   %d\n", (int)info.nproc);
  exit(0);
}
