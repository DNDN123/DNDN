// uptime — print the number of timer ticks since boot (SYS_uptime).
#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  printf("uptime: %d ticks\n", uptime());
  exit(0);
}
