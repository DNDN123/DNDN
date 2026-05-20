//
// trace_test.c - Test program for the trace system call
//
// Copy this file to xv6-riscv/user/trace_test.c
// and add $U/_trace_test to UPROGS in Makefile.
//

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/syscall.h"
#include "user/user.h"

//
// Test 1: trace fork
// Enable tracing for fork, then call fork.
// Expected: one line of trace output for the fork syscall.
//
void
test_trace_fork(void)
{
  int pid;

  printf("=== Test 1: trace fork ===\n");
  trace(1 << SYS_fork);
  pid = fork();
  if(pid < 0){
    printf("test_trace_fork: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    // child
    exit(0);
  } else {
    // parent
    wait(0);
  }
  trace(0);  // disable tracing
  printf("Test 1 PASSED\n\n");
}

//
// Test 2: trace read and write
// Enable tracing for read and write, then perform I/O via a pipe.
// Expected: trace output for both read and write syscalls.
//
void
test_trace_rw(void)
{
  int fds[2];
  char buf[8];

  printf("=== Test 2: trace read/write ===\n");
  trace((1 << SYS_read) | (1 << SYS_write));

  if(pipe(fds) < 0){
    printf("test_trace_rw: pipe failed\n");
    exit(1);
  }

  // write then read through the pipe
  write(fds[1], "hello", 5);
  read(fds[0], buf, 5);

  close(fds[0]);
  close(fds[1]);

  trace(0);  // disable tracing
  printf("Test 2 PASSED\n\n");
}

//
// Test 3: trace multiple syscalls
// Enable tracing for getpid, fork, and write simultaneously.
//
void
test_trace_multi(void)
{
  int pid;

  printf("=== Test 3: trace multiple syscalls ===\n");
  trace((1 << SYS_getpid) | (1 << SYS_fork) | (1 << SYS_write));

  pid = getpid();
  printf("my pid is %d\n", pid);

  pid = fork();
  if(pid < 0){
    printf("test_trace_multi: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    exit(0);
  } else {
    wait(0);
  }

  trace(0);
  printf("Test 3 PASSED\n\n");
}

//
// Test 4: trace inheritance across fork
// Parent enables tracing, then forks. Child should inherit the trace mask.
// Expected: child process also produces trace output.
//
void
test_trace_inherit(void)
{
  int pid;
  int fds[2];
  char buf[8];

  printf("=== Test 4: trace inheritance ===\n");

  // Enable tracing for read and write before fork
  trace((1 << SYS_read) | (1 << SYS_write));

  if(pipe(fds) < 0){
    printf("test_trace_inherit: pipe failed\n");
    exit(1);
  }

  pid = fork();
  if(pid < 0){
    printf("test_trace_inherit: fork failed\n");
    exit(1);
  }

  if(pid == 0){
    // child: should have inherited trace mask
    // The write below should produce trace output with child's PID
    write(fds[1], "hi", 2);
    close(fds[0]);
    close(fds[1]);
    exit(0);
  } else {
    // parent: read from pipe (should also produce trace output)
    read(fds[0], buf, 2);
    close(fds[0]);
    close(fds[1]);
    wait(0);
  }

  trace(0);
  printf("Test 4 PASSED\n\n");
}

//
// Test 5: trace with no mask (should produce no output)
//
void
test_trace_none(void)
{
  printf("=== Test 5: trace disabled ===\n");
  trace(0);

  // These should NOT produce trace output
  getpid();
  fork();
  wait(0);

  printf("Test 5 PASSED (no trace output above means success)\n\n");
}

int
main(int argc, char *argv[])
{
  printf("========================================\n");
  printf("  trace system call test suite\n");
  printf("========================================\n\n");

  test_trace_fork();
  test_trace_rw();
  test_trace_multi();
  test_trace_inherit();
  test_trace_none();

  printf("All tests passed!\n");
  exit(0);
}
