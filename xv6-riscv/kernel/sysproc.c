#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"
#include "procinfo.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
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
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
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

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->trace_mask = mask;
  return 0;
}

// kalloc.c
extern uint64 freemem_count(void);
// proc.c
extern uint64 proc_count(void);
extern struct proc proc[NPROC];

uint64
sys_sysinfo(void)
{
  uint64 addr;
  struct sysinfo info;
  struct proc *p = myproc();

  argaddr(0, &addr);

  info.freemem = freemem_count();
  info.nproc = proc_count();

  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}

// setpriority(pid, priority): set the scheduling priority of process pid.
// Returns 0 on success, -1 if pid not found or priority is out of [0,20].
uint64
sys_setpriority(void)
{
  int pid, priority;
  struct proc *p;

  argint(0, &pid);
  argint(1, &priority);

  if(priority < 0 || priority > 20)
    return -1;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED){
      p->priority = priority;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// getpriority(pid): return the scheduling priority of process pid,
// or -1 if no such process.
uint64
sys_getpriority(void)
{
  int pid;
  struct proc *p;

  argint(0, &pid);

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED){
      int prio = p->priority;
      release(&p->lock);
      return prio;
    }
    release(&p->lock);
  }
  return -1;
}

// ps(struct procinfo *buf, int max):
// Snapshot up to `max` active processes into the user buffer.
// Returns the number of entries written, or -1 on error.
//
// Note: parent->pid is read without wait_lock — this is the same
// race-tolerant pattern as procdump(). Acceptable for a debug-style
// listing; do NOT use the snapshot for safety-critical decisions.
uint64
sys_ps(void)
{
  uint64 addr;
  int max;
  struct proc *p;
  struct procinfo info;
  struct proc *self = myproc();
  int n = 0;

  argaddr(0, &addr);
  argint(1, &max);

  if(max <= 0)
    return -1;

  for(p = proc; p < &proc[NPROC] && n < max; p++){
    acquire(&p->lock);
    if(p->state != UNUSED){
      info.pid = p->pid;
      info.ppid = p->parent ? p->parent->pid : 0;
      info.state = (int)p->state;
      info.priority = p->priority;
      info.sz = p->sz;
      safestrcpy(info.name, p->name, sizeof(info.name));

      if(copyout(self->pagetable,
                 addr + (uint64)n * sizeof(info),
                 (char *)&info, sizeof(info)) < 0){
        release(&p->lock);
        return -1;
      }
      n++;
    }
    release(&p->lock);
  }
  return n;
}
