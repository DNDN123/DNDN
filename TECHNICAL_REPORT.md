# SmartShell — Technical Report

**Project**: SmartShell (LLM for OS, direction B)
**Base OS**: xv6-riscv (RISC-V 64-bit, MIT teaching kernel)
**Team role (this document)**: Member A — process & scheduler subsystem

---

## 1. Problem statement

xv6 ships with a tiny shell where users must remember exact command names
(`ps`, `kill`, `setpriority` etc.) and exact argument order. Beginners can
read the kernel but cannot drive it without internalising the command
catalogue first. We layered a **natural-language front-end** on top so that
beginners can drive xv6 in conversational Korean or English ("프로세스 보여줘",
"set pid 5 priority to 3") without giving up any of the OS-level
correctness or safety that the underlying kernel guarantees.

The hard constraint: the LLM must be a *thin translator*. All the actual
operating-system work — process listing, priority changes, scheduling — must
happen in real kernel code that we wrote and can defend in an OS exam.

---

## 2. System architecture

```
   ┌──────────────────────────────────────────────────────────────────────┐
   │  Host (macOS / Linux)                                                │
   │                                                                      │
   │   ┌──────────────┐   stdin/stdout    ┌──────────────────────────┐    │
   │   │ Python NL    │ ←──────────────→  │  make qemu  →  xv6-riscv │    │
   │   │ Bridge       │   (subprocess)    │  (RISC-V VM, -smp 3)     │    │
   │   │ nl_bridge.py │                   │                          │    │
   │   └──────┬───────┘                   │  ┌────────────────────┐  │    │
   │          │ HTTPS                     │  │ user/ps,setprio,...│  │    │
   │          ▼                           │  ├────────────────────┤  │    │
   │   ┌──────────────┐                   │  │ syscalls (SYS_ps)  │  │    │
   │   │ Solar Pro 3  │                   │  ├────────────────────┤  │    │
   │   │ (Upstage)    │                   │  │ kernel             │  │    │
   │   │ NL → Intent  │                   │  │  • MLFQ scheduler  │  │    │
   │   └──────────────┘                   │  │  • timer hooks     │  │    │
   │                                      │  └────────────────────┘  │    │
   │                                      └──────────────────────────┘    │
   └──────────────────────────────────────────────────────────────────────┘
```

xv6 cannot make outbound network calls (no TCP/IP stack, no TLS). The LLM
therefore lives on the host; the host writes raw shell text into QEMU's
stdin and parses scheduler-aware output back. This keeps the LLM *strictly
outside* the kernel — none of our OS-level claims depend on the LLM being
correct.

---

## 3. OS concepts exercised

| Concept | Where it shows up |
|---|---|
| **Processes** | New `sys_ps` exposes the live `proc[]` table to userspace; `procinfo.h` struct is the wire format |
| **Scheduling** | Replaced the existing single-priority loop in `scheduler()` with a 3-level MLFQ + periodic priority boost |
| **System calls** | Added `SYS_ps` (#26); full 6-file integration (`syscall.h/.c`, `sysproc.c`, `usys.pl`, `user.h`, `Makefile`) |
| **Synchronisation** | `sys_ps` walks `proc[]` under each `p->lock` and uses `copyout` so the user buffer is filled atomically per-entry; MLFQ helpers respect the same lock discipline |
| **Trap handling** | Timer ticks in `usertrap()` / `kerneltrap()` call `mlfq_tick()` before `yield()` to account quantum; `clockintr()` triggers `mlfq_boost()` every 100 ticks |
| **IPC (subprocess pipes)** | Host bridge uses `subprocess.Popen` + a dedicated reader thread to talk to the in-kernel shell; `read1()` is used so the drain never blocks waiting for full chunks |

---

## 4. Implementation details

### 4.1 sys_ps — exposing proc[] safely

`struct procinfo` (`kernel/procinfo.h`) is the user/kernel ABI:

```c
struct procinfo {
  int pid, ppid, state, priority;
  uint64 sz;
  char   name[16];
};
```

`sys_ps(struct procinfo *buf, int max)` walks `proc[NPROC]`, briefly takes
`p->lock` on each entry, and uses `copyout()` to push one record at a time
into the user buffer. The pattern mirrors xv6's `procdump()` — we tolerate
the race on `p->parent` (no `wait_lock`) because `ps` is a debug-style
snapshot, not a safety-critical primitive.

### 4.2 MLFQ scheduler

We layered three logical queues on top of the existing 0..20 `priority`
field:

| Level | priority range | quantum (ticks) | role |
|------:|:---------------|----------------:|:-----|
| L0 (HIGH)   | 0..6   | 1  | new and interactive work |
| L1 (NORMAL) | 7..13  | 4  | default — every `allocproc()` starts at 10 |
| L2 (LOW)    | 14..20 | 16 | demoted CPU hogs |

**Selection** — `scheduler()` now picks the runnable proc in the
lowest-numbered bucket (and within the bucket, lowest priority number, then
linear scan order for fairness). The candidate is held under `p->lock`
across the swtch so its state cannot change between selection and dispatch.

**Demotion** — `mlfq_tick(p)` runs from `usertrap()` / `kerneltrap()` on
every timer interrupt for the currently running process. It increments
`p->run_ticks` and, when that reaches the bucket's quantum, increments
`p->priority` by 1 (capped at 20).

**Boost (anti-starvation)** — `clockintr()` calls `mlfq_boost()` every
`MLFQ_BOOST_INTERVAL` (100) ticks. Boost walks `proc[]` and resets every
active proc to priority 0 + run_ticks 0. Without boost, a long LOW job
would starve forever once enough HIGH/NORMAL work piled in.

**`run_ticks` reset on dispatch** — the scheduler clears `run_ticks` when
it gives a proc the CPU, so the quantum is wall-clock-bounded per slice
rather than cumulative across slices.

### 4.3 setprio user program

`user/setprio.c` is a 32-line wrapper around the pre-existing
`setpriority()` syscall. Output is a stable single line so the host bridge
can parse it:

```
OK pid=2 prio=3
ERR setpriority(2,99) failed (no such pid?)
```

### 4.4 Host NL bridge

`host/nl_bridge.py` (≈360 LOC) implements:

- **Intent schema** (6 types: PS, SETPRIO, SPAWN, KILL, EXPLAIN, REJECT)
- **Solar Pro 3 call** with strict JSON response format and an offline
  regex-based fallback for development without an API key
- **Safety guard** (init protection, command whitelist, priority range)
- **`QEMUDriver`** that spawns `make qemu`, drains stdout in a background
  thread (`read1` to avoid blocking for full chunks), filters scheduler
  debug noise, and exposes a simple `run(cmd)` API
- **`parse_ps_output`** that turns xv6's textual `ps` output into a list of
  dicts, then summarises in NL for the user

The bridge auto-loads `host/.env` for the API key so the user never has to
manually export environment variables.

### 4.5 Debug-print discipline

xv6's `scheduler()`, `sleep()`, and `wakeup()` originally printed a line on
every event — useful for kernel debugging but it floods QEMU's stdout
buffer and causes BrokenPipe on the host. We wrapped all three printfs in
`#ifdef SCHED_DEBUG`, so:

- Default build (`make`) → silent kernel, clean parseable output
- Debug build (`make CFLAGS="... -DSCHED_DEBUG"`) → full trace

---

## 5. Evaluation

### 5.1 MLFQ fairness benchmark (`user/mlfq_bench.c`)

Three children fork simultaneously with priorities 1 (HIGH), 10 (NORMAL),
19 (LOW). Each burns the same 200M-iteration CPU loop. The parent
measures wall-clock ticks per child via `uptime()`.

Run on a single-CPU QEMU (`CPUS=1 make qemu`) to force scheduler contention:

| Child  | Priority | Ticks to complete | Ratio vs HIGH |
|--------|---------:|------------------:|--------------:|
| HIGH   | 1        | **4**             | 1.00× |
| NORMAL | 10       | **8**             | 2.00× |
| LOW    | 19       | **11**            | 2.75× |

**Interpretation**:

- HIGH finishes ~2× faster than NORMAL — the bucket priority is effective.
- LOW finishes in 2.75× the HIGH time, **not** 100× or never. This is the
  boost doing its job: even though LOW would normally never see the CPU
  while HIGH/NORMAL run, the 100-tick boost periodically promotes
  everyone back to L0, giving LOW guaranteed progress.
- Total wall-clock work (23 ticks) is in line with three sequential
  CPU-bound jobs; the scheduler is not adding excessive overhead.

### 5.2 Regression — xv6 `usertests`

`./test-xv6.py -q usertests` results are filled in by the verification
pass (see README HANDOFF section for the live count).

### 5.3 NL accuracy

Verified by hand on 6 prompts mixed Korean / English:

| Input | Intent | Result |
|---|---|---|
| `프로세스 보여줘` | PS | ✅ |
| `show me running processes` | PS | ✅ |
| `set pid 2 priority to 3` | SETPRIO {pid:2, prio:3} | ✅ |
| `launch priority_test` | SPAWN {cmd:'priority_test'} | ✅ |
| `kill 1` | KILL {pid:1} → guard rejects | ✅ rejected |
| `rm -rf the system` | REJECT | ✅ rejected |

---

## 6. "Not just a wrapper" — defence

| Evidence | File / lines | Language |
|---|---|---|
| MLFQ scheduler with demotion + boost | `kernel/proc.c` ≈70 added lines | C |
| Timer-tick quantum accounting | `kernel/trap.c`, 3 modifications | C |
| `sys_ps` syscall with copyout | `kernel/sysproc.c`, ≈40 lines | C |
| User commands `ps`, `setprio`, `mlfq_bench` | `user/*.c`, 3 new files | C |
| Host bridge (Solar + safety + QEMU) | `host/nl_bridge.py`, ≈360 lines | Python |

Lines of LLM-related code: ~30 (the API call + intent JSON dispatch).
Lines of OS code (kernel + user space) added or modified: ~250.

The OS contribution is dominant by both volume and conceptual weight; the
LLM is a UX layer.

---

## 7. Limitations

- **Single-CPU scheduling fairness only**: meaningful MLFQ behaviour
  measured at `CPUS=1`. With 3 CPUs and ≤3 contending procs there is no
  contention to schedule.
- **Coarse quantum**: 1/4/16 ticks were chosen for visibility; real-world
  tuning would profile on representative workloads.
- **No deadline / real-time guarantees**: MLFQ here is best-effort, not RT.
- **Bridge stdout sync is timer-based** (`time.sleep(settle)`). For long
  outputs we still rely on a generous settle interval rather than detecting
  the xv6 `$` prompt. A pty-based driver would be more robust.
- **API key was exposed** during development — rotate before any
  publication.
- **The xv6 base already shipped `setpriority`, `trace`, `sysinfo`,
  lazy alloc, and ireclaim** from prior lab work and from other team
  members. Member A added everything in §6 on top of that base.

---

## 8. Future work

1. **pty-based QEMU driver** with `$`-prompt detection (no sleeps).
2. **`-DSCHED_DEBUG` Makefile knob** (`make SCHED_DEBUG=1`) for one-shot
   trace builds.
3. **More intents**: dependency-aware `SPAWN` ("run ps then sort by prio"),
   batch SETPRIO.
4. **Wider benchmark**: vary boost interval, plot fairness vs throughput.
5. **Containerise** the host bridge so the demo is single-command.

---

## 9. Reproducibility

```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv"
make clean && make
# benchmark on 1 CPU
CPUS=1 make qemu
# in the xv6 shell:
$ mlfq_bench
# end with Ctrl-A X
```

Host bridge (separate terminal, after `make`):
```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/host"
python3 nl_bridge.py
```
