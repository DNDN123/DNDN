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

  pid = fork();

  if (pid < 0) {
    printf("fork failed!\n");
    exit(1);
  } else if (pid == 0) {
    // Child process
    printf("child: I am the child (fork returned %d)\n", pid);
    printf("child: my pid is %d\n", getpid());
  } else {
    // Parent process
    printf("parent: created child with pid %d\n", pid);
    printf("parent: my pid is %d\n", getpid());
    wait(0);
    printf("parent: child has exited\n");
  }

  printf("=== Test complete ===\n");
  exit(0);
}
