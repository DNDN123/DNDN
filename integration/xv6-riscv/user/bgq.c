// bgq — background work queue for the integrated xv6 tree.
//
// Reads a jobs file (one command per line, blank lines and "#"-comments
// allowed) and runs each entry as a separate child process pinned to the
// LOW MLFQ queue via forkpri(2). Waits for each child before starting the
// next, reaping the zombie with wait(). Demonstrates priority scheduling
// + sequential queue semantics + zombie cleanup in a single binary.
//
// Usage (inside xv6 shell):
//   $ bgq jobs.txt
//
// jobs.txt example:
//   # heavy computation, queue at LOW
//   cpu_burner 200000
//   # interactive-ish file read, still queued at LOW because background
//   cat README
//   echo done
//
// Output (one line per state transition, parseable by host tools):
//   BGQ start  <name>
//   BGQ done   <name> pid=<n> exit=<n>
//   BGQ all_done <count>
//
// Limits: at most BGQ_MAX_LINE bytes per line, BGQ_MAX_ARGS args.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BGQ_BUF       4096
#define BGQ_MAX_LINE  128
#define BGQ_MAX_ARGS  16
#define BGQ_LOW_PRI   2

static int
split_args(char *line, char *argv[BGQ_MAX_ARGS])
{
  int argc = 0;
  char *p = line;
  while (*p && argc < BGQ_MAX_ARGS - 1) {
    while (*p == ' ' || *p == '\t')
      *p++ = 0;
    if (*p == 0)
      break;
    argv[argc++] = p;
    while (*p && *p != ' ' && *p != '\t')
      p++;
  }
  argv[argc] = 0;
  return argc;
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(2, "usage: bgq <jobs.txt>\n");
    exit(1);
  }

  int fd = open(argv[1], 0);
  if (fd < 0) {
    fprintf(2, "bgq: cannot open %s\n", argv[1]);
    exit(1);
  }

  static char buf[BGQ_BUF];
  int n = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (n <= 0) {
    fprintf(2, "bgq: empty or unreadable %s\n", argv[1]);
    exit(1);
  }
  buf[n] = 0;

  int done = 0;
  char *line = buf;
  while (*line) {
    // Find end of current line.
    char *next = line;
    while (*next && *next != '\n')
      next++;
    if (*next == '\n')
      *next++ = 0;

    // Trim leading whitespace; skip blanks/comments.
    char *t = line;
    while (*t == ' ' || *t == '\t')
      t++;
    if (*t == 0 || *t == '#') {
      line = next;
      continue;
    }

    char *cargv[BGQ_MAX_ARGS];
    int cargc = split_args(t, cargv);
    if (cargc == 0) {
      line = next;
      continue;
    }

    printf("BGQ start %s\n", cargv[0]);

    int pid = forkpri(BGQ_LOW_PRI);
    if (pid < 0) {
      fprintf(2, "BGQ fork failed for %s\n", cargv[0]);
      line = next;
      continue;
    }
    if (pid == 0) {
      exec(cargv[0], cargv);
      fprintf(2, "BGQ exec failed: %s\n", cargv[0]);
      exit(127);
    }

    // Parent: wait for the queued job to finish, reaping its zombie slot.
    int status = 0;
    int reaped = wait(&status);
    printf("BGQ done %s pid=%d exit=%d\n", cargv[0], reaped, status);
    done++;

    line = next;
  }

  printf("BGQ all_done %d\n", done);
  exit(0);
}
