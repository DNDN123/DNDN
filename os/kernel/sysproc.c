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
#include "procinfo.h"   // === haneol: sys_ps ===
#include "sysinfo.h"    // === haneol: sys_sysinfo ===

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

// ============================================================
// === minju: 시스템콜 추적 모듈 ===
// ============================================================
// trace_on: 현재 프로세스의 추적을 시작. 통계는 0으로 리셋.
uint64
sys_trace_on(void)
{
  struct proc *p = myproc();
  memset(p->syscall_count,  0, sizeof(p->syscall_count));
  memset(p->syscall_errors, 0, sizeof(p->syscall_errors));
  p->traced = 1;
  return 0;
}

// trace_off: 추적 종료. 통계는 보존.
uint64
sys_trace_off(void)
{
  struct proc *p = myproc();
  p->traced = 0;
  return 0;
}

// trace_stats: 현재 프로세스 통계를 user 버퍼로 복사.
// 사용자 측 struct trace_stats (user.h) 와 레이아웃이 동일해야 함.
uint64
sys_trace_stats(void)
{
  struct proc *p = myproc();
  uint64 user_addr;
  argaddr(0, &user_addr);

  // K2 fix: array size 32 → 64 (must match struct proc + struct trace_stats)
  struct {
    int    traced;
    uint64 syscall_count[64];
    uint64 syscall_errors[64];
  } snapshot;

  snapshot.traced = p->traced;
  memmove(snapshot.syscall_count,  p->syscall_count,
          sizeof(snapshot.syscall_count));
  memmove(snapshot.syscall_errors, p->syscall_errors,
          sizeof(snapshot.syscall_errors));

  if(copyout(p->pagetable, user_addr,
             (char *)&snapshot, sizeof(snapshot)) < 0) {
    return -1;
  }
  return 0;
}

// ============================================================
// === jinhwan: Thread / Futex syscalls ===
// ============================================================
uint64
sys_thread_create(void)
{
  uint64 fcn, arg, stack;
  argaddr(0, &fcn);
  argaddr(1, &arg);
  argaddr(2, &stack);
  return kthread_create(fcn, arg, stack);
}

uint64
sys_thread_join(void)
{
  int tid;
  argint(0, &tid);
  return kthread_join(tid);
}

uint64
sys_thread_exit(void)
{
  kexit(0);
  return 0;   // not reached
}

uint64
sys_futex_wait(void)
{
  uint64 addr;
  int expected;
  argaddr(0, &addr);
  argint(1, &expected);
  return kfutex_wait(addr, expected);
}

uint64
sys_futex_wake(void)
{
  uint64 addr;
  argaddr(0, &addr);
  return kfutex_wake(addr);
}

// ============================================================
// === haneol: sysinfo + ps syscalls ===
// ============================================================
extern struct proc proc[NPROC];

uint64
sys_sysinfo(void)
{
  uint64 addr;
  struct sysinfo info;
  struct proc *p = myproc();

  argaddr(0, &addr);

  info.freemem = freemem_count();
  info.nproc   = proc_count();

  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
    return -1;
  return 0;
}

// sys_ps(buf, max) — fill user-side struct procinfo[max] with active procs.
// Returns the number of entries written, or -1 on error.
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
      info.pid      = p->pid;
      info.ppid     = p->parent ? p->parent->pid : 0;
      info.state    = (int)p->state;
      info.priority = p->priority;   // MLFQ queue level 0..2 (hyunsung)
      info.sz       = p->sz;
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

// sys_reap() — force-reap abandoned zombie processes. Returns the count reaped.
uint64
sys_reap(void)
{
  return proc_reap_zombies();
}
