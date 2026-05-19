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

// ============================================================
// === 시스템콜 추적 모듈 ===
// ============================================================
// 사용자에게 노출되는 추적 제어용 syscall 3개.
//
// 설계 원칙:
//   - "자기 자신의 통계만 조회" — 다른 프로세스 추적은 별도 syscall로 분리해야
//     proc 테이블 락 문제가 생기지 않음. 이번 모듈 범위 밖.
//   - 통계 데이터는 copyout으로 안전하게 사용자 공간 복사
//     (사용자가 잘못된 포인터를 넘겨도 커널 패닉 안 남)

// trace_on: 현재 프로세스의 추적을 시작.
// 통계는 0으로 리셋해서 "지금부터 측정 시작" 의미 분명히 함.
uint64
sys_trace_on(void)
{
  struct proc *p = myproc();

  memset(p->syscall_count,  0, sizeof(p->syscall_count));
  memset(p->syscall_errors, 0, sizeof(p->syscall_errors));
  p->traced = 1;
  return 0;
}

// trace_off: 추적을 종료. 통계는 보존(꺼진 뒤에도 stats로 읽을 수 있음).
uint64
sys_trace_off(void)
{
  struct proc *p = myproc();
  p->traced = 0;
  return 0;
}

// trace_stats: 현재 프로세스의 통계를 사용자 버퍼로 복사.
// 사용자 코드:
//   struct trace_stats st;
//   trace_stats(&st);
//
// 인자는 사용자 공간의 struct trace_stats* (포인터 한 개).
// 커널/사용자 메모리 경계를 안전하게 넘으려고 copyout 사용.
uint64
sys_trace_stats(void)
{
  struct proc *p = myproc();
  uint64 user_addr;

  // a0 레지스터에 실린 사용자 포인터 가져오기
  argaddr(0, &user_addr);

  // 사용자에게 보낼 스냅샷.
  // 사용자 측 user/user.h 의 struct trace_stats 와 레이아웃이 동일해야 함.
  struct {
    int    traced;
    uint64 syscall_count[32];
    uint64 syscall_errors[32];
  } snapshot;

  snapshot.traced = p->traced;
  memmove(snapshot.syscall_count,  p->syscall_count,
          sizeof(snapshot.syscall_count));
  memmove(snapshot.syscall_errors, p->syscall_errors,
          sizeof(snapshot.syscall_errors));

  // copyout: 커널 → 사용자 페이지 테이블 통해 안전 복사.
  // 사용자가 잘못된 주소를 줬으면 -1 반환.
  if(copyout(p->pagetable, user_addr,
             (char *)&snapshot, sizeof(snapshot)) < 0) {
    return -1;
  }
  return 0;
}
