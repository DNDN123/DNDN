# Verification Report — xv6 NL-OS

A systematic verification pass over the kernel, scheduler, syscalls, NL pipeline,
and safety guard. Each finding lists the symptom, the root-cause investigation,
the fix, and the evidence that the fix holds.

## Methodology

- **`host/qemu_probe.py`** — a deterministic, scriptable driver for the xv6
  console: boots QEMU, feeds commands, captures the transcript, waits on
  completion markers (`--until`) or idle for slow tests. Used for every
  in-guest test below.
- **`host/test_nlos.py`** — offline checks (no QEMU, no model) for the host
  pipeline: guard verdicts, command mapping, sentinel parser, rule fallback,
  preflight.
- Tests were repeated (5–16×) where flakiness was suspected, and isolated by
  varying one factor at a time (`-smp`, clean vs dirty `fs.img`, single vs
  multi-command).

---

## Bug #1 — `usertests writebig` fails: filesystem too small

- **Symptom:** `writebig` fails with `balloc: out of blocks` at block ~239.
- **Root cause:** this repo packs 40+ user programs into `fs.img`. With the
  stock `FSSIZE = 2000` blocks, mkfs reports "first 1760 blocks allocated",
  leaving < 268 free — fewer than `MAXFILE` (268), which `writebig` needs.
  Not a kernel bug; a capacity shortfall from the extra programs.
- **Fix:** `kernel/param.h` `FSSIZE 2000 → 8000` (fs.img 8 MB, ample headroom).
- **Evidence:** `test writebig: OK` after the change.

## Bug #2 — MLFQ scheduler starvation (the important one)

- **Symptom:** `usertests reparent` fails intermittently (≈17% at `-smp 3`).
  The reason line is suppressed because the test's master process is killed,
  pointing at **proc-table exhaustion** (a grandchild `fork()` returns < 0).
- **Isolation:** failure rate vs core count —
  - `-smp 1`: **8/8 FAILED (100%)**
  - `-smp 3`: 2/8 failed
  Failing *more* with *fewer* cores rules out an SMP data race and points to a
  scheduling/throughput problem: the single CPU cannot reap orphaned children
  fast enough, so the proc table fills.
- **Root cause:** the MLFQ scheduler rescanned `proc[]` from index 0 every
  iteration and ran the *first* RUNNABLE proc at the top priority, then broke
  and restarted. With many equal-priority runnable procs (a fork storm), the
  lowest-index ones were always chosen and **higher-index runnable procs
  starved** — they never ran, never exited, never got reaped → table fills →
  `fork()` fails. `boost`/`aging` cannot help: the starved procs are already at
  HIGH. (Stock xv6 avoids this by running *every* runnable proc once per pass.)
- **Fix:** round-robin **within** a priority level via a rotating `rr_cursor`
  (`kernel/proc.c scheduler()`), so equal-priority runnable procs each get a
  turn. Priority levels remain strictly ordered (0 before 1 before 2).
- **Evidence:** after the fix, `reparent` — `-smp 1`: **0/10**, `-smp 3`:
  **0/10**. Full `usertests`: **ALL TESTS PASSED** (61 tests), and **stable** —
  3/3 independent clean-image runs all PASSED (previously flaky). 8 concurrent
  `cpu_burner`s all scheduled (no starvation) and cleaned up by `killall`.

## Methodology fix — `fs.img` persistence masquerading as a kernel bug

- **Symptom:** `usertests bigdir` failed at the first link `link(bd, x00)`.
- **Investigation:** basic `ln README r2` worked, but `ln README x00` failed
  while `ls` showed `x00` already present — and a clean run later **passed**.
- **Root cause:** QEMU writes to `fs.img` and those writes **persist across
  boots**. Repeated test runs left `x00…` entries on disk, so the next
  `link(bd, x00)` hit "name exists". Worse, `make fs.img` does **not**
  regenerate the image when only the kernel changed, so the polluted image
  carried into later runs.
- **Resolution:** regenerate a clean image (`rm fs.img && make fs.img`) before
  fs-mutating tests. On a clean image `bigdir` → **OK**. No kernel defect.

## Harness fix — double-send corrupting side-effecting commands

- **Symptom:** flaky `ln`/`link` "failed" results that contradicted `ls`.
- **Root cause:** `qemu_probe.send()` resent a command if it didn't see the
  echo within 2 s, occasionally running side-effecting commands (`ln`, `mkdir`)
  **twice** — the second invocation failed ("already exists").
- **Fix:** treat "any new console output after the write" as proof the command
  landed; resend only on a *total* drop (no output at all). Never resend a
  command that already produced output.

## Guard hardening — invalid pids

- `kill` now rejects non-positive and absurdly large pids (`pid < 1` or
  `pid > 1_000_000`) in addition to the existing protection of pid 0/1.
  Injection-shaped args (`"1; rm -rf /"`) were already rejected as non-numeric.
- `host/test_nlos.py` extended accordingly (negative / huge / injection pids).

---

## Feature — ask-less auto-fallback ("just talk to the OS")

Typing plain language at the xv6 prompt now works **without the `ask` prefix**.

- **Mechanism:** the bridge reconstructs the user's typed line; when xv6's shell
  prints `exec <w> failed` (sh.c:80 — unknown command), the bridge re-interprets
  that line as natural language, translates, guards, and injects the command.
- **Precise trigger (verified):** fires only when the failed program name equals
  the **first token of what the user typed**, so a *program* that itself prints
  `exec ... failed` does **not** misfire. Test: `echo exec zzz failed` → no
  fallback; `무거운거 정리해줘` → exactly one fallback → `killheavy`. Validated
  against every `exec…failed`-shaped line in the repo: `usertests` L195
  (`exec(echo…` — no space, no match), L706 (`<test>: exec echo failed` — not at
  line start), and `nlrun`/`rrunner`/`wrunner` (the failed name is the launched
  workload, never the launcher the user typed) — none can spuriously fire.
  **Empirically confirmed:** running the full `usertests` *through the live
  bridge* → ALL TESTS PASSED with **0** spurious fallbacks and **0** spurious
  injections over the entire ~4-minute run (which includes exec-failure tests).
- **Rapid/pasted multi-line (verified):** typed lines are kept in a queue and an
  `exec X failed` is correlated to the typed line whose first token is X — not
  just the most recent line — so pasting several NL commands at once matches each
  to the right one. Test: `업타임 알려줘` + `좀비 정리해줘` pasted together →
  uptime AND reap, each correctly. (A single-slot tracker would have lost the
  first.)
- **Safe:** real commands (`ps`, `ls`, `kill 7`) run directly and fast (no
  interception); injected commands are never re-translated (no loop); model
  unreachable → rule fallback; destructive ops still gated.
- **Verified live** (on-device model): Korean + English bare NL —
  `제일 무거운 프로세스 종료해줘`→killheavy, `clean up zombie processes`→reap,
  `업타임 얼마나 됐어`→uptime, `7번 프로세스 꺼줘`→kill 7. Explicit `ask` still
  works; `NLBRIDGE_NOAUTO=1` disables the fallback (ask-only).
- **Known limit:** if the NL starts with a real command word (`ls 정리해줘`) the
  shell runs that command — use `ask` for those.

## Independent code review (7-angle finders + verify)

A separate adversarial review of every changed file. Most candidates were
refuted by construction; the real ones were fixed:

- **REFUTED** (false positives, kept for the record): `rr_cursor` multi-hart race
  (benign — every scan still covers all NPROC slots, so no starvation; proven by
  usertests 3/3 and reparent 0/10); deque `remove`-during-iteration (the `break`
  immediately after `remove` prevents any further iteration → no RuntimeError,
  verified empirically); `last_injected[0]` read/write race (atomic under the
  GIL, and injected commands are never in `typed` anyway); `model_server` empty
  `messages` (already caught → HTTP 500); `qemu_probe` partial write (commands ≪
  PIPE_BUF, writes are atomic).
- **FIXED:**
  1. `executor.py` — `setpri` validated only the priority, not the pid; now
     rejects pid < 1, pid > 1e6, and protected pids (symmetry with `kill`).
  2. `nlos.py` — model-server log file handle leaked on early return; now opened
     in a `with` (child keeps its dup'd fd).
  3. `nlos.py` — `have_local_model()` could raise on `os.listdir` (TOCTOU/perms);
     now guarded with try/except → returns False.
  `test_nlos.py` extended (55 checks total).

## Confirmed-correct behavior (no defect)

- **Kernel correctness:** `usertests` → **ALL TESTS PASSED** (61) on a clean
  image after the two fixes above.
- **MLFQ policy:** CPU-bound `cpu_burner` is demoted HIGH→LOW (PRIO 2);
  IO-bound `io_burner` stays HIGH (PRIO 0); two CPU hogs both progress at LOW
  (round-robin fairness). `tracepid` emits per-tick TRACE lines showing the
  slice counter and demotion.
- **Syscalls:** `ps`, `kill`, `killall`, `killheavy`, `reap`, `setpri`,
  `tracepid`, `forkpri` (sets initial level before RUNNABLE; boost then resets
  per design), `uptime`, `sysinfo`, and `thread_create/join/exit` + `futex`
  (threadtest: shared counter = 200000, no lost updates) all verified.
- **Safety guard:** protects init, gates destructive ops, validates args,
  rejects malformed/unknown/injection specs. `test_nlos.py`: 52/52.
- **NL model accuracy** (on-device fine-tuned model, via `host/eval_sample.py`):
  - 200-sample: cmd 98.5% / queue_hint 100%.
  - **Full test set (1854):** **cmd 98.2%** (1820/1854), **queue_hint 99.4%**
    (1842/1854), both 97.7%, 1 error. 18 of 20 intents are ~100% (cat, kill,
    killall, mkdir, rm, uptime, ps, reap, sysinfo, trace, io_burner, cpu_burner,
    echo, ln, …). Two soft spots, both fixable with targeted training data, not
    a model-capacity limit:
    - `explain` 94% (246/261) — "프로세스가 뭐야" classified as `ps` (inherent
      action-vs-explain ambiguity).
    - `mixed_burner` 90% (91/101) — confused with `cpu_burner` (several misses
      are typo'd inputs like `miexd_burner`).
  - The guard is a second line of defense regardless of model error.
