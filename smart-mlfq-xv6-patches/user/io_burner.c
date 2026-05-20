// io_burner: frequent I/O blocking. Minimal CPU.
// Usage: io_burner <ops>

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
  int ops = 20;
  if(argc >= 2){
    ops = atoi(argv[1]);
  }

  char buf[16];
  for(int i = 0; i < 16; i++) buf[i] = 'A' + (i % 26);

  char fname[32];
  for(int op = 0; op < ops; op++){
    // pause(1) makes us sleep one tick — proxy for I/O block.
    pause(1);

    int pid = getpid();
    char *p = fname;
    *p++ = 'i'; *p++ = 'o'; *p++ = '_';
    int tmp = pid;
    char digs[8]; int d = 0;
    if(tmp == 0) digs[d++] = '0';
    while(tmp > 0){ digs[d++] = '0' + (tmp % 10); tmp /= 10; }
    for(int k = d-1; k >= 0; k--) *p++ = digs[k];
    *p++ = '_';
    tmp = op; d = 0;
    if(tmp == 0) digs[d++] = '0';
    while(tmp > 0){ digs[d++] = '0' + (tmp % 10); tmp /= 10; }
    for(int k = d-1; k >= 0; k--) *p++ = digs[k];
    *p = 0;

    int fd = open(fname, O_CREATE | O_WRONLY);
    if(fd >= 0){
      write(fd, buf, sizeof(buf));
      close(fd);
      unlink(fname);
    }
  }

  printf("io_burner pid=%d done ops=%d\n", getpid(), ops);
  exit(0);
}
