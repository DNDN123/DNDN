# Smart-MLFQ-xv6 — Kernel Patch Set (Critical + Important + Compat Fix)

This patch set addresses 5 design-review issues plus one **critical
build-blocker** found in a follow-up verification pass against the
project's design docs (`docs/proposal.md`, `docs/architecture.md`,
`Solo-MLFQ-xv6-roadmap.md`) and the lecture-repo xv6 source.

All changes preserve the project's overall structure and APIs; they
harden correctness, measurement fidelity, LLM-input quality, and
buildability against the lecture-repo xv6 variant.

## 🚨 Critical compat fix — `forkret()` would not link

**File**: `kernel/proc.c`
**Symptom**: The user's pre-existing `forkret()` ended with a call to
`usertrapret()`. That symbol does not exist in the **lecture-repo's
xv6 variant** — the lecture-repo replaces mainline xv6's
`usertrapret()` with an inline `prepare_return()` + manual trampoline
jump. Building the kernel would fail at link time with:

```
ld: kernel/proc.o: in function `forkret':
proc.c:(.text+...): undefined reference to `usertrapret'
```

This was already in the user's uploaded code (not introduced by the
other 5 patches). Discovered by diffing every "supposedly-unchanged"
function in `proc.c` against `lecture-repo/xv6-riscv/kernel/proc.c`.

**Fix**: Replaced the body of `forkret()` so it ends with the
lecture-repo's return path:
```c
prepare_return();
uint64 satp = MAKE_SATP(p->pagetable);
uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
((void (*)(uint64))trampoline_userret)(satp);
```
A comment in the code explains why.

## Files Touched

```
kernel/
  proc.h        unchanged
  proc.c        modified  (scheduler, sleep, allocproc, kfork,
                           + new helpers: proc_settrace, kforkpri)
  trap.c        unchanged
  defs.h        modified  (added prototypes)
  syscall.h     modified  (added SYS_settrace=24, SYS_forkpri=25)
  syscall.c     modified  (added table entries)
  sysproc.c     modified  (added sys_settrace, sys_forkpri handlers)
user/
  user.h        modified  (added settrace, forkpri prototypes)
  usys.pl       modified  (added stub generators)
```

## Patch Summary

### C1 — Serialized periodic boost (race condition)
**File**: `kernel/proc.c`
**Symptom**: Multiple harts could enter the boost block simultaneously,
each walking `proc[]` and acquiring per-proc locks. Idempotent but
multiplies lock contention and adds measurement jitter.

**Fix**: Added a dedicated `boost_lock` (initialized in `procinit`).
The scheduler uses a double-checked locking pattern: a lock-free read
gates the slow path; after acquiring `boost_lock`, an authoritative
re-check guards the actual boost. The lock is held for the full boost
walk so that only one CPU per interval performs the work.

### C2 — Real-I/O accounting in `sleep()`
**File**: `kernel/proc.c`
**Symptom**: `total_io_blocks` was incremented for every `sleep()`
call, including `kwait()` (parent waiting on child) and `sys_pause()`
(timed sleep). This polluted the per-process behavior summary that is
fed to Solar Pro 3, causing `workload_runner` and any process that
calls `pause()` to be misclassified as "I/O-bound".

**Fix**: In `sleep()`, the increment is skipped when `chan` is a
recognized kernel-internal channel:
- `&ticks` (used by `sys_pause`)
- Any address inside the `proc[NPROC]` array (used by `kwait`)

`time_slice_used = 0` is still applied unconditionally — the wake
semantic (retain queue, fresh slice) is unchanged. Only the
classification counter is filtered.

### C3 — `printf()` outside scheduler lock
**File**: `kernel/proc.c`
**Symptom**: TRACE lines were emitted inside `p->lock` and with
interrupts disabled in the scheduler. xv6's `printf` polls UART
synchronously, costing tens to hundreds of microseconds per line and
distorting every scheduling decision.

**Fix**: The scheduler now snapshots the fields it wants to log
(`tick`, `pid`, `pri`, `slice`, `trace_enabled`) before `swtch`, then
emits the TRACE line *after* `swtch` returns and the lock has been
released. The process whose decision is being logged has already
finished its quantum at that point, so its measured `run_ticks` are
not contaminated. The host's parser is unaffected because the `tick`
field still records the scheduling decision time.

### I2 — `settrace(pid, on)` syscall, tracing off by default
**Files**: `kernel/proc.c`, `kernel/syscall.h`, `kernel/syscall.c`,
`kernel/sysproc.c`, `kernel/defs.h`, `user/user.h`, `user/usys.pl`
**Symptom**: `trace_enabled` defaulted to 1 in `allocproc`, so every
process including `init` and `sh` flooded the console with TRACE/EXIT
lines, complicating host-side trace parsing.

**Fix**:
- `allocproc` now sets `trace_enabled = 0`.
- `kfork` propagates the parent's `trace_enabled` to the child, so
  enabling tracing on `workload_runner` cleanly enables it on all
  workload children too.
- New syscall `settrace(int pid, int on)` (SYS_settrace = 24).
  `pid = 0` means "current process".

Typical usage in `workload_runner.c`:
```c
settrace(0, 1);             // enable tracing on self
for (...) {
    int pid = forkpri(level);
    if (pid == 0) { exec(workload); }
    // child inherits trace_enabled = 1
}
```

### I4 — `forkpri(level)` syscall
**Files**: `kernel/proc.c`, `kernel/syscall.h`, `kernel/syscall.c`,
`kernel/sysproc.c`, `kernel/defs.h`, `user/user.h`, `user/usys.pl`
**Symptom**: With plain `fork() + setpri(child_pid, level)`, the child
could be scheduled at the default HIGH priority for one or two ticks
before the parent's `setpri` call took effect. Short workloads showed
contamination from this initial drift.

**Fix**: New syscall `forkpri(int level)` (SYS_forkpri = 25). It is a
near-copy of `kfork` that sets the child's `priority = level` and
`time_slice_used = 0` after `allocproc` but before transitioning the
child to `RUNNABLE`. There is no window during which the child is
schedulable at the wrong priority.

Returns the child's pid in the parent, 0 in the child, -1 on error.

## Bonus tighten — `proc_setpri` rejects ZOMBIE/UNUSED

`proc_setpri` previously accepted any non-UNUSED state, including
ZOMBIE. ZOMBIE is unreachable from the scheduler and mutating its
priority would mask a host-side race rather than expose it. The
function now requires `USED | RUNNABLE | RUNNING | SLEEPING` via a
shared `proc_is_live()` helper (also used by `proc_settrace`).

## Verification

Performed in this sandbox (no RISC-V toolchain available — full build
must be done by the user; see `VSCODE_VERIFICATION.md` for the
checklist):

- `gcc -fsyntax-only -Wall -Wextra -Werror` against the patched files
  (using xv6 headers from `lecture-repo/xv6-riscv/kernel/` as include
  path) returns cleanly for `proc.c`, `trap.c`, `syscall.c`,
  `sysproc.c`. Only pre-existing xv6 warnings (sign-compare in stock
  `forkret`/`sys_pause`/`syscall`) remain — these are not introduced
  by the patches.
- Brace counts in all modified files balance.
- Every supposedly-unchanged function in `proc.c` was diff'd
  function-by-function against `lecture-repo/xv6-riscv/kernel/proc.c`.
  All differences are either intentional (MLFQ additions, `procdump`
  extended) or whitespace/comment-only — **with the single exception
  of `forkret`**, which is now fixed (see "Critical compat fix"
  above).
- All new symbols (`proc_settrace`, `kforkpri`, `proc_is_live`,
  `boost_lock`, `last_boost_tick`, `sys_settrace`, `sys_forkpri`) have
  definitions in `proc.c`/`sysproc.c` and declarations in `defs.h`
  where needed (`proc_is_live` is `static inline`; file-local
  variables don't need decls).
- No new public API removed; all original syscalls (`setpri`,
  `getstats`) retain their numbers and signatures.

## Suggested Next Steps

1. Apply patches over `xv6-riscv/` source tree and `make qemu` to
   confirm a clean boot.
2. Update `user/workload_runner.c` to use `forkpri(level)` and
   `settrace(0, 1)` per the example above. (User-space changes were
   not in this patch set — those files were not requested for review.)
3. Update `docs/architecture.md` line 90 — the state diagram says
   *"NEW → READY (priority=2 LOW, by default)"*, but the code (and
   `docs/proposal.md`) initialize to HIGH. Reconcile by editing the
   diagram to say `priority=0 HIGH`.
4. After collecting clean baselines, run the host pipeline and verify
   that `total_io_blocks` for `workload_runner` is now 0 (it only
   `kwait`s on children, no real I/O).
