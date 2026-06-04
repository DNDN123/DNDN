// mixed_burner: classic "mixed" workload — alternates between a short
// CPU burst and an I/O block. Forces the MLFQ scheduler to confront a
// process that is neither cleanly CPU-bound nor cleanly I/O-bound, so
// it ends up in MID under normal demotion rules.
//
// Usage:
//   mixed_burner <iterations>
//
// Each iteration: ~1 short CPU loop, then pause(1) to yield as if
// waiting on I/O. Total run/io ratio puts it in the MID band.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int iters = 30;
  if(argc >= 2){
    iters = atoi(argv[1]);
  }

  volatile unsigned long acc = 1;
  for(int i = 0; i < iters; i++){
    // CPU burst — long enough to consume the HIGH slice (=2 ticks)
    // but short enough that MID (=4 ticks) holds it.
    for(int j = 0; j < 30; j++){
      for(int k = 0; k < 100; k++){
        acc = (acc * 1103515245UL + 12345UL) & 0x7fffffff;
      }
    }
    // I/O proxy: sleep one tick. Per the sleep() patch this resets the
    // slice counter but does not demote — so over the run we get a
    // sawtooth between fresh slice and partial slice, landing in MID.
    pause(1);
  }

  printf("mixed_burner pid=%d done iters=%d acc=%lu\n",
         getpid(), iters, acc);
  exit(0);
}
