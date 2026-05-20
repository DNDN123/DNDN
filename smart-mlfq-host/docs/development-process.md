# Development Process Document — Scheduler Track

Author: 조현성
Project: Smart-MLFQ on xv6 (Direction B, scheduler slice)
Period: Week 9 (2026-04-30) — Week 14 (2026-06-XX)

> **STATUS — LIVE DOCUMENT.** Update weekly with what actually happened,
> not what was planned. Honest retrospectives matter more than green
> checkmarks at submission time.

---

## 1. Overview

Course rule requires a written record of planning, scheduling, execution,
and retrospective. This document captures the scheduler track. The team
project as a whole is documented in each member's section of the shared
report; here I cover only what I personally did and the decisions I made.

### Roles (team-wide reference)
| Member | Track |
|---|---|
| 전한얼 | Process |
| 기민주 | System call |
| 안진환 | Thread |
| **조현성 (this doc)** | **Scheduler** |

---

## 2. Weekly progress log

### Week 9 (2026-04-30 — 2026-05-07) — Direction and team
**Planned:** Decide Direction A vs B, form team, pick personal track, set
up xv6 build environment.

**Executed:**
- Direction B chosen (LLM-for-OS, intelligent shell theme)
- Personal track: scheduler
- xv6-riscv cloned, WSL Ubuntu environment confirmed
- Read `kernel/proc.c::scheduler()` end-to-end; took notes on the
  RR loop, `swtch`, and `p->lock` discipline

**Issues / decisions:**
- Considered Python simulation vs real xv6 modification — picked real
  xv6 for portfolio value (see `docs/planning/decision-aid-A-vs-B.md`)
- Solo execution within the team frame: each member builds an
  independent slice, integration deferred to W13

**Retrospective:** Reading the stock xv6 scheduler took longer than
expected (~6 hrs) but paid off — later patches reused the locking
pattern without surprises.

---

### Week 10 — Architecture and `struct proc` extension
**Planned:** Architecture diagram, interface decisions, first kernel
patches (priority field + simple priority scheduler).

**Executed:**
- Block diagram + OS concept mapping → `docs/architecture-scheduler.md`
- `struct proc` extended (`priority`, `time_slice_used`, run/io
  counters, arrival/completion ticks, trace flag)
- `allocproc()` initializes new fields
- Stock RR scheduler replaced with priority-based selection (no
  demotion yet)

**Issues / decisions:**
- Slice limits: chose 2/4/8 ticks for HIGH/MID/LOW (OSTEP-inspired,
  small enough to converge fast at xv6 tick rate)

**Retrospective:** Single-pass scheduler patch broke `forkret` on
the first compile — the course's xv6 doesn't have `usertrapret()` in
the same place. Captured as patch issue 🚨 in `REVIEW_AND_PATCHES.md`.

---

### Week 11 (current target) — Natural-language shell + demo
**Planned:** Natural-language prototype + simple execution path. End-to-end
flow: NL → spec → xv6 → MLFQ.

**Executed:**
- 3-level MLFQ with demotion (`trap.c`) and boost (`proc.c`) shipped
- Multi-CPU safe boost via `boost_lock`
- I/O retention patch (sleep doesn't demote)
- Four syscalls: `setpri`, `getstats`, `settrace`, `forkpri`
- `nlrun.c` user program (host NL spec → forkpri → exec)
- `nl_shell.py` host REPL with Solar Pro 3 + heuristic fallback
- `NL_TO_SPEC_PROMPT` few-shot prompt
- WSL end-to-end verified: NL request "Run a heavy job in background"
  → cpu_burner ran at queue 2 (TRACE confirmed `final_pri=2`)
- 4 charts generated (`avg_turnaround`, `per_pid_tt`, two Gantt)
- W11 slide content (English) drafted

**Issues / decisions:**
- Solar Pro 2 vs Solar Pro 3: switched to Pro 3 (course mandate) and
  added `extra_body={"reasoning_effort": "low"}` for cost control
- `.env.example` was accidentally created as a *directory* on WSL
  due to a tool quirk — caught and recreated as a file
- Removed `hello` from `KNOWN_PROGRAMS` (no such xv6 program); used
  `echo hello` for the HIGH-queue demo instead

**Retrospective:** The WSL ↔ Windows file synchronization is a recurring
hazard (two viz.py syncing accidents and one `.env` directory accident).
Going forward, treat Windows as canonical and re-sync from there for
every build.

---

### Week 12 (in progress) — Quantitative evaluation
**Planned:**
- 3 workloads × 3 modes (baseline / heuristic / Solar) experimental
  matrix
- `evaluator.py --heuristic --llm` three-way comparison
- Identify ≥1 case where Solar genuinely outperforms heuristic
- Add `mixed_burner.c` so MID queue is exercised
- Update charts; draft technical report numbers

**Executed:**
- [x] `mixed_burner.c` added; `EXIT final_pri=1` confirms MID retained
- [x] `workloads/three_way.txt` and `workloads/realprog.txt` registered
      in `fs.img`
- [x] `evaluator.py` extended with `--heuristic` + `three_way_compare`
- [x] Tail metrics added: `p95_turnaround`, `p99_turnaround`,
      `p95_response`, `starved_pids`, `longest_low_gap`
- [x] `run_w12_matrix.sh` driver — 5 workloads × 3 modes = 15 traces +
      combined report
- [x] Matrix executed (initial 3 workloads + extended 2);
      see `~/mlfq-experiments/w12_*/`
- [x] Stock-xv6 baseline: built `~/xv6-stock` (no MLFQ patches),
      ran `rrunner` on the 3 workloads — §5.4 Table 2
- [x] Multi-CPU sweep `CPUS=1/2/4` on mixed — Table 3
- [x] Stress test (10 simultaneous procs); `starved_pids=0` — Table 4
- [x] `reasoning_effort` sweep (low/medium/high) — Table 5
- [x] **Aging** added: `last_ran_tick` field + `MLFQ_AGING_INTERVAL=25`
      / `MLFQ_AGE_THRESHOLD=30` aging pass in `scheduler()`
- [x] Hint cache (`~/.smart-mlfq/hints_cache.json`, sha1 key,
      7-day TTL) → 2.8× speedup, API call saved
- [x] Diagnosis layer: `diagprog.c` + `nl_shell.py --diagnose-from FILE`
      + `DIAGNOSE_PROMPT`
- [x] `technical-report.md` §5 fully populated; abstract + conclusion
      rewritten with measured numbers

**Issues / decisions:**
- *I-11* First-cut `three_way.txt` used `cpu_burner 100000`, which
  completed in ≤1 tick under QEMU — invisible to demotion. Bumped to
  `cpu_burner 20000000` so the process survives long enough for the
  full HIGH → MID → LOW walk and for aging to become observable.
- *I-12* xv6's `DIRSIZ = 14` rejected the filename `real_programs.txt`.
  Renamed to `realprog.txt` everywhere.
- *I-13* Default `make` target builds only the kernel binary; user
  programs are produced by `make fs.img`. Documented this in the demo
  runbook so future runs don't ship without `_nlrun` / `_diagprog`.
- *I-14* `.env.example` was edited with a live API key in one of the
  sync passes. Restored to placeholder and rotated the key. `.env` is
  in `.gitignore`; `.env.example` is allowed by explicit exception.
- *I-15* `apply_patches.sh` originally copied only kernel files. After
  re-patching a fresh xv6 tree the workload programs were missing.
  Extended the script to copy `{cpu,io,mixed}_burner.c`, `wrunner.c`,
  `nlrun.c`, `diagprog.c` and to idempotently insert `UPROGS`
  entries via `sed`.
- *I-16* For the stock-xv6 comparison `wrunner` couldn't be reused —
  it calls our `settrace` / `forkpri` syscalls which don't exist in
  stock. Wrote a stripped-down `rrunner.c` using only stock syscalls
  (`fork` / `exec` / `wait` / `uptime` / `pause`) and timed the batch
  via `uptime()`.
- *I-17* Multi-CPU runs slow *down* at `CPUS=4`. Investigated and
  attributed to UART console serialisation + `boost_lock`, not a
  scheduler defect. Documented as a known limitation in §6 of the
  technical report.

**Retrospective:**
- W12 went smoother than W11 because the test infrastructure
  (`parse_trace` + `evaluator` + `viz`) was already mature. The bulk
  of the week was data collection, not new code.
- The most informative result was the mixed-workload Jain fairness
  jump (+37.65 %) — that is the headline number for the W14 slides.
- The most embarrassing result was `cpu_heavy` showing no benefit at
  any setting. This is honest and goes in the report as a negative
  finding rather than being papered over.
- Time budget: estimated 10 h, actual ~9 h. Aging, hint cache, and
  diagnosis were each sub-2-hour additions thanks to interfaces
  established in W11.

---

### Week 13 — Team integration + dry run
**Planned:**
- Coordinate with process / syscall / thread tracks
- `docs/integration-guide.md` distributed and discussed
- Joint demo: NL shell that knows about all four tracks
- Wednesday dry run of final presentation

**Executed:** [TODO]

**Issues / decisions:** [TODO]

**Retrospective:** [TODO]

---

### Week 14 — Final presentation
**Planned:**
- English slide deck final pass
- Backup demo video re-recorded
- Technical report + this doc finalized
- Q&A rehearsal

**Executed:** [TODO]

**Retrospective:** [TODO]

---

## 3. Notable issues encountered

| # | Issue | Resolution | Time cost |
|---|---|---|---|
| I-1 | Stock RR vs MLFQ — `forkret()` referenced absent function in course xv6 | Patched to use course-tree-compatible return path; logged as fix 🚨 in `REVIEW_AND_PATCHES.md` | ~2 hr |
| I-2 | `boost` ran on every CPU concurrently, causing races on `last_boost_tick` | Added `boost_lock` with double-checked acquire | ~3 hr |
| I-3 | `wrunner`'s own `pause(1)` was counted as I/O block in stats | Filtered framework processes via `SKIP_NAMES` (`wrunner`, `nlrun`, `sh`, `init`) | ~1 hr |
| I-4 | `fork()` then `setpri()` had a race: child could run before priority was set | Added atomic `forkpri()` syscall | ~2 hr |
| I-5 | Scheduler held `p->lock` while calling `printf` for TRACE → measurable distortion | Moved trace `printf` to after `swtch` returns, no lock held | ~1 hr |
| I-6 | xv6 `DIRSIZ=14` — `workload_runner` filename too long for fs.img | Renamed to `wrunner` | ~10 min |
| I-7 | `viz.py` paste from markdown introduced phantom 2-space indent → `IndentationError` | Re-wrote file without indent prefix; verified via `ast.parse` | ~5 min |
| I-8 | WSL pip refused `pip install matplotlib` (PEP 668) | Used per-project venv; `pip install` works inside venv | ~10 min |
| I-9 | `apply_patches.sh` did not copy workload `.c` files to xv6 user/ — `_nlrun` failed to build | Extended script to also copy `*_burner.c`, `wrunner.c`, `nlrun.c` and to patch Makefile `UPROGS` idempotently | ~30 min |
| I-10 | `.env.example` modified to contain real key by accident — security risk | Restored placeholder, rotated key (W11) | ~15 min + key revoke |

[TODO add W12+ issues as they happen]

---

## 4. Key design choices and rationale

### 4.1 LLM downstream of a single integer
**Choice:** Solar's JSON output is reduced to one int in `{0,1,2}`
before crossing into kernel space.
**Why:** This is what makes the project not a "thin LLM wrapper." It
forces the OS to retain decision authority. Any malicious or
hallucinated LLM output is bounded — at worst it picks the wrong
queue, which MLFQ demotion corrects within a few ticks.

### 4.2 `forkpri` instead of `fork` + `setpri`
**Choice:** Single atomic syscall.
**Why:** Avoids the race window where a child runs at default priority
before being re-tiered. Important for short-lived processes whose
total runtime might be smaller than the race window.

### 4.3 Heuristic fallback
**Choice:** `nl_shell.py` and `llm_hint.py` both fall back to a
deterministic keyword classifier when the API key is missing or the
call fails.
**Why:** (a) Demos can't break from network blips, (b) anyone can
clone the repo without an API key and still run the code, (c) provides
a meaningful baseline to compare Solar against in evaluation.

### 4.4 Per-process priority (for now)
**Choice:** Priority stored in `struct proc`, not in a per-thread
struct.
**Why:** xv6 stock has no threads. Per-process keeps `scheduler()`
simple. If the thread track adds clone semantics in W13, we'll revisit.

---

## 5. Tools and environment

| Tool | Use |
|---|---|
| xv6-riscv (MIT) | Target OS kernel |
| `riscv64-unknown-elf-gcc` | Cross-compiler |
| QEMU `qemu-system-riscv64` | Emulator |
| WSL2 Ubuntu | Build host (Windows-side editor) |
| Python 3.12 + venv | Host scripting |
| `openai` SDK (1.x) | Solar API client |
| `matplotlib` | Charts |
| `nano` / VS Code | Editing |
| Git | Version control (per team repo) |
| Claude Code | Pair-programming assist (allowed by course) |

---

## 6. Time accounting

| Phase | Estimated | Actual (so far) |
|---|---|---|
| W9 orientation | 4 hr | 5 hr |
| W10 architecture + initial patches | 12 hr | 14 hr |
| W11 MLFQ core + NL shell + demo | 16 hr | 18 hr |
| W12 evaluation | 10 hr | [TODO] |
| W13 integration + dry run | 8 hr | [TODO] |
| W14 polish + presentation | 8 hr | [TODO] |
| **Subtotal** | **58 hr** | **[TODO]** |

[TODO update weekly. Tracked from git commits + work log.]

---

## 7. Risks tracked at start of project — what actually happened

| Risk anticipated in W9 | What actually happened |
|---|---|
| Kernel panic during multi-CPU work | Mitigated by `boost_lock` patch; no panics in W11 demo |
| LLM API unavailable on demo day | Heuristic fallback handles it; never needed in W11 |
| Trace volume swamps console | Mitigated by `settrace` toggle (default off) |
| Solo workload too large | On track — kernel side is the heavy part and is done by W11 |
| Team integration mismatches | Deferred to W13 by mutual agreement; risk recorded for that week |

---

## 8. Honest retrospective (running notes)

- **What worked:** Reading the stock scheduler line-by-line before
  patching. Locking down a stable trace format early.
- **What hurt:** Cross-platform editing (Windows ↔ WSL). 3+ separate
  sync accidents (viz.py indent, .env directory, missing apply_patches
  workload copy). Each fixed in <30 min but cumulatively painful.
- **What I'd do differently:** Treat Windows as canonical from day 1.
  Commit to git on the Windows side and `git pull` on the WSL side
  instead of manual `cp`.

---

## 9. Acknowledgements

- Course staff for the xv6 starting tree and lecture material
- Upstage for Solar Pro 3 API access (per course distribution)
- Team members for parallel work on the other three tracks
- Claude Code (used per the course's permitted-tools policy)
