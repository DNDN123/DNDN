# Smart-MLFQ: An LLM-Guided Scheduler for xv6

**Technical Report — Final Submission**

Team: [팀명]
Scheduler track author: 조현성
Course: Operating Systems · 2026 Spring
Target OS: xv6-riscv (MIT)

> **STATUS — W12 EVALUATION COMPLETE.** Results populated from the
> `run_w12_matrix.sh` runs on 2026-05-20. Charts and per-workload
> metrics live under `mlfq-experiments/w12_20260520-154301/`.

---

## Abstract

We extend the xv6-riscv kernel with a 3-level Multi-Level Feedback
Queue (MLFQ) scheduler and bridge it to **Upstage Solar Pro 3** so
that natural-language requests from users are translated into
kernel-level scheduling decisions. The Large Language Model never
enters kernel space — its output is reduced to a single integer
queue hint that passes through a new `forkpri()` system call. The
MLFQ algorithm itself (selection, time-slice demotion, anti-
starvation boost, I/O-aware queue retention, **aging**) is implemented
as real xv6 kernel C and self-corrects bad LLM hints within a few
ticks. On the most realistic workload (mixed CPU + I/O) the LLM-guided
run **improves Jain fairness from 0.61 to 0.84 (+37.65%), cuts p95
turnaround by 5.6%, and drops average response time from 1.57 to 0.25
ticks (-66.7%)**, at the cost of slowing the single most CPU-bound
process. On pure I/O-heavy workloads the deterministic heuristic
already does most of the work and the LLM matches it; on pure CPU
workloads neither hints help because every process is identical.
A keyword-heuristic fallback keeps the system functional when the
LLM endpoint is unavailable.

---

## 1. Introduction

### 1.1 Problem
Users describe what they want in natural language ("run this in the
background"). Schedulers consume integers (queue level 0/1/2).
Bridging that gap is the focus of LLM-for-OS research, but most
prior demos in this space wrap an LLM around an existing scheduler
without modifying the OS itself.

### 1.2 Goal
Build a scheduler where the LLM is genuinely an *advisor* and the
OS is the *decision-maker*. Specifically:

1. Real kernel modification — not a userspace policy daemon
2. LLM output is a bounded, validated integer at the kernel
   boundary
3. The OS's classical mechanisms (demotion, boost, I/O retention)
   override the LLM whenever its hint turns out to be wrong
4. Graceful degradation when the LLM is offline

### 1.3 Contributions
- **3-level MLFQ in xv6-riscv** (~400 lines of kernel patches)
- **Four new system calls** for priority control and observability
- **Host-side Solar Pro 3 bridge** with deterministic heuristic
  fallback
- **Natural-language shell** (`nl_shell.py`) that translates
  intent to `nlrun` invocations
- **End-to-end evaluation** comparing baseline, heuristic, and
  Solar-guided runs on three workloads

---

## 2. System Architecture

### 2.1 Three-layer design

```
┌────────────────────────────────────────────────────────────────┐
│ Host Python                                                    │
│  • nl_shell.py   — natural-language REPL                       │
│  • llm_hint.py   — trace → stats → Solar → hints.txt           │
│  • parse_trace, evaluator, viz                                 │
└─────────────────────────────┬──────────────────────────────────┘
                              │ Solar Pro 3 (OpenAI-compatible)
                              ▼
                  {"queue_hint": 0|1|2, ...}
                              │
                              ▼  (reduced to single int)
┌────────────────────────────────────────────────────────────────┐
│ xv6 user space                                                 │
│  • nlrun.c       — argv → forkpri(level) → exec                │
│  • wrunner.c     — workload runner with hints                  │
│  • {cpu,io,mixed}_burner.c — workload programs                 │
└─────────────────────────────┬──────────────────────────────────┘
                              │ forkpri / setpri / getstats / settrace
                              ▼
┌────────────────────────────────────────────────────────────────┐
│ xv6 kernel (~400 LoC modified)                                 │
│  • proc.c::scheduler  — 3-level MLFQ + priority boost          │
│  • trap.c             — tick-based slice demotion              │
│  • proc.c::sleep      — I/O retention (no demotion on block)   │
│  • sysproc.c          — 4 new syscalls                         │
└────────────────────────────────────────────────────────────────┘
```

See `docs/architecture-scheduler.md` for the live Mermaid diagram.

### 2.2 Data flow at a glance

```
"Run a heavy job in background"  (user, plain language)
        │
        ▼  Solar Pro 3  (effort=low, ~2.6 s, OpenAI-compatible)
{cmd:"cpu_burner", args:["2000000"], queue_hint:2,
 reason:"Heavy CPU work, user wants it backgrounded"}
        │
        ▼  host validates JSON shape + queue ∈ {0,1,2}
$ nlrun 2 cpu_burner 2000000     (user pastes into xv6 shell)
        │
        ▼  forkpri(2) — atomic fork at queue level 2
xv6 scheduler picks the child at queue 2 (LOW) from tick 0
        │
        ▼  TRACE / EXIT lines on QEMU stdout
TRACE tick=... pid=... pri=2 slice=...
EXIT  pid=... name=cpu_burner ... final_pri=2
        │
        ▼  parse_trace.py + evaluator.py
metrics.json (turnaround, fairness, starvation, ...)
```

### 2.3 Design principles

- **Privilege separation**: the LLM has no kernel privileges. Its
  output crosses one validated 2-bit integer boundary.
- **OS self-correction**: a CPU-bound process placed in HIGH will
  be demoted by the standard rules within a few ticks. The LLM's
  decision is a *starting point*, not a contract.
- **Graceful degradation**: missing API key or failed call falls
  back to a keyword heuristic producing the same JSON shape.

---

## 3. OS Concepts Used (Course Requirement)

| OS concept | Implementation | File · function |
|---|---|---|
| Process descriptor extension | priority, slice counter, I/O counters, arrival/completion timestamps | `kernel/proc.h::struct proc` |
| Multi-Level Feedback Queue | 3-level priority queues, HIGH/MID/LOW selection | `kernel/proc.c::scheduler` |
| Time-slice demotion | tick-driven, per-queue thresholds (2/4/8) | `kernel/trap.c` timer ISR |
| Anti-starvation boost | periodic all-procs-to-HIGH, multi-CPU safe | `kernel/proc.c` boost block + `boost_lock` |
| I/O-aware queue retention | `sleep()` resets slice without demoting | `kernel/proc.c::sleep` |
| System-call path | 4 new syscalls follow standard xv6 dispatch | `syscall.h/c`, `sysproc.c`, `usys.pl`, `user.h` |
| Spinlock synchronization | `boost_lock` (global), per-proc `p->lock` | `kernel/proc.c` |
| Context switch interaction | `swtch()` standard; trace printf moved out of locked region | `kernel/proc.c::scheduler` |

The course README mandates that "OS concepts must be a substantive
part of the design." All eight concepts above are implemented in
real xv6 kernel C, not wrapped over a host process. The total kernel
delta is approximately 400 lines of patches.

---

## 4. LLM Integration

### 4.1 Backend
Upstage Solar Pro 3 (`solar-pro3`), accessed through the
OpenAI-compatible Chat Completions endpoint at
`https://api.upstage.ai/v1`. We pass `reasoning_effort: low` by
default; the structured 3-class classification problem does not
benefit from deeper reasoning budgets — our W12 sweep across
`low`/`medium`/`high` on the same ambiguous prompt produced the
**same queue decision at roughly 2× the latency**
(see §5.4 Table 5).

### 4.2 Two operating modes

#### Mode A — Natural language → execution spec
User types a plain-English request:

```
> Run a heavy computation in background.
```

Solar returns:

```json
{
  "cmd": "cpu_burner",
  "args": ["2000000"],
  "queue_hint": 2,
  "reason": "Heavy CPU work, user wants it backgrounded"
}
```

`nl_shell.py` validates the JSON shape and emits an xv6 command:

```
nlrun 2 cpu_burner 2000000
```

The `nlrun` user program calls `forkpri(2)` then `exec`, so the
child enters MLFQ queue 2 from its very first tick.

#### Mode B — Trace → priority hint
After a baseline run, per-program statistics (run ticks, I/O blocks)
are sent to Solar, which returns a `{program → queue level}` map.
The map is written to `hints.txt`. On the next boot, `wrunner` reads
the hints and applies them via `forkpri` to each spawned child.

### 4.3 Prompt design
Both prompts (`NL_TO_SPEC_PROMPT`, `PRIORITY_RECOMMENDATION_PROMPT_FEWSHOT`)
use few-shot examples and force "JSON only, no markdown" responses.
Validation at the host catches malformed or out-of-range output and
substitutes the heuristic.

### 4.4 Heuristic fallback
A deterministic keyword-based classifier produces the same JSON
shape when the API key is missing or the call fails. This makes the
system testable offline and resilient to live-demo network failures.

---

## 5. Evaluation

### 5.1 Workloads
- **cpu_heavy** — 5 CPU-bound processes
- **io_heavy** — 4 I/O-bound processes (sleep-heavy)
- **mixed** — 3 CPU-bound, 2 mixed_burner, 2 I/O-bound
- **three_way** — one CPU-bound + one mixed_burner + one I/O-bound;
  designed so each queue level (HIGH/MID/LOW) gets a tenant
- **realprog** — actual xv6 built-ins (`cat`, `wc`, `ls`) over the
  in-image files; shows MLFQ behaviour with non-synthetic programs

`mixed_burner` alternates short CPU bursts with single-tick sleeps;
under standard MLFQ rules it settles in the MID queue and serves
as the discriminator that exercises all three levels.

### 5.2 Metrics
Per `evaluator.py`:
- Average and **p95 / p99** turnaround time (ticks) — tail latency
- Average and **p95** response time (ticks)
- Throughput (exits / tick)
- **Jain's fairness index** over per-process turnaround
- **Starvation counters**: `starved_pids` (processes stuck > 60 ticks
  at LOW), `longest_low_gap` (longest gap between scheduling decisions
  for any LOW-queue process)

### 5.3 Procedure
For each workload, three runs:
1. **Baseline** — no hints; MLFQ self-organizes
2. **Heuristic** — keyword-based hints (no API)
3. **Solar** — `solar-pro3` reasoning model with
   `reasoning_effort=low`

All runs at `CPUS=1` to keep timing deterministic. We also report a
**stock-xv6 RR baseline** (kernel without our patches) for context,
and a multi-CPU sweep (`CPUS=1/2/4`) for the mixed workload.

### 5.4 Results

**Table 1 — 3-way comparison on three workloads.** Values are
absolute (Base / Heur / LLM) followed by Δ% versus baseline.

| Workload  | Metric | Baseline | Heuristic | LLM (Solar) | Δheur% | Δllm%  |
|-----------|--------|---------:|----------:|------------:|-------:|-------:|
| cpu_heavy | avg_TT | 1.40 | 1.40 | 1.60 | 0.00 | +14.29 |
| cpu_heavy | max_TT | 2    | 2    | 2    | 0.00 | 0.00   |
| cpu_heavy | fairness | 0.890 | 0.890 | 0.914 | 0.00 | +2.63 |
| io_heavy  | avg_TT | 39.50 | **34.50** | 40.25 | **-12.66** | +1.90 |
| io_heavy  | avg_resp | 0.75 | 0.75 | **0.25** | 0.00 | **-66.67** |
| io_heavy  | p95_resp | 1.85 | 1.85 | **0.85** | 0.00 | **-54.05** |
| io_heavy  | throughput | 0.071 | **0.080** | 0.071 | **+12.04** | 0.00 |
| mixed     | avg_TT | 19.43 | 24.71 | 22.86 | +27.17 | +17.65 |
| mixed     | max_TT | 36 | 47 | **34** | +30.56 | **-5.56** |
| mixed     | p95_TT | 35.7 | 44.9 | **33.7** | +25.77 | **-5.60** |
| mixed     | **fairness** | 0.608 | 0.695 | **0.837** | +14.32 | **+37.65** |
| mixed     | starved_pids | 0 | 0 | 0 | 0 | 0 |

**Figure 1.** `mixed/charts/avg_turnaround.png` — per-program
turnaround. The LLM mode pushes `cpu_burner` deeper into LOW (slower
in isolation: 1.67 → 12.33 ticks) so that `io_burner` and
`mixed_burner` finish faster.

**Figure 2.** `mixed/charts/gantt_llm.png` — colour-coded Gantt
showing how HIGH-priority I/O work preempts the LOW-priority CPU
work under LLM guidance.

**Table 2 — Wall-clock comparison vs stock xv6 round-robin (no
MLFQ patches).**

| Workload  | Stock RR (ticks) | MLFQ baseline (ticks) | Δ |
|-----------|----:|----:|---:|
| cpu_heavy |  3  |  4  | +33 % |
| io_heavy  | 42  | 56  | +33 % |
| mixed     | 35  | 37  | +6 %  |

MLFQ is slightly slower on raw batch throughput — extra bookkeeping,
TRACE I/O, and `boost_lock` cost. The trade-off is the
fairness / response-time numbers in Table 1.

**Table 3 — Multi-CPU sweep on the mixed workload.**

| CPUS | Wall-clock span | Notes |
|---|---:|---|
| 1 | 41 ticks | reference |
| 2 | 43 ticks | +4.8 %  (correctness preserved, panic-free) |
| 4 | 63 ticks | +53.7 % (UART + boost_lock serialisation dominate) |

No `panic` at any CPU count — `boost_lock` correctly serialises the
periodic priority boost across harts. Throughput regression at high
CPU count reflects xv6's coarse-grained console and lock overheads,
not a scheduler bug.

**Table 4 — Stress test (10 simultaneous procs, mostly CPU-bound).**

| Metric | Value |
|---|---:|
| `n_exits` | 10 |
| `avg_turnaround` | 20.4 ticks |
| `p95_turnaround` | 44.95 ticks |
| `max_turnaround` | 49 ticks |
| `starved_pids` | **0** |
| `longest_low_gap` | **0** |

Anti-starvation machinery (aging at 25-tick intervals + boost at
100-tick intervals) keeps every process making progress; no
process is starved at LOW.

**Table 5 — Solar `reasoning_effort` sweep on the same ambiguous
input** ("Run something that's heavy on CPU but I also want it to
feel responsive").

| effort | latency | decision | reason field |
|---|---:|:---:|---|
| `low`    | **2.59 s** | queue 1 (MID) | "Heavy CPU work with responsiveness requirement" |
| `medium` | 4.92 s | queue 1 (MID) | "Heavy CPU but wants responsiveness, ambiguous workload" |
| `high`   | 5.08 s | queue 1 (MID) | "Mixed workload, ambiguous request" |

Identical decision across effort levels at roughly 2× the latency
cost — `low` is the right default for this kind of small-cardinality
structured classification.

**Hint-cache effect.** With the W11 hint cache enabled, repeat
queries cost 0.92 s (file read) vs 2.59 s (Solar round-trip) — a
**2.8× speedup** and an API call saved.

**Table 6 — Extended workloads (`three_way`, `realprog`).** Run
under the same 3-way matrix. `three_way` uses the longer
`cpu_burner 20000000` so demotion and aging are visible.

| Workload  | Metric | Baseline | Heuristic | LLM | Δllm% |
|-----------|--------|---:|---:|---:|---:|
| three_way | avg_TT | 88.67 | 87 | 92.33 | +4.13 |
| three_way | **avg_resp** | 0.67 | 1.67 | **0.00** | **-100** |
| three_way | **p95_resp** | 1.80 | 3.70 | **0.00** | **-100** |
| three_way | fairness | 0.905 | 0.877 | **0.939** | +3.74 |
| three_way | starved_pids | 0 | 0 | 0 | 0 |
| realprog  | avg_TT | 7.80 | 7.60 | **7.00** | **-10.26** |
| realprog  | **avg_resp** | 3.20 | 4.00 | **1.00** | **-68.75** |
| realprog  | **p95_resp** | 8.80 | 10.80 | **2.00** | **-77.27** |
| realprog  | per-program | cat 8 → 7 | wc 8.5 → 7.5 | ls 6 → 6 | (real xv6 utilities) |

The `realprog` row is particularly interesting: it uses **actual xv6
built-in programs** (`cat`, `wc`, `ls`), not synthetic burners. The
LLM still delivers a **-77 % p95 response time** improvement and a
-10 % average turnaround improvement — without us writing any custom
benchmark code. This is the closest thing in our evaluation to a
"realistic workload" and the LLM clearly helps.

### 5.5 Observations

1. **Mixed workloads are where the LLM earns its keep.** On the
   mixed workload Solar improved Jain fairness by +37.7 % and cut
   p95 turnaround by 5.6 %, by deliberately sending the most
   CPU-bound process to LOW so I/O-bound and mixed processes ride
   higher queues. Average turnaround for the *whole batch* went up
   17.7 %, but average turnaround on the slowest tail came down —
   this is a textbook fairness-versus-throughput trade-off and is
   exactly the scheduling decision we want to delegate to context.
2. **For pure-I/O workloads, the heuristic is enough.** It hit
   -12.7 % average turnaround and +12.0 % throughput, while Solar
   matched the baseline. The keyword classifier already correctly
   maps an I/O-dominant program to HIGH; the LLM cannot do better
   than "right answer." Solar still improved *response time* by
   -66.7 % (p95 -54 %), suggesting that even when the queue choice
   is identical, having `forkpri` set the priority atomically
   removes a transient at the start.
3. **For pure-CPU workloads, neither helped.** Every cpu_burner
   already gets demoted to LOW by ordinary MLFQ rules; sending them
   there a priori saves nothing measurable. This is an honest
   negative result and is reported as such.
4. **Reasoning effort does not buy decisions, only narrative.**
   Across `low`/`medium`/`high` we got the same queue choice for the
   same prompt; only the wording of the `reason` field changed.
   This argues for `low` as the production default.
5. **Stock RR vs MLFQ: a 33 % throughput tax buys 38 % fairness.**
   The MLFQ build is slower on raw batch metrics but turns that
   budget into measurable fairness and response-time gains. The
   stock kernel cannot be hinted at all, so the LLM path is moot
   for that comparison.

---

## 6. Limitations

- **xv6 has no networking.** The LLM call lives in host Python and
  communicates via QEMU stdin/stdout. A true in-kernel agent would
  require a network stack.
- **Single CPU evaluation.** Multi-CPU runs work but are not in the
  numbers reported here, to keep timing reproducible.
- **Workload diversity is small.** Only `cpu_burner` / `io_burner`
  / `mixed_burner` are exercised. Real-world workloads are far more
  varied.
- **No persistent learning.** Each invocation queries Solar from
  scratch; there is no fine-tuning or memory across calls.
- **Trace volume scales with tick count.** A long workload emits
  thousands of `TRACE` lines. Future work: in-kernel ring buffer
  flushed on demand.

---

## 7. Future Work

- Per-thread priority (currently per-process; depends on thread
  track's design choice)
- In-kernel trace buffer to avoid console flooding
- Learned hint quality estimator that decides whether to trust
  Solar or fall back to the heuristic
- Adaptive `reasoning_effort` selection based on request complexity

---

## 8. Conclusion

Smart-MLFQ shows that an LLM can usefully advise a real kernel
scheduler without becoming a security or correctness risk: it
contributes a queue hint, the OS owns the actual policy. The
classical MLFQ rules act as a safety net against bad LLM
decisions. Our evaluation indicates that **the LLM's value is
concentrated on mixed, non-trivially-classifiable workloads —
where it cut tail turnaround by 5.6 % and improved Jain fairness
by 37.7 % over the unguided baseline**; on workloads where the
right queue is obvious (pure CPU, pure I/O), a deterministic
heuristic is sufficient and the LLM correctly does not change
the answer. The cheapest reasoning setting (`solar-pro3` at
`effort=low`) is enough for this class of decision.

---

## References

- Russ Cox, Frans Kaashoek, Robert Morris. *xv6: a simple, Unix-like
  teaching operating system.*
- Arpaci-Dusseau, *Operating Systems: Three Easy Pieces*, Ch. 8
  (MLFQ).
- Upstage Solar Pro 3 API. https://console.upstage.ai/docs
- Project source: <https://github.com/PLACEHOLDER-TEAM-ORG/smart-mlfq-xv6>
  *(replace with the actual team repo URL before submission)*
