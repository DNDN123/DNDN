// nlrun: launch a program at a specific MLFQ queue level.
//
// Usage:
//   nlrun <queue 0|1|2> <program> [args...]
//
// Example:
//   nlrun 2 cpu_burner 1000000
//   nlrun 0 echo hello
//
// Used by the natural-language shell (host nl_shell.py): the LLM
// translates a user's plain-language request into a queue hint and a
// program command. The host prints the resulting `nlrun ...` line, and
// the user runs it inside xv6.
//
// Implementation: forkpri(level) to create a child already in the
// requested queue, then exec the program. Parent waits.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void
usage(void)
{
  printf("usage: nlrun <queue 0|1|2> <program> [args...]\n");
  exit(1);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    usage();
  }

  int level = atoi(argv[1]);
  if(level < 0 || level > 2){
    printf("nlrun: queue level must be 0, 1, or 2 (got %d)\n", level);
    exit(1);
  }

  // NOTE: tracing is intentionally OFF here — it floods the NL-shell console
  // with TRACE/EXIT lines. Scheduler behavior is still observable via `ps`
  // (the PRIO column shows MLFQ demotion). Use `tracepid <pid> 1` to opt in.

  // forkpri puts the child directly into queue `level` (no race vs setpri).
  int pid = forkpri(level);
  if(pid < 0){
    printf("nlrun: forkpri failed\n");
    exit(1);
  }

  if(pid == 0){
    // Child: rebuild argv for exec — argv[2] is program, argv[3..] its args.
    char *cargv[16];
    int ci = 0;
    for(int i = 2; i < argc && ci < 15; i++){
      cargv[ci++] = argv[i];
    }
    cargv[ci] = 0;

    exec(cargv[0], cargv);
    // exec only returns on failure.
    printf("nlrun: exec %s failed\n", cargv[0]);
    exit(1);
  }

  // Parent: wait for the child to finish.
  int status = 0;
  int wpid = wait(&status);
  printf("nlrun: child pid=%d done (queue=%d, status=%d)\n",
         wpid, level, status);
  exit(0);
}
