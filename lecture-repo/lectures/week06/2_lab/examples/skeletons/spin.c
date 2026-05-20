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
  // TODO: Write an infinite loop that keeps the CPU busy.
  // This makes the process RUNNABLE at all times, which is useful
  // for observing the scheduler's round-robin behavior.

  exit(0);
}
