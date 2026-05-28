// Smart-MLFQ-xv6 modified syscall.c
// =================================
// Added: sys_setpri, sys_getstats, sys_settrace, sys_forkpri
// entries in the syscall table.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

void
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_pause(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_setpri(void);    // Smart-MLFQ
extern uint64 sys_getstats(void);  // Smart-MLFQ
extern uint64 sys_settrace(void);  // Smart-MLFQ (I2)
extern uint64 sys_forkpri(void);   // Smart-MLFQ (I4)
// === minju: per-syscall trace ===
extern uint64 sys_trace_on(void);
extern uint64 sys_trace_off(void);
extern uint64 sys_trace_stats(void);
// === jinhwan: thread / futex ===
extern uint64 sys_thread_create(void);
extern uint64 sys_thread_join(void);
extern uint64 sys_thread_exit(void);
extern uint64 sys_futex_wait(void);
extern uint64 sys_futex_wake(void);
// === haneol: process listing ===
extern uint64 sys_ps(void);
extern uint64 sys_sysinfo(void);

static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_pause]   sys_pause,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_setpri]   sys_setpri,     // Smart-MLFQ
[SYS_getstats] sys_getstats,    // Smart-MLFQ
[SYS_settrace] sys_settrace,    // Smart-MLFQ (I2)
[SYS_forkpri]  sys_forkpri,     // Smart-MLFQ (I4)
// === minju: per-syscall trace ===
[SYS_trace_on]    sys_trace_on,
[SYS_trace_off]   sys_trace_off,
[SYS_trace_stats] sys_trace_stats,
// === jinhwan: thread / futex ===
[SYS_thread_create] sys_thread_create,
[SYS_thread_join]   sys_thread_join,
[SYS_thread_exit]   sys_thread_exit,
[SYS_futex_wait]    sys_futex_wait,
[SYS_futex_wake]    sys_futex_wake,
// === haneol: process listing ===
[SYS_ps]      sys_ps,
[SYS_sysinfo] sys_sysinfo,
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();
  uint64 ret;

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // === minju: per-syscall trace hook ===
    // 모든 syscall이 이 한 지점을 거치므로, 여기서 카운트만 하면
    // 새 syscall이 추가돼도 추적 코드를 손볼 필요가 없다.
    // 단, 추적 제어용 syscall 자체는 추적에서 제외 (관찰자 효과 방지).
    int trackable = (p->traced &&
                     num < 64 &&    // K2 fix: matches new array size
                     num != SYS_trace_on &&
                     num != SYS_trace_off &&
                     num != SYS_trace_stats);

    if(trackable) {
      p->syscall_count[num]++;
    }

    ret = syscalls[num]();
    p->trapframe->a0 = ret;

    // 반환값이 음수면 실패로 간주.
    if(trackable && (long)ret < 0) {
      p->syscall_errors[num]++;
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
