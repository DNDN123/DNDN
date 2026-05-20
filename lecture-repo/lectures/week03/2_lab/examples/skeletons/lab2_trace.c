// lab2_trace.c — Test program for sys_fork() tracing
//
// After applying the sys_fork trace patch, run this program in xv6
// to confirm that the kernel prints a trace message each time fork() is called.
//
// To add this program to xv6:
//   1. Copy this file to xv6-riscv/user/lab2_trace.c
//   2. Edit xv6-riscv/Makefile: add $U/_lab2_trace to UPROGS
//   3. Run: make clean && make qemu
//   4. At the xv6 shell prompt: lab2_trace

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid;

  printf("=== Lab 2: sys_fork() trace test ===\n");

  // TODO: Call fork() to create a child process.
  // The kernel should print a [TRACE] message if the patch is applied.

  // TODO: Check the return value of fork().
  // - If pid < 0: fork failed, print an error and exit.
  // - If pid == 0: this is the child process, print "child: ..." message.
  // - If pid > 0: this is the parent process, print "parent: ..." message
  //   and call wait() to wait for the child.

  printf("=== Test complete ===\n");
  exit(0);
}
