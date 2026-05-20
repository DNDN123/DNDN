// spin.c -- xv6 user program for testing round-robin scheduling
// Usage: run multiple instances in xv6 shell:
//   $ spin &
//   $ spin &
//   $ spin &

#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  for (;;)
    ;  // infinite loop: keeps CPU busy

  exit(0);  // never reached
}
