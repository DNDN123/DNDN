# Week 3 Lab: Process 2 - Exploring the xv6 Internals

> **Duration**: approximately 50 minutes
> **Prerequisites**: C language basics, process concepts (Week 2)
> **Reference textbook**: xv6 textbook Ch 2 (OS Organization), Ch 4 (Traps and System Calls)

---

## Learning Objectives

1. Understand the source code structure of the xv6 operating system.
2. Trace the full path from a user program's system call invocation to the kernel.
3. Analyze each field of `struct proc`, which represents a process.
4. Read the kernel implementation of the `fork()` system call and understand how it works.

---

## Lab 0: Environment Setup & Verification (5 min)

### Prerequisites

xv6-riscv requires a RISC-V cross-compiler and QEMU. Install them for your platform:

**macOS (Homebrew)**:
```bash
brew install qemu riscv64-elf-gcc
```

**Ubuntu / Debian**:
```bash
sudo apt update
sudo apt install -y git build-essential qemu-system-misc \
    gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

**Windows**: Install [WSL2](https://learn.microsoft.com/en-us/windows/wsl/install) with Ubuntu (`wsl --install -d Ubuntu` in PowerShell as Administrator), then follow the Ubuntu instructions above inside the WSL terminal. **Important**: work in `~/`, not `/mnt/c/` — the latter is very slow.

**Clone xv6** (all platforms):
```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
```

> For detailed instructions, see the [Environment Setup Guide](../week02/2_lab/setup_xv6_env.md).
> See also: [MIT 6.1810 Tools Page](https://pdos.csail.mit.edu/6.828/2024/tools.html) · [xv6-riscv GitHub](https://github.com/mit-pdos/xv6-riscv)

### Build & Run

```bash
cd xv6-riscv
make clean
make qemu
```

You should see:
```
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$
```

Try `ls`, `echo hello` in the xv6 shell.
To exit: press `Ctrl-a`, then press `x`.

### Troubleshooting

| Problem | Fix |
|---------|-----|
| `Couldn't find a riscv64 version of GCC` | Set TOOLPREFIX manually: `make TOOLPREFIX=riscv64-linux-gnu- qemu` (Ubuntu) or `make TOOLPREFIX=riscv64-elf- qemu` (macOS) |
| QEMU version too old | xv6 requires QEMU 5.0+. Check with `qemu-system-riscv64 --version` |
| macOS Apple Silicon linker errors | Use `brew install riscv64-elf-gcc` (not the linux-gnu variant) |
| WSL2 "KVM not available" warning | Harmless — xv6 uses software emulation (TCG) and does not need KVM |

---

## Lab 1: Exploring the xv6 Source Code Structure (7 min)

### 1.1 Overall Directory Structure

The main directories in the xv6 repository are as follows.

| Directory | Description |
|-----------|-------------|
| `kernel/` | Kernel source code (process management, memory management, file system, trap handling, etc.) |
| `user/`   | User program source code (shell, cat, ls, etc.) |
| `mkfs/`   | File system image generation tool |

### 1.2 Key Files in the kernel/ Directory

```
kernel/
  ├── proc.h          # struct proc definition (process state)
  ├── proc.c          # Process management (fork, exit, wait, scheduler)
  ├── syscall.h       # System call number definitions
  ├── syscall.c       # System call dispatch (syscall() function)
  ├── sysproc.c       # Process-related system call implementations (fork, exit, wait, etc.)
  ├── sysfile.c       # File-related system call implementations (open, read, write, etc.)
  ├── trap.c          # Trap/interrupt handling (usertrap, kerneltrap)
  ├── trampoline.S    # User<->kernel mode transition assembly
  ├── vm.c            # Virtual memory management
  ├── kalloc.c        # Physical memory allocation (page granularity)
  ├── spinlock.c      # Spinlock implementation
  ├── main.c          # Kernel boot entry point
  ├── param.h         # System parameters (NPROC, NOFILE, etc.)
  ├── defs.h          # Collection of kernel function declarations
  ├── riscv.h         # RISC-V related registers/macros
  └── memlayout.h     # Memory layout constants
```

### 1.3 Key Files in the user/ Directory

```
user/
  ├── user.h          # System call declarations for user programs
  ├── usys.pl         # System call stub generation script (Perl)
  ├── ulib.c          # User library (strcmp, strlen, etc.)
  ├── printf.c        # User printf implementation
  ├── umalloc.c       # User malloc/free implementation
  ├── sh.c            # Shell
  ├── cat.c           # cat command
  ├── ls.c            # ls command
  ├── echo.c          # echo command
  └── ...             # Other utilities
```

### [Question] Describe the role of each of the following files in one sentence.

1. `kernel/proc.h` -
2. `kernel/syscall.c` -
3. `kernel/trap.c` -
4. `user/usys.pl` -

---

## Lab 2: Tracing the System Call Path (20 min)

We will trace what happens when a user program calls `fork()`, following the source code along the way.

### 2.1 The Call from User Space

User programs call functions declared in `user/user.h`.

```c
// user/user.h (excerpt)

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
// ... etc.
```

**Key point**: The declaration of `fork()` exists, but there is no C implementation of `fork()` anywhere in the `user/` directory. So where is the actual code?

### 2.2 System Call Stub Generation: `user/usys.pl`

```perl
#!/usr/bin/perl -w

# Generate usys.S, the stubs for syscalls.

print "# generated by usys.pl - do not edit\n";

print "#include \"kernel/syscall.h\"\n";

sub entry {
    my $prefix = "sys_";
    my $name = shift;
    if ($name eq "sbrk") {
	print ".global $prefix$name\n";
	print "$prefix$name:\n";
    } else {
	print ".global $name\n";
	print "$name:\n";
    }
    print " li a7, SYS_${name}\n";
    print " ecall\n";
    print " ret\n";
}

entry("fork");
entry("exit");
entry("wait");
# ... remaining system calls
```

This Perl script automatically generates the `user/usys.S` assembly file during the build.

**Generated assembly (for fork)**:

```asm
.global fork
fork:
 li a7, SYS_fork      # Load system call number (1) into register a7
 ecall                 # RISC-V environment call instruction -> trap into kernel
 ret                   # Return after coming back from kernel
```

**Key points**:
- The system call number is placed in the `a7` register
- The `ecall` instruction triggers a trap, causing the CPU to switch from user mode to kernel mode
- For system calls with arguments, the arguments are passed via registers `a0`~`a5`
- The return value comes back in the `a0` register

### 2.3 System Call Numbers: `kernel/syscall.h`

```c
// kernel/syscall.h

// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_pause  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21
```

Each system call is assigned a unique number. `fork` is number 1.

### 2.4 Trap Handling: What Happens After `ecall`

When the `ecall` instruction is executed, the following sequence occurs:

```
[User Space]                          [Kernel Space]

 fork()
   |
   v
 li a7, SYS_fork
 ecall -------------------------------->  trampoline.S: uservec
                                              |
                                              | (save registers, switch to kernel stack)
                                              v
                                          trap.c: usertrap()
                                              |
                                              | (scause == 8 -> system call)
                                              v
                                          syscall.c: syscall()
                                              |
                                              | (read number from a7, table lookup)
                                              v
                                          sysproc.c: sys_fork()
                                              |
                                              v
                                          proc.c: kfork()
                                              |
                                              | (actual process duplication)
                                              v
                                          store return value in a0
                                              |
 ret <----------------------------------  trampoline.S: userret
   |
   v
 (use fork return value)
```

### 2.5 `usertrap()` - Determining the Trap Type

```c
// kernel/trap.c (excerpt)

uint64
usertrap(void)
{
  // ...
  struct proc *p = myproc();

  // save user program counter.
  p->trapframe->epc = r_sepc();

  if(r_scause() == 8){
    // system call
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok (device interrupt)
  } else {
    // unexpected trap
    // ...
  }
  // ...
}
```

**Key points**:
- `scause == 8` means a system call (`ecall from U-mode`)
- `epc += 4`: advance the PC by 4 bytes to return to the instruction after `ecall`
- Then calls the `syscall()` function

### 2.6 `syscall()` - The Dispatch Function

```c
// kernel/syscall.c (excerpt)

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
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
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

**Key points**:
- `syscalls[]` is a **function pointer array**. Indexing with the system call number yields the corresponding implementation function.
- The system call number is read from `p->trapframe->a7` (the value the user placed in `a7`).
- The function is called and its return value is stored in `p->trapframe->a0` (the value to return to the user).

### 2.7 Implementation Functions: `sys_fork()` and `kfork()`

```c
// kernel/sysproc.c
uint64
sys_fork(void)
{
  return kfork();
}

// kernel/proc.c
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // ... (copy file descriptors, copy name, etc.)

  np->state = RUNNABLE;
  return pid;
}
```

### [Exercise] Fill in the blanks below.

When a user calls `fork()`:

1. In `user/usys.S`, the number `_____` is placed in register `a7` and the `_____` instruction is executed.
2. The CPU switches from _____ mode to _____ mode.
3. The `_____()` function in `kernel/trap.c` is called and confirms that `scause == _____`.
4. In `kernel/syscall.c`, the `_____()` function calls `_____()` via `syscalls[_____]`.
5. Finally, the actual process duplication is performed in the `_____()` function in `kernel/proc.c`.

---

## Lab 3: Analyzing `struct proc` (10 min)

### 3.1 Process State

```c
// kernel/proc.h

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
```

Process lifecycle:

```
  allocproc()         Selected by
      |                scheduler
      v                    |
   UNUSED --> USED --> RUNNABLE --> RUNNING
                           ^            |
                           |            | yield() / interrupt
                           +------------+
                                        |
                           sleep()      |  exit()
                              |         |    |
                              v         |    v
                          SLEEPING      |  ZOMBIE
                              |         |    |
                           wakeup()     |  parent's wait()
                              |         |    |
                              +---------+    v
                                          UNUSED (freeproc)
```

### 3.2 Full Structure of `struct proc`

```c
// kernel/proc.h

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
};
```

### 3.3 Role of Each Field

| Field | Type | Description |
|-------|------|-------------|
| `lock` | `struct spinlock` | Spinlock for protecting the process structure |
| `state` | `enum procstate` | Current process state (UNUSED, RUNNABLE, RUNNING, etc.) |
| `chan` | `void *` | Channel on which process is sleeping (acts as a condition variable) |
| `killed` | `int` | If 1, marked for termination (will exit at next trap) |
| `xstate` | `int` | Exit status value (passed to parent's `wait()`) |
| `pid` | `int` | Process ID (unique identifier) |
| `parent` | `struct proc *` | Pointer to parent process |
| `kstack` | `uint64` | Virtual address of kernel stack |
| `sz` | `uint64` | Size of user memory (bytes) |
| `pagetable` | `pagetable_t` | User page table |
| `trapframe` | `struct trapframe *` | Storage area for user registers during traps |
| `context` | `struct context` | Kernel registers for context switching |
| `ofile[NOFILE]` | `struct file *[]` | Open file table (NOFILE=16) |
| `cwd` | `struct inode *` | Current working directory |
| `name[16]` | `char[]` | Process name (for debugging) |

### 3.4 trapframe vs context

Both structures save registers, but they serve different purposes:

- **`trapframe`**: Saves/restores **user registers** during user mode <-> kernel mode transitions
  - When a trap occurs via `ecall`, trampoline.S saves all user registers here
  - When returning from kernel to user, registers are restored from here

- **`context`**: Saves/restores kernel registers during **context switches** between kernel threads
  - The `swtch()` function saves only callee-saved registers (ra, sp, s0-s11)
  - Used for switching between processes and the scheduler

### [Exercise] Answer the following questions.

1. What is the maximum number of processes that can exist simultaneously? (Hint: `NPROC` in `kernel/param.h`)
2. What is the maximum number of files a single process can have open at once?
3. When a child process is created with `fork()`, what happens to the parent's open files?
4. After a process calls `exit()` but before it is `wait()`ed on, what state is it in?

---

## Lab 4: Reading the `fork()` Implementation Code (10 min)

### 4.1 Full Analysis of `kfork()`

We will analyze the `kfork()` function in `kernel/proc.c` line by line.

```c
// kernel/proc.c

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();          // (1) Get current (parent) process

  // Allocate process.
  if((np = allocproc()) == 0){        // (2) Allocate a new process slot
    return -1;                        //     Return -1 on failure
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);                     // (3) Copy parent's memory to child
    release(&np->lock);               //     Clean up and return -1 on failure
    return -1;
  }
  np->sz = p->sz;                     // (4) Set memory size to be the same

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe); // (5) Copy all user registers

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;             // (6) Child's return value = 0

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)         // (7) Copy open file descriptors
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);            // (8) Copy current directory

  safestrcpy(np->name, p->name, sizeof(p->name));  // (9) Copy process name

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;                     // (10) Set parent-child relationship
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;               // (11) Set child to runnable state
  release(&np->lock);

  return pid;                         // (12) Return child's PID to parent
}
```

### 4.2 How fork() Returns Twice

The idea that `fork()` "returns twice" is one of the most confusing concepts when first learning xv6.

1. **Parent process**: `kfork()` returns the child's PID via `return pid;`
2. **Child process**: Due to `np->trapframe->a0 = 0;`, when the child returns to user mode, the `a0` register is 0, so `fork()`'s return value is 0

```
[Parent]                              [Child]
kfork() {                             (not yet running)
  ...
  np->trapframe->a0 = 0;
  np->state = RUNNABLE;               Selected by scheduler
  return pid; --> fork()=pid               |
}                                          v
                                    forkret() -> userret
                                        |
                                        v
                                    a0=0 --> fork()=0
```

### 4.3 `allocproc()` - Process Slot Allocation

```c
// kernel/proc.c

static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {  // Iterate over entire process table
    acquire(&p->lock);
    if(p->state == UNUSED) {               // Find an UNUSED slot
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;                                // No free slot available

found:
  p->pid = allocpid();                     // Allocate PID
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);        // Create page table
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;         // When first scheduled, start at forkret()
  p->context.sp = p->kstack + PGSIZE;      // Top of kernel stack

  return p;
}
```

**Key points**:
- xv6 manages the process table as a `proc[NPROC]` array (maximum 64 entries).
- `context.ra = forkret`: When a new process is first scheduled, `swtch()` jumps to `forkret()`.

### [Exercise] Read the code and answer the following questions.

1. What would happen if you removed `np->trapframe->a0 = 0;` from `kfork()`?
2. Why is the `for(i = 0; i < NOFILE; i++)` loop necessary? What would happen if file descriptors were not copied?
3. What problems would arise if you changed `np->state = RUNNABLE;` to `RUNNING`?
4. What does `context.ra = (uint64)forkret;` mean in `allocproc()`?

---

## Summary: Complete System Call Flow

```
+----------------------------------------------------------+
| User Space                                                |
|                                                           |
|  user program:  fork()                                    |
|       |                                                   |
|       v                                                   |
|  usys.S:  li a7, SYS_fork                                |
|           ecall                                           |
|       |                                                   |
|-------|---------------------------------------------------|
|       v  [TRAP: user -> kernel mode switch]               |
| Kernel Space                                              |
|                                                           |
|  trampoline.S: uservec (save registers, switch to         |
|       |                  kernel stack)                     |
|       v                                                   |
|  trap.c: usertrap()  <- scause == 8 (system call)        |
|       |                                                   |
|       v                                                   |
|  syscall.c: syscall()                                     |
|    num = trapframe->a7;         <- read syscall number    |
|    trapframe->a0 = syscalls[num]();  <- call + ret value  |
|       |                                                   |
|       v                                                   |
|  sysproc.c: sys_fork() -> proc.c: kfork()                 |
|       |                                                   |
|       v                                                   |
|  trampoline.S: userret (restore registers, return to      |
|       |                  user mode)                        |
|-------|---------------------------------------------------|
|       v                                                   |
| User Space                                                |
|  fork() returns (a0 = pid or 0)                           |
+----------------------------------------------------------+
```

---

## Supplementary Learning Resources

- xv6 textbook Chapter 2: Operating System Organization
- xv6 textbook Chapter 4: Traps and System Calls
- RISC-V Privileged Specification - sections on `ecall`, `sret`, `scause`
