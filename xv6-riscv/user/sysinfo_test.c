#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  struct sysinfo info;

  printf("========================================\n");
  printf("  sysinfo system call test\n");
  printf("========================================\n\n");

  if(sysinfo(&info) < 0){
    printf("FAIL: sysinfo call failed\n");
    exit(1);
  }

  printf("Free memory: %d bytes (%d KB)\n", (int)info.freemem, (int)(info.freemem / 1024));
  printf("Active processes: %d\n", (int)info.nproc);

  // Basic sanity checks
  if(info.freemem == 0){
    printf("FAIL: freemem should not be 0\n");
    exit(1);
  }
  if(info.nproc < 2){
    printf("FAIL: nproc should be at least 2 (init + sh)\n");
    exit(1);
  }

  // Test: fork should increase nproc
  int pid = fork();
  if(pid < 0){
    printf("FAIL: fork failed\n");
    exit(1);
  }
  if(pid == 0){
    // child
    struct sysinfo info2;
    if(sysinfo(&info2) < 0){
      printf("FAIL: sysinfo in child failed\n");
      exit(1);
    }
    if(info2.nproc <= info.nproc){
      printf("FAIL: nproc should increase after fork\n");
      exit(1);
    }
    printf("After fork, active processes: %d\n", (int)info2.nproc);
    exit(0);
  } else {
    wait(0);
  }

  printf("\nAll sysinfo tests passed!\n");
  exit(0);
}
