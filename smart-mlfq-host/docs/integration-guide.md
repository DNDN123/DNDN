# Team Integration Guide — Scheduler Slice

Owner: 조현성 (스케줄러 담당)
Status: draft for W13 team integration meeting
Audience: 전한얼 (프로세스), 기민주 (시스템콜), 안진환 (스레드), 진단 담당

This document describes what the scheduler slice **provides** to the rest
of the team and what it **expects** from the rest of the team. Read this
before integrating your module.

---

## 1. What the scheduler slice owns

| Layer | File | Owns |
|---|---|---|
| Kernel scheduler | `kernel/proc.c::scheduler` | the 3-level MLFQ selection loop, the priority-boost mechanism |
| Tick handling | `kernel/trap.c` (timer ISR section) | slice-counter increment, time-based demotion |
| Process struct fields | `kernel/proc.h::struct proc` | `priority`, `time_slice_used`, `total_run_ticks`, `total_io_blocks`, `arrival_tick`, `completion_tick`, `trace_enabled`, `final_pri` |
| I/O retention | `kernel/proc.c::sleep` (patched) | resets slice counter on sleep without demoting |
| Syscalls (4) | `kernel/sysproc.c`, syscall numbers in `kernel/syscall.h` | `setpri`, `getstats`, `settrace`, `forkpri` |
| Spinlocks | `kernel/proc.c` | `boost_lock` (global), reuses standard per-proc `p->lock` |
| User programs | `user/wrunner.c`, `user/nlrun.c`, `user/{cpu,io,mixed}_burner.c` | workload runners and test programs |
| Host bridge | `smart-mlfq-host/*.py` | trace parsing, evaluation, NL shell, LLM calls |

If you need to change anything in this list, **ping me before patching**
— most of these are touched by Solar Pro 3 prompts or by the host
evaluator and silent changes will break the evaluation.

---

## 2. Reserved syscall numbers

These four numbers are committed and must not be reused by other slices:

| # | Name | Signature | Notes |
|---|---|---|---|
| 22 | `setpri`   | `int setpri(int pid, int level)` | -1 on invalid pid/level |
| 23 | `getstats` | `int getstats(int pid, struct procstats *out)` | fills user struct |
| 24 | `settrace` | `int settrace(int pid, int on)` | `pid=0` ⇒ self |
| 25 | `forkpri`  | `int forkpri(int level)` | atomic fork with priority |

(Exact numbers visible in `smart-mlfq-xv6-patches/kernel/syscall.h`.)

**Action for syscall track (기민주):** when adding new syscalls, start at
`#26` and walk upward. Keep `struct procstats` byte-compatible between
`kernel/proc.h` and `user/user.h`.

---

## 3. APIs that other slices can call

### 3.1 From user space (other team members' programs)

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// Atomically fork a child into queue `level` (0=HIGH, 1=MID, 2=LOW).
int pid = forkpri(2);
if (pid == 0) {
    // child runs at LOW priority from its very first tick
    exec("cpu_burner", (char*[]){"cpu_burner", "100000", 0});
}

// Read priority + counters of any process.
struct procstats st;
if (getstats(target_pid, &st) == 0) {
    printf("pid=%d priority=%d run=%d io=%d\n",
           st.pid, st.priority, st.total_run_ticks, st.total_io_blocks);
}

// Re-tier a running process.
setpri(target_pid, 0);    // promote to HIGH

// Turn TRACE / EXIT log emission on (pid=0 means self + future children).
settrace(0, 1);
```

### 3.2 From host Python (diagnosis layer)

The diagnosis layer can shell out into xv6, call `getstats`, and parse
the output. For automated use:

```python
# Parse `EXIT pid=... name=... arrival=... completion=... run=... ioblock=... final_pri=...`
# lines from any captured xv6 log.
from parse_trace import parse
result = parse("/path/to/xv6_run.log")
for evt in result["events"]:
    if evt["kind"] == "exit":
        # diagnosis logic here
        ...
```

---

## 4. What I expect from other slices

### 4.1 Process track (전한얼)
- `fork`/`exec`/`wait` semantics must remain xv6-stock-compatible. The
  scheduler relies on `state` transitions (`UNUSED` → `EMBRYO` → ... →
  `ZOMBIE` → `UNUSED`) being unchanged.
- If you add a new process state, tell me — `scheduler()` walks
  `proc[NPROC]` and checks `state == RUNNABLE`.
- Zombie reclamation belongs to your slice. The scheduler does not
  reap; `nlrun.c` and `wrunner.c` already `wait()` for their direct
  children.

### 4.2 Syscall track (기민주)
- New syscall numbers start at 26 (see §2).
- `argint` / `argaddr` conventions unchanged.
- If you add a new field to `struct proc`, place it **after** the MLFQ
  fields (priority/time_slice_used/...) — moving them changes offsets
  in the scheduler's hot path.

### 4.3 Thread track (안진환)
- xv6 stock has no threads. If you add a `clone()` syscall or thread
  table, decide up-front: **is priority per-thread or per-process?**
  The current MLFQ assumes per-process. If you want per-thread you'll
  need to move `priority`, `time_slice_used`, `trace_enabled` out of
  `struct proc` into a per-thread struct and update `scheduler()`'s
  inner loop. Ping me first — this is a deep change.
- For the W14 demo, simplest win: keep priority per-process; threads
  inherit their owner process's queue level.

### 4.4 Diagnosis layer
- Trace format is stable. Don't expect new fields without coordination.
- `settrace` is opt-in: if you want trace for a process you didn't
  spawn, call `settrace(pid, 1)` first.
- `getstats` is read-only; safe to poll.

---

## 5. Cross-cutting conventions

- **Process names skipped in stats**: `wrunner`, `nlrun`, `sh`, `init`.
  Encoded in `SKIP_NAMES` in `viz.py`, `evaluator.py`, `llm_hint.py`.
  If you add a framework process (a launcher, a daemon), add its name
  to that set too.
- **LLM never reaches the kernel.** Solar's output is reduced to one
  integer in `{0, 1, 2}` before it crosses `forkpri()`/`setpri()`.
  If your slice wants to consult Solar, do so in host Python and pass
  the result through this funnel.
- **Locks.** Per-proc operations: hold `p->lock`. Global MLFQ state
  (`last_boost_tick`): hold `boost_lock`. Always acquire in this order
  if you need both: `boost_lock` first, then `p->lock`.

---

## 6. Smoke test for your slice

After integrating, run this from `~/smart-mlfq-host` to verify the
scheduler is still intact:

```bash
# 1) Boot xv6, check 4 syscalls return sane values.
make qemu CPUS=1
# inside xv6 shell:
$ nlrun 0 echo hello              # ⇒ TRACE pri=0
$ nlrun 2 cpu_burner 100000       # ⇒ TRACE pri=2 + EXIT final_pri=2

# 2) Host smoke
python parse_trace.py xv6_run.log --output /tmp/t.json
python evaluator.py --baseline /tmp/t.json
```

If both steps still pass, your changes haven't broken the scheduler.

---

## 7. Open coordination items (to settle at W13 meeting)

- [ ] **#26 first syscall reservation** — syscall track decides
- [ ] **Per-thread vs per-process priority** — thread track decides
- [ ] **Diagnosis layer's interface** — does it call `getstats` directly
      or scrape `EXIT` lines? Need consistency
- [ ] **Shared NL shell** — does the team build one unified `nl_shell.py`
      that knows about all three slices' commands, or three separate
      REPLs? Recommendation: one unified, with each slice contributing
      a prompt section to `prompts.py`
- [ ] **Workload integration** — should the diagnosis layer's *"diagnose
      this slowness"* be expressible as an `nlrun` invocation, or is it
      its own user program?

---

## 8. Contact

If anything in this document is ambiguous or you need a contract change,
contact 조현성 directly. Don't paper over scheduler internals — most of
the subtle parts (multi-CPU race, I/O retention, boost timing) are
captured in `smart-mlfq-xv6-patches/REVIEW_AND_PATCHES.md`.
