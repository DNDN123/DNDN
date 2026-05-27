// diagprog: dump getstats() for every live process — the "system
// diagnosis" data source for the host-side NL shell.
//
// Output format (one STATS line per live proc, parseable by host Python):
//   STATS pid=<N> priority=<L> run=<R> io=<I> arrival=<A> completion=<C>
//
// Skips its own pid + framework names so the diagnosis isn't polluted.
//
// Pipe the output to a file then feed it to:
//   python nl_shell.py --diagnose-from <file>

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROC_SCAN 64   // xv6 default NPROC is 64

int
main(void)
{
  int me = getpid();
  struct procstats st;

  printf("DIAG_BEGIN\n");
  for(int pid = 1; pid < NPROC_SCAN; pid++){
    if(pid == me) continue;
    if(getstats(pid, &st) < 0) continue;     // slot empty / dead — skip
    // Don't filter by name here — the host-side parser knows what to skip.
    printf("STATS pid=%d priority=%d run=%d io=%d arrival=%d completion=%d final_pri=%d\n",
           st.pid, st.priority, st.total_run_ticks, st.total_io_blocks,
           st.arrival_tick, st.completion_tick, st.final_pri);
  }
  printf("DIAG_END\n");
  exit(0);
}
