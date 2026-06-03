// Process info exposed by sys_ps to user space.
// Mirrors a subset of struct proc fields useful for `ps`.
struct procinfo {
  int pid;
  int ppid;
  int state;        // procstate enum value (UNUSED..ZOMBIE)
  int priority;     // MLFQ queue level: 0=HIGH .. 2=LOW (integrated kernel; was 0..20 in haneol's original — see MERGE_NOTES §2.1)
  uint64 sz;        // process memory size (bytes)
  char name[16];    // process name (debugging)
};
