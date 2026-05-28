// Smart-MLFQ-xv6 modified proc.c
// =================================
// Changes from stock xv6:
//   * struct proc extended with priority + scheduling stats (see proc.h)
//   * scheduler() implements 3-level MLFQ with serialized periodic boost
//     (double-checked locking on boost_lock) and TRACE logging deferred
//     until AFTER swtch returns (no printf inside p->lock).
//   * trap.c (timer interrupt) updates time_slice_used + demotes
//   * sleep() retains queue level (I/O block does not demote); only
//     genuine I/O channels (not &ticks or proc[] addresses) bump
//     total_io_blocks.
//   * kexit() records completion_tick and emits EXIT trace
//   * allocproc() initializes MLFQ fields; trace_enabled defaults OFF.
//   * kfork() makes child inherit trace_enabled from parent.
//   * NEW: kforkpri(level) — fork variant that sets child's priority
//     atomically before it becomes RUNNABLE.
//   * NEW: proc_settrace(pid, on) — toggle TRACE/EXIT logging per proc.
//   * boost: every MLFQ_BOOST_INTERVAL ticks, all procs are reset to HIGH
//
// NOTE: This is a drop-in replacement for xv6-riscv kernel/proc.c.
// Only modified functions are commented; the rest are unchanged from
// xv6-riscv mainline.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

struct spinlock wait_lock;

// === Smart-MLFQ: per-level time slice limit (ticks) ===
const int mlfq_slice_limit[MLFQ_LEVELS] = { 2, 4, 8 };

// Global tracker for periodic priority boost.
// Protected by boost_lock to serialize boosts across multiple harts.
//
// LOCK ORDER: boost_lock -> p->lock.
// boost/aging passes acquire boost_lock first, then walk proc[] taking
// p->lock per entry. Any future code that touches both MUST obey this
// order; the reverse risks deadlock.
static int last_boost_tick = 0;
static struct spinlock boost_lock;

// === Smart-MLFQ: Aging — gentler than boost ===
// Every MLFQ_AGING_INTERVAL ticks we scan for RUNNABLE non-HIGH procs
// that have not been scheduled for MLFQ_AGE_THRESHOLD ticks and promote
// them by ONE level. This prevents starvation between boosts (which
// happen only every MLFQ_BOOST_INTERVAL ticks) and gives the system a
// continuous, finer-grained fairness signal than pure boost.
// We reuse boost_lock to serialize aging passes too.
static int last_aging_tick = 0;

// Allocate a page for each process's kernel stack.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

void
procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  initlock(&boost_lock, "mlfq_boost");   // === Smart-MLFQ ===
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

int
cpuid()
{
  int id = r_tp();
  return id;
}

struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // === Smart-MLFQ initialization ===
  p->priority = 0;             // start at HIGH
  p->time_slice_used = 0;
  p->total_run_ticks = 0;
  p->total_io_blocks = 0;
  p->arrival_tick = ticks;
  p->completion_tick = 0;
  p->trace_enabled = 0;        // off by default; user enables via settrace().
                               // kfork/kforkpri inherit parent's value.
  p->last_ran_tick = ticks;    // Aging baseline — newly created proc is "fresh".

  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  p->cwd = namei("/");
  p->state = RUNNABLE;

  release(&p->lock);
}

int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if(sz + n > TRAPFRAME) {
      return -1;
    }
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  if((np = allocproc()) == 0){
    return -1;
  }

  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  *(np->trapframe) = *(p->trapframe);
  np->trapframe->a0 = 0;

  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  // === Smart-MLFQ: child inherits parent's tracing setting ===
  np->trace_enabled = p->trace_enabled;

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// === MODIFIED ===
// Emit EXIT trace line before tearing down.
void
kexit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  // === Smart-MLFQ: record completion + emit trace ===
  p->completion_tick = ticks;
  if(p->trace_enabled){
    printf("EXIT pid=%d name=%s arrival=%d completion=%d run=%d ioblock=%d final_pri=%d\n",
           p->pid, p->name, p->arrival_tick, p->completion_tick,
           p->total_run_ticks, p->total_io_blocks, p->priority);
  }

  acquire(&wait_lock);

  reparent(p);

  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  sched();
  panic("zombie exit");
}

int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }

    sleep(p, &wait_lock);
  }
}

// === MODIFIED ===
// 3-level MLFQ scheduler.
//
// At each scheduling decision:
//   1. Check if we should do a periodic boost (every MLFQ_BOOST_INTERVAL
//      ticks). Boost is serialized across harts by boost_lock with a
//      double-checked locking pattern so only one CPU per interval
//      walks the proc[] array.
//   2. Walk levels 0 (HIGH) -> 1 (MID) -> 2 (LOW). For each level, find
//      a RUNNABLE proc at that level and switch to it. Break on first
//      match.
//   3. Repeat forever.
//
// The chosen proc runs until it yields (timer interrupt) or sleeps
// (I/O block). On yield, trap.c has already updated time_slice_used
// and possibly demoted the proc.
//
// TRACE log lines are emitted AFTER swtch returns (i.e. after the
// chosen proc has finished its quantum and given the CPU back to us).
// This avoids holding p->lock across a synchronous UART write, which
// would distort scheduling timing and corrupt our measurements.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();
    intr_off();

    // --- Smart-MLFQ: periodic boost (double-checked, serialized) ---
    // Fast-path read is racy but the inner check after acquiring
    // boost_lock is authoritative. We hold boost_lock for the entire
    // boost so concurrent harts that find the inner check false will
    // simply release and continue.
    if(ticks - last_boost_tick >= MLFQ_BOOST_INTERVAL){
      acquire(&boost_lock);
      if(ticks - last_boost_tick >= MLFQ_BOOST_INTERVAL){
        for(p = proc; p < &proc[NPROC]; p++){
          acquire(&p->lock);
          if(p->state != UNUSED){
            p->priority = 0;
            p->time_slice_used = 0;
            p->last_ran_tick = ticks;  // boost also resets aging timer
          }
          release(&p->lock);
        }
        last_boost_tick = ticks;
      }
      release(&boost_lock);
    }

    // --- Smart-MLFQ: aging pass (between boosts, finer-grained) -------
    // Every MLFQ_AGING_INTERVAL ticks promote any RUNNABLE non-HIGH proc
    // that has been starved for MLFQ_AGE_THRESHOLD ticks. Unlike boost
    // this is non-destructive: only one level up, and only for procs
    // that genuinely haven't run.
    if(ticks - last_aging_tick >= MLFQ_AGING_INTERVAL){
      acquire(&boost_lock);
      if(ticks - last_aging_tick >= MLFQ_AGING_INTERVAL){
        for(p = proc; p < &proc[NPROC]; p++){
          acquire(&p->lock);
          if(p->state == RUNNABLE && p->priority > 0 &&
             (ticks - p->last_ran_tick) >= MLFQ_AGE_THRESHOLD){
            p->priority--;             // gentle promotion
            p->time_slice_used = 0;
            p->last_ran_tick = ticks;  // give it a fresh chance
          }
          release(&p->lock);
        }
        last_aging_tick = ticks;
      }
      release(&boost_lock);
    }

    int found = 0;
    for(int level = 0; level < MLFQ_LEVELS && !found; level++){
      for(p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if(p->state == RUNNABLE && p->priority == level) {
          // Snapshot trace info to print AFTER swtch — see comment above.
          int log_trace = p->trace_enabled;
          int log_tick  = ticks;
          int log_pid   = p->pid;
          int log_pri   = p->priority;
          int log_slice = p->time_slice_used;

          p->state = RUNNING;
          p->last_ran_tick = ticks;   // Aging: reset starvation timer.
          c->proc = p;
          swtch(&c->context, &p->context);

          c->proc = 0;
          found = 1;
          release(&p->lock);

          // No locks held here — safe to call printf without affecting
          // the proc's measured run time (it already finished its
          // quantum above).
          if(log_trace){
            printf("TRACE tick=%d pid=%d pri=%d slice=%d\n",
                   log_tick, log_pid, log_pri, log_slice);
          }
          break;
        }
        release(&p->lock);
      }
    }

    if(found == 0) {
      asm volatile("wfi");
    }
  }
}

void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched RUNNING");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){ "/init", 0 });
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // === Compatibility fix for lecture-repo xv6 ===
  // Mainline MIT xv6 ends forkret with `usertrapret()`. The lecture-repo
  // variant uses an inline return-to-userspace sequence instead (no
  // `usertrapret` symbol exists in this kernel). Using the mainline
  // version here would fail to link with `undefined reference to
  // 'usertrapret'`. Below mirrors `usertrap()`'s return path.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// === MODIFIED ===
// On sleep (I/O block), reset time_slice_used so that when we return,
// we get a fresh slice at the same priority. This prevents I/O-bound
// processes from being penalized.
//
// Only count this as an I/O block if `chan` is *not* a kernel-internal
// wait channel. We exclude:
//   - &ticks      : sys_pause (timed sleep — not I/O)
//   - addresses inside proc[] : kwait waiting on a child to exit
// Everything else (pipe, console, disk, etc.) counts as real I/O and
// drives the LLM's priority recommendation via total_io_blocks.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  // === Smart-MLFQ: I/O block — retain queue, reset slice counter ===
  // Always reset slice (sleeping process keeps its priority on wakeup).
  // Only bump total_io_blocks for genuine I/O channels.
  int is_internal =
      (chan == (void*)&ticks) ||
      ((char*)chan >= (char*)proc &&
       (char*)chan <  (char*)(proc + NPROC));
  if(!is_internal){
    p->total_io_blocks++;
  }
  p->time_slice_used = 0;

  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = 0;

  release(&p->lock);
  acquire(lk);
}

void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

int
kkill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s pri=%d slice=%d", p->pid, state, p->name,
           p->priority, p->time_slice_used);
    printf("\n");
  }
}

// === Smart-MLFQ helpers — called from sysproc.c ===

// Returns 1 if p->state is "live" enough to receive priority/trace updates.
// Excludes UNUSED (slot not in use) and ZOMBIE (already exited; mutating
// it would only hide a host-side race).
static inline int
proc_is_live(struct proc *p)
{
  return p->state == USED      ||
         p->state == RUNNABLE  ||
         p->state == RUNNING   ||
         p->state == SLEEPING;
}

// Set a process's initial priority. Called by setpri syscall.
int
proc_setpri(int pid, int level)
{
  struct proc *p;

  if(level < 0 || level >= MLFQ_LEVELS) return -1;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && proc_is_live(p)){
      p->priority = level;
      p->time_slice_used = 0;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Enable or disable TRACE/EXIT logging for a process.
// pid == 0 means "current process".
// Returns 0 on success, -1 if pid not found.
int
proc_settrace(int pid, int on)
{
  struct proc *p;
  struct proc *me = myproc();

  if(pid == 0 && me){
    acquire(&me->lock);
    me->trace_enabled = on ? 1 : 0;
    release(&me->lock);
    return 0;
  }

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && proc_is_live(p)){
      p->trace_enabled = on ? 1 : 0;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Fill stats for a given pid. Returns 0 on success, -1 if pid not found.
int
proc_getstats(int pid, struct procstats *out)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->state != UNUSED){
      out->pid = p->pid;
      out->priority = p->priority;
      out->total_run_ticks = p->total_run_ticks;
      out->total_io_blocks = p->total_io_blocks;
      out->arrival_tick = p->arrival_tick;
      out->completion_tick = p->completion_tick;
      out->final_pri = p->priority;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Called by trap.c on each timer tick (only for the currently running proc).
// Increments time_slice_used and total_run_ticks; demotes if slice expired.
void
proc_on_timer_tick(void)
{
  struct proc *p = myproc();
  if(!p) return;

  acquire(&p->lock);
  p->time_slice_used++;
  p->total_run_ticks++;

  int limit = mlfq_slice_limit[p->priority];
  if(p->time_slice_used >= limit){
    if(p->priority < MLFQ_LEVELS - 1){
      p->priority++;  // demote one level
    }
    p->time_slice_used = 0;
  }
  release(&p->lock);
}

// === Smart-MLFQ: fork variant that sets the child's initial priority
// atomically, BEFORE the child can be scheduled. This avoids the
// race where workload_runner forks at HIGH (default) and the child
// runs a few ticks before the parent's setpri takes effect.
//
// This is a near-copy of kfork() with two additions:
//   * level validation up front
//   * np->priority = level set before np->state = RUNNABLE
int
kforkpri(int level)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  if(level < 0 || level >= MLFQ_LEVELS) return -1;

  if((np = allocproc()) == 0){
    return -1;
  }

  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  *(np->trapframe) = *(p->trapframe);
  np->trapframe->a0 = 0;

  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  // Inherit tracing, override priority. Set BEFORE RUNNABLE.
  np->trace_enabled = p->trace_enabled;
  np->priority = level;
  np->time_slice_used = 0;

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}
