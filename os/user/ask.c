// ask: speak to the OS in natural language from inside xv6.
//
// Usage (inside the xv6 console):
//   ask 7번 프로세스 꺼줘
//   ask cpu 많이 먹는 작업 하나 낮은 우선순위로 돌려줘
//
// ask itself does NOT understand language. It simply emits the request on
// the console wrapped in a sentinel line:
//
//   @@NL <the whole request>
//
// The host bridge (host/nlbridge.py) wraps QEMU, watches the console for
// that sentinel, asks the LLM to translate it into an intent, runs it
// through the SafetyGuard, and types the resulting xv6 command back into
// this same console. From the user's seat it looks like xv6 understood
// plain language directly.
//
// The LLM never enters the kernel: only the small translated command
// (e.g. "kill 7") ever crosses the syscall boundary.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf("usage: ask <natural language request>\n");
    exit(1);
  }

  // Emit the sentinel, then the request words joined by single spaces.
  printf("@@NL ");
  for(int i = 1; i < argc; i++){
    write(1, argv[i], strlen(argv[i]));
    if(i + 1 < argc)
      write(1, " ", 1);
  }
  write(1, "\n", 1);

  exit(0);
}
