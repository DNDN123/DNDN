# Week 3 Homework: Adding the trace System Call

> **Deadline**: Before the next class
> **Based on**: MIT 6.1810 Lab: System Calls
> **Difficulty**: Required (trace) + Bonus (sysinfo)
> **Environment**: xv6-riscv (QEMU + RISC-V toolchain required — see [Week 2 setup guide](../../week02/2_lab/setup_xv6_env.md))

---

## Overview

In this assignment, you will add a new system call `trace(int mask)` to xv6.
Through this process, you will experience firsthand how system calls are registered and what path they take from user programs to the kernel.

---

## Assignment 1: `trace` System Call (Required)

### Functional Specification

The `trace(int mask)` system call enables system call tracing for the calling process.

- **mask**: A bitmask of the system calls to trace. To trace system call number `n`, set bit `(1 << n)`.
- Each time a traced system call is invoked, the kernel prints a line in the following format:

  ```
  <pid>: syscall <syscall_name> -> <return_value>
  ```

  Example:
  ```
  3: syscall fork -> 4
  4: syscall write -> 1
  ```

- Child processes created by `fork()` **inherit** the parent's trace mask.
- Calling `trace(0)` disables tracing.

### Usage Example

```c
// Calling trace(1 << SYS_fork) traces the fork system call
// A log is printed each time fork is called

$ trace 1 fork    // mask = (1 << SYS_fork) = 2
3: syscall fork -> 4

$ trace 5 read write  // mask = (1<<SYS_read)|(1<<SYS_write) = 0x10020
// A log is printed each time read or write is called
```

---

## Step-by-Step Implementation Guide

### List of Files to Modify

| File | Modification |
|------|-------------|
| `kernel/syscall.h` | Add `SYS_trace` number (22) |
| `user/usys.pl` | Add `trace` entry |
| `user/user.h` | Add `trace()` function declaration |
| `kernel/proc.h` | Add `trace_mask` field to `struct proc` |
| `kernel/sysproc.c` | Implement `sys_trace()` function |
| `kernel/syscall.c` | Add to syscalls table, add trace logic to `syscall()` |
| `kernel/proc.c` | Copy `trace_mask` in `kfork()` |
| `Makefile` | Add test program to UPROGS (optional) |

### Step 1: Register the System Call Number

Add the new system call number to `kernel/syscall.h`.

```c
// kernel/syscall.h
// Last existing line:
#define SYS_close  21

// Add:
#define SYS_trace  22
```

### Step 2: User Space Interface

Add an entry at the end of the **`user/usys.pl`** file:

```perl
entry("trace");
```

This will cause the build to generate the following assembly in `usys.S`:

```asm
.global trace
trace:
 li a7, SYS_trace
 ecall
 ret
```

Add a function declaration to **`user/user.h`**:

```c
// Add to the system calls section
int trace(int);
```

### Step 3: Add a Field to the Process Structure

Add a field to store the trace mask in `struct proc` in `kernel/proc.h`.

```c
struct proc {
  // ... existing fields ...
  char name[16];               // Process name (debugging)
  int trace_mask;              // Trace syscall mask (added)
};
```

### Step 4: Implement `sys_trace()`

Implement the `sys_trace()` function in `kernel/sysproc.c`.

This function should:
1. Read the first argument (mask) using `argint()`
2. Store it in the current process's `trace_mask` field

```c
uint64
sys_trace(void)
{
  // TODO:
  // 1. Read the first argument (mask) using argint()
  // 2. Store it in myproc()->trace_mask
  // 3. Return 0 on success
}
```

**Hint**: Refer to the `sys_kill()` function. The pattern for reading arguments with `argint()` is the same.

```c
// Reference: sys_kill() in kernel/sysproc.c
uint64
sys_kill(void)
{
  int pid;
  argint(0, &pid);
  return kkill(pid);
}
```

### Step 5: Modify the syscall Table and Dispatch Logic

Modify three things in **`kernel/syscall.c`**.

**(a)** Add the function prototype:

```c
extern uint64 sys_trace(void);
```

**(b)** Add an entry to the `syscalls[]` array:

```c
static uint64 (*syscalls[])(void) = {
  // ... existing entries ...
  [SYS_close]   sys_close,
  [SYS_trace]   sys_trace,   // added
};
```

**(c)** Add a syscall name array and add trace logic to the `syscall()` function:

```c
// Syscall name string array (for trace output)
static char *syscall_names[] = {
  [SYS_fork]    "fork",
  [SYS_exit]    "exit",
  [SYS_wait]    "wait",
  [SYS_pipe]    "pipe",
  [SYS_read]    "read",
  [SYS_kill]    "kill",
  [SYS_exec]    "exec",
  [SYS_fstat]   "fstat",
  [SYS_chdir]   "chdir",
  [SYS_dup]     "dup",
  [SYS_getpid]  "getpid",
  [SYS_sbrk]    "sbrk",
  [SYS_pause]   "pause",
  [SYS_uptime]  "uptime",
  [SYS_open]    "open",
  [SYS_write]   "write",
  [SYS_mknod]   "mknod",
  [SYS_unlink]  "unlink",
  [SYS_link]    "link",
  [SYS_mkdir]   "mkdir",
  [SYS_close]   "close",
  [SYS_trace]   "trace",
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();

    // TODO: Add trace logic
    // If bit number `num` is set in p->trace_mask,
    // print "<pid>: syscall <name> -> <return_value>" using printf
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

**Hints**:
- The bitmask check uses the expression `(p->trace_mask & (1 << num))`.
- `p->trapframe->a0` is of type `uint64`, so it should be cast to `(int)` when used with `printf`'s `%d`.
  (The `-Werror` flag causes type mismatch warnings to be treated as compilation errors.)

### Step 6: Inherit trace_mask in `fork()`

In the `kfork()` function in `kernel/proc.c`, add a line so that the child process inherits the parent's `trace_mask`.

```c
int
kfork(void)
{
  // ...
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // TODO: Copy trace_mask from parent to child
  // np->trace_mask = ???;

  // increment reference counts on open file descriptors.
  // ...
}
```

### Step 7: Initialize trace_mask on Process Initialization

The `trace_mask` must be initialized to 0 in the `freeproc()` function in `kernel/proc.c`.

```c
static void
freeproc(struct proc *p)
{
  // ... existing code ...
  p->state = UNUSED;
  // TODO: Add p->trace_mask = 0;
}
```

---

## Testing

### Test Program

Copy `trace_test.c` to the `user/` directory, add it to `UPROGS` in the `Makefile`, and run the tests.
(`trace_test.c` is included in the assignment distribution files.)

```bash
# Build and run xv6
make clean && make qemu

# Test in the xv6 shell
$ trace_test
```

### Expected Output

```
=== Test 1: trace fork ===
<pid>: syscall fork -> <child_pid>
Test 1 PASSED

=== Test 2: trace read/write ===
(write/read trace output appears)
Test 2 PASSED

=== Test 3: trace multiple syscalls ===
(trace output for multiple system calls)
Test 3 PASSED

=== Test 4: trace inheritance ===
(trace output from child processes as well)
Test 4 PASSED

All tests passed!
```

### Manual Testing

You can also verify directly in the xv6 shell:

```bash
# Write a simple program and test it
$ trace_test
```

---

## Submission Method

Submit using one of the following:

**Method 1: patch file**
```bash
cd xv6-riscv
git diff > trace.patch
```

**Method 2: Modified files**

Submit the following files:
- `kernel/syscall.h`
- `kernel/syscall.c`
- `kernel/sysproc.c`
- `kernel/proc.h`
- `kernel/proc.c`
- `user/usys.pl`
- `user/user.h`

---

## Assignment 2: `sysinfo` System Call (Bonus, +10 points)

### Functional Specification

Add a `sysinfo(struct sysinfo *)` system call. This system call returns information about the current system state.

### What to Implement

1. Create the `kernel/sysinfo.h` file:

```c
struct sysinfo {
  uint64 freemem;   // Available memory (bytes)
  uint64 nproc;     // Number of active processes (state != UNUSED)
};
```

2. Register `SYS_sysinfo` as number 23

3. Implement the required helper functions:
   - `freemem_count()` in `kernel/kalloc.c`: Traverse the free list and return the number of free pages * PGSIZE
   - `proc_count()` in `kernel/proc.c`: Traverse the `proc[]` array and return the count of processes that are not UNUSED

4. Implement `sys_sysinfo()` in `kernel/sysproc.c`:
   - Copy the `struct sysinfo` data to the user space pointer (using `copyout()`)

### Hints

- Use `copyout()` to copy kernel data to user space
- Use `argaddr()` to receive the user pointer
- Refer to the implementation pattern of `sys_fstat()`

---

## Grading Criteria

| Item | Score |
|------|-------|
| `trace` system call is properly registered (compilation success) | 20 points |
| `sys_trace()` implementation and mask storage | 20 points |
| `syscall()` produces trace output in the correct format | 30 points |
| trace_mask inheritance works on `fork()` | 20 points |
| Code style and comments | 10 points |
| (Bonus) `sysinfo` system call | +10 points |

---

## References

- xv6 textbook Chapter 2: Operating System Organization
- xv6 textbook Chapter 4: Traps and System Calls
- [MIT 6.1810 Lab: System Calls](https://pdos.csail.mit.edu/6.828/2024/labs/syscall.html)
- System call path analysis from the class lab (Week 3 Lab)
