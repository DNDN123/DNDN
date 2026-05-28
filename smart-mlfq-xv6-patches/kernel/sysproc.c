// Smart-MLFQ-xv6 modified sysproc.c
// =================================
// Added: sys_setpri, sys_getstats, sys_settrace, sys_forkpri implementations.

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// === Smart-MLFQ syscalls ===

// setpri(int pid, int level)
//   level: 0 (HIGH), 1 (MID), 2 (LOW)
// Returns 0 on success, -1 on invalid arg or pid not found.
uint64
sys_setpri(void)
{
  int pid, level;
  argint(0, &pid);
  argint(1, &level);
  return proc_setpri(pid, level);
}

// getstats(int pid, struct procstats *out)
// Fills *out with the process's MLFQ stats.
// Returns 0 on success, -1 on pid not found.
uint64
sys_getstats(void)
{
  int pid;
  uint64 addr;
  struct procstats local;

  argint(0, &pid);
  argaddr(1, &addr);

  if(proc_getstats(pid, &local) < 0) return -1;

  // Copy struct procstats to user space.
  struct proc *p = myproc();
  if(copyout(p->pagetable, addr, (char*)&local, sizeof(local)) < 0)
    return -1;
  return 0;
}

// settrace(int pid, int on)
//   Enable (on=1) or disable (on=0) TRACE/EXIT logging for `pid`.
//   pid == 0 means "current process".
// Returns 0 on success, -1 if pid not found.
uint64
sys_settrace(void)
{
  int pid, on;
  argint(0, &pid);
  argint(1, &on);
  return proc_settrace(pid, on);
}

// forkpri(int level)
//   Like fork(), but the child is created with priority `level` (0/1/2)
//   set BEFORE it becomes RUNNABLE. Eliminates the race where a freshly-
//   forked child runs a few ticks at HIGH before the parent's setpri()
//   takes effect.
// Returns child's pid in the parent, 0 in the child, -1 on error.
uint64
sys_forkpri(void)
{
  int level;
  argint(0, &level);
  return kforkpri(level);
}
