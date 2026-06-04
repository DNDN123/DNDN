// wrunner: workload runner for Smart-MLFQ-xv6.
// (Renamed from "workload_runner" because xv6 DIRSIZ=14.)
//
// Usage:
//   wrunner <workload_file>            (baseline: default priority)
//   wrunner <workload_file> <hints>    (LLM mode: hints applied via forkpri)
//
// Workload file format (one line per process, # = comment):
//   <name> <arg> <spawn_at_tick>
//
// Hints file format (one line per program name, # = comment):
//   <name> <level>     (0=HIGH 1=MID 2=LOW)
//
// This version uses the new syscalls from the patch bundle:
//   - settrace(0, 1) at start so wrunner + all its children emit TRACE
//   - forkpri(level) when a hint is available, avoiding the
//     fork() → setpri() race window.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAX_PROCS 16
#define MAX_HINTS 16
#define LINE_MAX 128

struct workitem {
  char name[32];
  char arg[32];
  int spawn_at;
};

struct hint {
  char name[32];
  int level;
};

static struct workitem items[MAX_PROCS];
static int nitems = 0;

static struct hint hints[MAX_HINTS];
static int nhints = 0;

static int
read_line(int fd, char *buf, int max)
{
  int n = 0;
  char c;
  while(n < max - 1){
    int r = read(fd, &c, 1);
    if(r <= 0){
      if(n == 0) return 0;
      break;
    }
    if(c == '\n') break;
    buf[n++] = c;
  }
  buf[n] = 0;
  return 1;
}

static char *
skip_ws(char *s)
{
  while(*s == ' ' || *s == '\t') s++;
  return s;
}

static char *
take_token(char *s, char *out, int max)
{
  s = skip_ws(s);
  int n = 0;
  while(*s && *s != ' ' && *s != '\t' && n < max - 1){
    out[n++] = *s++;
  }
  out[n] = 0;
  return s;
}

static int
load_workload(const char *path)
{
  int fd = open((char*)path, O_RDONLY);
  if(fd < 0){
    printf("wrunner: cannot open %s\n", path);
    return -1;
  }
  char line[LINE_MAX];
  while(read_line(fd, line, sizeof(line))){
    char *p = skip_ws(line);
    if(*p == 0 || *p == '#') continue;
    if(nitems >= MAX_PROCS){
      printf("wrunner: too many items\n");
      break;
    }
    char arg_spawn[32];
    p = take_token(p, items[nitems].name, sizeof(items[nitems].name));
    p = take_token(p, items[nitems].arg, sizeof(items[nitems].arg));
    p = take_token(p, arg_spawn, sizeof(arg_spawn));
    items[nitems].spawn_at = atoi(arg_spawn);
    nitems++;
  }
  close(fd);
  return 0;
}

static int
load_hints(const char *path)
{
  int fd = open((char*)path, O_RDONLY);
  if(fd < 0){
    printf("wrunner: no hints file (%s), using defaults\n", path);
    return -1;
  }
  char line[LINE_MAX];
  while(read_line(fd, line, sizeof(line))){
    char *p = skip_ws(line);
    if(*p == 0 || *p == '#') continue;
    if(nhints >= MAX_HINTS) break;
    char lvl[16];
    p = take_token(p, hints[nhints].name, sizeof(hints[nhints].name));
    p = take_token(p, lvl, sizeof(lvl));
    hints[nhints].level = atoi(lvl);
    nhints++;
  }
  close(fd);
  return 0;
}

// Look up a hint for a program name. Returns level or -1 if none.
static int
lookup_hint(const char *name)
{
  for(int i = 0; i < nhints; i++){
    int match = 1;
    for(int j = 0; ; j++){
      if(hints[i].name[j] != name[j]){ match = 0; break; }
      if(name[j] == 0) break;
    }
    if(match) return hints[i].level;
  }
  return -1;
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf("usage: wrunner <workload_file> [hints_file]\n");
    exit(1);
  }

  // Enable tracing for ourselves (and via inheritance, our children).
  settrace(0, 1);

  if(load_workload(argv[1]) < 0) exit(1);

  if(argc >= 3){
    load_hints(argv[2]);
  }

  printf("wrunner: loaded %d items, %d hints\n", nitems, nhints);

  int t0 = uptime();
  int spawned = 0;

  while(spawned < nitems){
    int now = uptime() - t0;
    for(int i = 0; i < nitems; i++){
      if(items[i].name[0] == 0) continue;
      if(items[i].spawn_at <= now){
        int lvl = lookup_hint(items[i].name);
        int pid;
        if(lvl >= 0){
          // Use forkpri to atomically set initial priority — avoids the
          // fork()→setpri() race where the child might start running
          // before its priority is applied.
          pid = forkpri(lvl);
        } else {
          pid = fork();
        }

        if(pid == 0){
          // child
          char *cmd_argv[3];
          cmd_argv[0] = items[i].name;
          cmd_argv[1] = items[i].arg;
          cmd_argv[2] = 0;
          exec(items[i].name, cmd_argv);
          printf("wrunner: exec %s failed\n", items[i].name);
          exit(1);
        } else if(pid > 0){
          if(lvl >= 0){
            printf("wrunner: spawn %s pid=%d priority=%d\n",
                   items[i].name, pid, lvl);
          } else {
            printf("wrunner: spawn %s pid=%d (default priority)\n",
                   items[i].name, pid);
          }
          items[i].name[0] = 0;
          spawned++;
        } else {
          printf("wrunner: fork failed\n");
          exit(1);
        }
      }
    }
    if(spawned < nitems){
      pause(1);
    }
  }

  int status;
  while(wait(&status) > 0) { /* reap */ }

  printf("ALL_DONE wrunner pid=%d\n", getpid());
  exit(0);
}
