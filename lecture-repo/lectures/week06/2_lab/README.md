# Week 6 Lab: CPU Scheduling - Understanding Context Switching

> Duration: ~50 minutes
> Prerequisites: Concepts of process states, registers, and stacks
> Requirements: xv6-riscv build environment (QEMU + RISC-V toolchain)

## Learning Objectives

- Understand xv6's context switch mechanism at the assembly level.
- Grasp the operating principles of the `scheduler()` function and observe round-robin scheduling.
- Trace how the `sleep`/`wakeup` mechanism actually switches between processes.

---

## Exercise 1: Analyzing swtch.S Code (~10 min)

### Background

In xv6, context switching is implemented in a single assembly file called `swtch.S`. This function saves the registers of the currently running kernel thread and restores the registers of the next kernel thread to be executed.

### Examining the context Struct

Open `kernel/proc.h` and examine `struct context`:

```c
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
```

**Question**: Why does it only save `ra` (return address), `sp` (stack pointer), and callee-saved registers (s0-s11)? Why aren't caller-saved registers (t0-t6, a0-a7) saved?

**Answer**: `swtch` is called following the C calling convention. Caller-saved registers have already been saved on the stack by the caller, so `swtch` only needs to save the callee-saved registers. This is an optimization that leverages the RISC-V calling convention.

### Analyzing swtch.S

Open `kernel/swtch.S`:

```asm
# Context switch
#
#   void swtch(struct context *old, struct context *new);
#
# Save current registers in old. Load from new.

.globl swtch
swtch:
        sd ra, 0(a0)        # old->ra = ra
        sd sp, 8(a0)        # old->sp = sp
        sd s0, 16(a0)       # old->s0 = s0
        sd s1, 24(a0)       # old->s1 = s1
        sd s2, 32(a0)       # old->s2 = s2
        sd s3, 40(a0)       # old->s3 = s3
        sd s4, 48(a0)       # old->s4 = s4
        sd s5, 56(a0)       # old->s5 = s5
        sd s6, 64(a0)       # old->s6 = s6
        sd s7, 72(a0)       # old->s7 = s7
        sd s8, 80(a0)       # old->s8 = s8
        sd s9, 88(a0)       # old->s9 = s9
        sd s10, 96(a0)      # old->s10 = s10
        sd s11, 104(a0)     # old->s11 = s11

        ld ra, 0(a1)        # ra = new->ra
        ld sp, 8(a1)        # sp = new->sp
        ld s0, 16(a1)       # s0 = new->s0
        ld s1, 24(a1)       # s1 = new->s1
        ld s2, 32(a1)       # s2 = new->s2
        ld s3, 40(a1)       # s3 = new->s3
        ld s4, 48(a1)       # s4 = new->s4
        ld s5, 56(a1)       # s5 = new->s5
        ld s6, 64(a1)       # s6 = new->s6
        ld s7, 72(a1)       # s7 = new->s7
        ld s8, 80(a1)       # s8 = new->s8
        ld s9, 88(a1)       # s9 = new->s9
        ld s10, 96(a1)      # s10 = new->s10
        ld s11, 104(a1)     # s11 = new->s11

        ret                  # pc = ra (jump to new->ra)
```

### Key Understanding Points

1. **`a0`** = first argument = `old` context pointer (current process)
2. **`a1`** = second argument = `new` context pointer (next process)
3. **`sd`** (store doubleword): saves register value to memory
4. **`ld`** (load doubleword): restores value from memory to register
5. **`ret`**: jumps to the address in the `ra` register -- at this point, `ra` has already been replaced with `new->ra`!

### Lab Assignment

Draw the following diagram on paper. Illustrate step by step the process of switching from Process A to Process B.

```
Process A (running)            scheduler              Process B (runnable)
     |                          |                         |
     | yield()                  |                         |
     |   sched()                |                         |
     |     swtch(A->ctx,        |                         |
     |           cpu->ctx) ---->|                         |
     |                          | (A's registers saved)   |
     |                          |                         |
     |                          | scheduler loop...       |
     |                          | Found B (RUNNABLE)      |
     |                          |                         |
     |                          | swtch(cpu->ctx,         |
     |                          |       B->ctx) --------->|
     |                          |                         | (B's registers restored)
     |                          |                         | ret -> returns to the
     |                          |                         |        point where B last
     |                          |                         |        called sched()
```

**Question**: Where does execution jump to when the `ret` instruction of `swtch` is executed?

**Answer**: It jumps to the address stored in `new->ra`. It returns to the line right after the `swtch` call inside the `sched()` function, where the process previously called `swtch`. For a newly created process, it jumps to the start address of the `forkret()` function (because `allocproc` sets `p->context.ra = (uint64)forkret`).

---

## Exercise 2: Adding printf to scheduler() (~15 min)

### Goal

Directly observe the order in which the scheduler selects processes.

### Applying the Patch

Apply the provided `scheduler_trace.patch` file to the xv6-riscv directory:

```bash
cd /path/to/xv6-riscv
git apply /path/to/materials/week6/lab/scheduler_trace.patch
```

This patch adds the following code to the `scheduler()` function:

```c
// kernel/proc.c - inside the scheduler() function

if(p->state == RUNNABLE) {
  // === Added trace code ===
  printf("[sched] cpu%d: switch to pid=%d name=%s\n",
         cpuid(), p->pid, p->name);
  // ===========================

  p->state = RUNNING;
  c->proc = p;
  swtch(&c->context, &p->context);
  c->proc = 0;
  found = 1;
}
```

### Manually Modifying Instead of Applying the Patch

Instead of using the patch, you can directly edit `kernel/proc.c`. Find the `scheduler()` function and add the `printf` inside the `if(p->state == RUNNABLE)` block:

```c
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    intr_on();
    intr_off();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Add here!
        printf("[sched] cpu%d: switch to pid=%d name=%s\n",
               cpuid(), p->pid, p->name);

        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      asm volatile("wfi");
    }
  }
}
```

### Build and Run

```bash
make clean
make qemu
```

### Expected Output

When xv6 boots, messages like the following will appear:

```
[sched] cpu0: switch to pid=1 name=
[sched] cpu0: switch to pid=1 name=initcode
[sched] cpu0: switch to pid=1 name=init
[sched] cpu0: switch to pid=2 name=sh
[sched] cpu0: switch to pid=2 name=sh
...
```

**Note**: When there are multiple CPUs (default QEMU configuration uses CPUS=3), output from different CPUs will be interleaved. If there is too much output, build with `CPUS=1`:

```bash
make clean
make CPUS=1 qemu
```

### Observation Points

1. `pid=1` is the `init` process. It is the first process scheduled at boot time.
2. When `init` forks `sh` (shell), `sh` with `pid=2` appears.
3. When the shell is waiting for key input, it is in the SLEEPING state and will not be scheduled.

### Lab Assignment

Observe the `printf` output and answer the following questions:

1. Which process is scheduled first right after booting?
2. After the shell prompt appears, does the scheduler output stop if you don't press any key? Why?
3. How does the output differ between `CPUS=1` and `CPUS=3`?

---

## Exercise 3: Observing Round-Robin Behavior (~15 min)

### Goal

Observe how round-robin scheduling works when multiple processes are running simultaneously.

### Test Method

In the xv6 shell, run the following command to execute multiple CPU-bound processes concurrently:

```
$ spin &; spin &; spin &
```

**Note**: `spin` is a simple program that runs an infinite loop. If xv6 doesn't have `spin`, you can create a simple user program as follows. (You can skip this step and run existing commands like `cat README` in the background instead.)

Alternatively, use existing commands:

```
$ cat README &
$ cat README &
$ ls &
```

### Expected Output

With scheduler tracing enabled, when multiple processes are RUNNABLE simultaneously:

```
[sched] cpu0: switch to pid=3 name=spin
[sched] cpu0: switch to pid=4 name=spin
[sched] cpu0: switch to pid=5 name=spin
[sched] cpu0: switch to pid=3 name=spin
[sched] cpu0: switch to pid=4 name=spin
[sched] cpu0: switch to pid=5 name=spin
...
```

### Observation Points

The scheduler iterates through the `proc[]` array from beginning to end, looking for RUNNABLE processes:

```c
for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == RUNNABLE) {
        // Run this process
    }
    release(&p->lock);
}
```

This is the simplest form of **round-robin scheduling**:
- Iterate through the array in order
- Run a process when a RUNNABLE one is found
- Return to the scheduler via timer interrupt
- Iterate from the beginning of the array again

### Lab Assignment

1. Observe the pattern of PIDs in the trace output. Is it truly a round-robin pattern?
2. What are the drawbacks of this implementation? (Hint: Do processes at the beginning of the `proc[]` array have an advantage?)
3. **Challenge**: Modify the `scheduler()` function to start iterating from the position after the last executed process. (Hint: You need a variable to remember the last index.)

---

## Exercise 4: Tracing sleep/wakeup Behavior (~10 min)

### Goal

Trace how `sleep` and `wakeup` work when a read on a pipe blocks.

### Understanding the sleep/wakeup Mechanism

Let's examine the `sleep()` function in `kernel/proc.c`:

```c
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  // Go to sleep.
  p->chan = chan;          // Record which event to wait for
  p->state = SLEEPING;    // Change state to SLEEPING

  sched();                // Switch to scheduler (yield CPU)

  // Woken up here (by wakeup)
  p->chan = 0;

  release(&p->lock);
  acquire(lk);
}
```

The `wakeup()` function:

```c
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;  // Wake up and make runnable again
      }
      release(&p->lock);
    }
  }
}
```

### sleep/wakeup in Pipes

Let's look at the `piperead()` function in `kernel/pipe.c`:

```c
int
piperead(struct pipe *pi, uint64 addr, int n)
{
  int i;
  struct proc *pr = myproc();
  char ch;

  acquire(&pi->lock);
  while(pi->nread == pi->nwrite && pi->writeopen){  // If no data to read
    if(killed(pr)){
      release(&pi->lock);
      return -1;
    }
    sleep(&pi->nread, &pi->lock);   // sleep! (channel: &pi->nread)
  }
  // ... read data ...
  wakeup(&pi->nwrite);              // Wake up the writer
  release(&pi->lock);
  return i;
}
```

### Tracing Exercise

Add the following printf statements to the `sleep()` function (edit directly):

```c
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  // Added trace
  printf("[sleep] pid=%d name=%s chan=%p\n", p->pid, p->name, chan);

  p->chan = chan;
  p->state = SLEEPING;
  sched();
  p->chan = 0;

  // Added trace
  printf("[wakeup] pid=%d name=%s\n", p->pid, p->name);

  release(&p->lock);
  acquire(lk);
}
```

### Test

In the xv6 shell, run a command that uses a pipe:

```
$ echo hello | cat
```

### Expected Output

```
[sleep] pid=3 name=cat chan=0x________
[sched] cpu0: switch to pid=4 name=echo
[wakeup] pid=3 name=cat
[sched] cpu0: switch to pid=3 name=cat
hello
```

Flow interpretation:
1. `cat` tries to read from the pipe, but there is no data yet, so it calls `sleep`
2. The scheduler runs `echo`
3. `echo` writes "hello" to the pipe and calls `wakeup(&pi->nread)`
4. `cat` wakes up, reads the data, and prints it

### Lab Assignment

1. What does the `chan` address in the output represent?
2. After `wakeup` is called, does the woken process run immediately? Or must it go through the scheduler?
3. What is the role of the lock in `sleep` and `wakeup`? (Think about this in relation to the lost wakeup problem)

---

## Summary and Key Takeaways

| Concept | Key Points |
|------|------------|
| **swtch.S** | Only saves/restores callee-saved registers. Jumps to the new process's execution point via `ret` |
| **scheduler()** | Iterates through the proc array to find and run RUNNABLE processes (round-robin) |
| **yield/sched** | Called when a process yields the CPU. Changes state to RUNNABLE and calls swtch |
| **sleep/wakeup** | Similar to condition variables. Puts processes to sleep and wakes them based on channels (chan) |

## Reverting Changes

When the lab is finished, revert the modified code:

```bash
cd /path/to/xv6-riscv
git checkout kernel/proc.c
```

Or reverse-apply the patch:

```bash
git apply -R /path/to/materials/week6/lab/scheduler_trace.patch
```
