// Process info exposed by sys_ps to user space.
// Mirrors a subset of struct proc fields useful for `ps`.
struct procinfo {
  int pid;
  int ppid;
  int state;        // procstate enum value (UNUSED..ZOMBIE)
  int priority;     // 0=highest .. 20=lowest
  uint64 sz;        // process memory size (bytes)
  char name[16];    // process name (debugging)
};
