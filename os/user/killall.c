// killall <name> — terminate every process whose name matches <name>.
//
// Tier-1 "cleanup" helper: pure userspace, built only on existing syscalls
// (ps, kill, getpid). Enables NL commands like "cpu_burner 다 꺼줘".
//
// Safety: pid 0/1 (init) are never touched, the shell and init are protected
// by name, and we never kill ourselves — so the console/login can't die.
#include "kernel/types.h"
#include "kernel/procinfo.h"
#include "user/user.h"

#define MAX_PROCS 64

// procinfo.state enum: 0=unused 1=used 2=sleep 3=runnable 4=running 5=zombie
#define ST_UNUSED 0
#define ST_ZOMBIE 5

static int
protected_name(const char *n)
{
  return strcmp(n, "init") == 0
      || strcmp(n, "sh") == 0
      || strcmp(n, "killall") == 0;
}

int
main(int argc, char *argv[])
{
  if(argc != 2){
    printf("usage: killall <name>\n");
    exit(1);
  }

  char *target = argv[1];
  struct procinfo info[MAX_PROCS];
  int n = ps(info, MAX_PROCS);
  if(n < 0){
    printf("killall: ps failed\n");
    exit(1);
  }

  int self = getpid();
  int killed = 0;

  for(int i = 0; i < n; i++){
    if(info[i].pid <= 1) continue;                         // unused slot / init
    if(info[i].pid == self) continue;                      // don't kill self
    if(info[i].state == ST_UNUSED || info[i].state == ST_ZOMBIE) continue;
    if(protected_name(info[i].name)) continue;             // never kill shell/init
    if(strcmp(info[i].name, target) == 0){
      if(kill(info[i].pid) == 0){
        printf("killed pid %d (%s)\n", info[i].pid, info[i].name);
        killed++;
      }
    }
  }

  printf("killall: %d process(es) killed matching '%s'\n", killed, target);
  exit(0);
}
