// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// === Smart-MLFQ: priority levels and time slice limits ===
// 0 = HIGH (short slice, interactive)
// 1 = MID  (medium slice)
// 2 = LOW  (long slice, CPU-bound)
#define MLFQ_LEVELS 3
#define MLFQ_BOOST_INTERVAL 100  // ticks — full reset of all procs to HIGH
#define MLFQ_AGING_INTERVAL 25   // ticks — between aging passes
#define MLFQ_AGE_THRESHOLD  30   // ticks of not running before promotion
// time slice per level (in clock ticks)
extern const int mlfq_slice_limit[MLFQ_LEVELS];

// Stats struct returned by getstats() syscall.
struct procstats {
  int pid;
  int priority;          // current queue level (0/1/2)
  int total_run_ticks;   // accumulated CPU time
  int total_io_blocks;   // number of sleep() invocations
  int arrival_tick;
  int completion_tick;   // 0 if still running
  int final_pri;         // priority at exit time
};

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  // === Smart-MLFQ fields ===
  int priority;          // current queue level: 0/1/2
  int time_slice_used;   // ticks consumed in current scheduling burst
  int total_run_ticks;   // accumulated CPU time
  int total_io_blocks;   // count of sleep() calls (I/O block proxy)
  int arrival_tick;      // tick at which proc was created
  int completion_tick;   // tick at which proc exited (0 if running)
  int trace_enabled;     // 1 = emit TRACE/EXIT log lines
  int last_ran_tick;     // tick of last time scheduler picked this proc;
                         //   used by Aging to promote starving RUNNABLEs.
};
