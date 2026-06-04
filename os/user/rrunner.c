// rrunner: workload runner compatible with STOCK xv6 (no MLFQ syscalls).
// Used to benchmark stock round-robin scheduler against the MLFQ build.
//
// Same workload file format as wrunner:
//   <name> <arg> <spawn_at_tick>
//
// Measures total batch wall-clock time via uptime() and prints:
//   RR_BATCH workload=<file> elapsed=<ticks> n_procs=<N>

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAX_PROCS 16
#define LINE_MAX 128

struct workitem {
  char name[32];
  char arg[32];
  int spawn_at;
};

static struct workitem items[MAX_PROCS];
static int nitems = 0;

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
skip_ws(char *s) { while(*s == ' ' || *s == '\t') s++; return s; }

static char *
take_token(char *s, char *out, int max)
{
  s = skip_ws(s);
  int n = 0;
  while(*s && *s != ' ' && *s != '\t' && n < max - 1) out[n++] = *s++;
  out[n] = 0;
  return s;
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf("usage: rrunner <workload_file>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDONLY);
  if(fd < 0){ printf("rrunner: cannot open %s\n", argv[1]); exit(1); }

  char line[LINE_MAX];
  while(read_line(fd, line, sizeof(line))){
    char *p = skip_ws(line);
    if(*p == 0 || *p == '#') continue;
    if(nitems >= MAX_PROCS){ printf("rrunner: too many items\n"); break; }
    char sp[32];
    p = take_token(p, items[nitems].name, sizeof(items[nitems].name));
    p = take_token(p, items[nitems].arg,  sizeof(items[nitems].arg));
    p = take_token(p, sp, sizeof(sp));
    items[nitems].spawn_at = atoi(sp);
    nitems++;
  }
  close(fd);

  printf("rrunner: loaded %d items\n", nitems);

  int t0 = uptime();
  int spawned = 0;

  while(spawned < nitems){
    int now = uptime() - t0;
    for(int i = 0; i < nitems; i++){
      if(items[i].name[0] == 0) continue;
      if(items[i].spawn_at <= now){
        int pid = fork();
        if(pid == 0){
          char *cargv[3];
          cargv[0] = items[i].name;
          cargv[1] = items[i].arg;
          cargv[2] = 0;
          exec(items[i].name, cargv);
          printf("rrunner: exec %s failed\n", items[i].name);
          exit(1);
        }
        items[i].name[0] = 0;
        spawned++;
      }
    }
    if(spawned < nitems) pause(1);
  }

  int status;
  while(wait(&status) > 0) { /* reap */ }

  int elapsed = uptime() - t0;
  printf("RR_BATCH workload=%s elapsed=%d n_procs=%d\n",
         argv[1], elapsed, nitems);
  exit(0);
}
