# W11 Progress Slides — Scheduler Track

Speaker: 조현성 (scheduler role)
Time budget: ~3 minutes for this track
Language: English (per course requirement)

Copy each `## Slide N` block into your slide deck. Speaker notes (italic)
go in the "presenter notes" pane, not on the slide.

---

## Slide 1 — Title

```
Smart-MLFQ: An LLM-Hinted Scheduler for xv6
W11 Progress Report — Scheduler Track

조현성 / Team [팀명]
LLM-for-OS · 2026 Spring
```

*Speaker: "I'm in charge of the scheduler track. Today I'll show how
natural-language requests translate into kernel-level scheduling
decisions on xv6, in three minutes."*

---

## Slide 2 — The problem

```
Users say:                     The OS expects:
─────────────                  ───────────────
"do it in the background"      nlrun 2 cpu_burner 1000000
"just print fast"              nlrun 0 echo hello

Gap: humans don't write queue levels.
Goal: bridge intent → kernel decisions without making
      the kernel trust an LLM directly.
```

*Speaker: "Top half is how users think. Bottom half is what the kernel
actually needs. Our scheduler slice fills the gap — but carefully: the
LLM never enters kernel space."*

---

## Slide 3 — System diagram (scheduler slice)

```
┌───────────────┐    ┌──────────┐    ┌──────────────────┐
│ User (NL)     │───►│ nl_shell │───►│ Solar Pro API    │
└───────────────┘    │  (host)  │◄───│ JSON: queue_hint │
                     └────┬─────┘    └──────────────────┘
                          │ "nlrun <q> <cmd> ..."
                          ▼
                  ┌───────────────────┐
                  │  xv6 user: nlrun  │
                  │  forkpri(level)   │
                  └─────────┬─────────┘
                            ▼
                  ┌───────────────────┐
                  │  MLFQ scheduler   │  ← kernel/proc.c
                  │  trap.c demotion  │  ← kernel/trap.c
                  └─────────┬─────────┘
                            ▼
                  TRACE / EXIT log → host parser → charts
```

*Speaker: "Three layers. Host Python talks to Solar. The LLM's output is
boiled down to one integer that crosses into the kernel via forkpri.
Everything after that — selection, demotion, boost — is real xv6 kernel
C code that we wrote."*

(Replace the ASCII with the Mermaid diagram from `architecture-scheduler.md`
when you build the actual deck.)

---

## Slide 4 — What lives in the kernel

```
struct proc additions       priority, time_slice_used, total_run_ticks,
                            total_io_blocks, arrival_tick, ...

scheduler() (proc.c)        HIGH→MID→LOW selection
                            + periodic priority boost (boost_lock)

trap.c timer ISR            tick-driven demotion when slice exhausted
                            slices = {HIGH: 2, MID: 4, LOW: 8}

sleep() patch (proc.c)      I/O block does NOT demote — keeps I/O-bound
                            workloads out of LOW

4 new syscalls              forkpri  setpri  getstats  settrace
```

*Speaker: "Six OS concepts in one slide: process descriptor extension,
multi-level queue selection, time-slice demotion, anti-starvation boost,
I/O-aware queue retention, and the syscall path. All implemented as
xv6 kernel C — about 400 lines of patches."*

---

## Slide 5 — Demo (scenario A: HIGH queue)

```
$ python nl_shell.py --once "print hello quickly, I'm waiting"
  source     : solar
  program    : echo hello
  queue_hint : 0 (HIGH)
  reason     : Short interactive task, latency sensitive
  xv6 cmd    : $ nlrun 0 echo hello

xv6:
  $ nlrun 0 echo hello
  TRACE tick=… pid=… pri=0 slice=…   ← HIGH from tick 0
  hello
  nlrun: child pid=… done (queue=0, status=0)
```

*Speaker: "User asks for speed. LLM picks queue 0. The forkpri syscall
puts the child directly in HIGH — TRACE confirms pri=0 from its very
first tick. No race window."*

---

## Slide 6 — Demo (scenario B: LOW queue)

```
$ python nl_shell.py --once "Run a heavy computation in background"
  queue_hint : 2 (LOW)
  xv6 cmd    : $ nlrun 2 cpu_burner 1000000

xv6:
  $ nlrun 2 cpu_burner 1000000
  TRACE tick=… pid=… pri=2 slice=…   ← LOW from start
```

*Speaker: "Same LLM, same syscall path, different decision — purely
because the user said 'background.' The kernel doesn't reason about
intent; it just applies the integer it was given."*

---

## Slide 7 — Demo (scenario C: HIGH preempts LOW)

```
xv6:
  $ nlrun 2 cpu_burner 2000000 &      ← background heavy
  $ nlrun 0 echo "urgent"             ← interactive, preempts
```

```
Gantt (from viz.py):
  pid 4 (echo, q=0)      █             ← HIGH grabs CPU
  pid 3 (cpu_burner, q=2) ████___█████ ← LOW yields, resumes
```

*Speaker: "This is MLFQ priority in action — the HIGH process preempts
the LOW one. We didn't write extra scheduling code for this demo; the
kernel does it automatically because the priorities are correctly set."*

---

## Slide 8 — Robustness: heuristic fallback

```
solar API unavailable?  →  keyword heuristic produces identical JSON shape

  "background" / "heavy" / "long"   →  queue 2
  "fast" / "quick" / "now"          →  queue 0
  i/o keywords                       →  queue 0
  ambiguous                          →  queue 1

Same downstream — same nlrun command — same kernel path.
```

*Speaker: "If the Solar endpoint goes down during the live demo, we
fall back to a deterministic heuristic. Same JSON shape, same xv6
command line, same kernel behaviour. The demo cannot be broken by a
network blip."*

---

## Slide 9 — Status and next steps

```
DONE (W10–W11)
  ✓ MLFQ kernel patches (3-level + demotion + boost + I/O retention)
  ✓ 4 syscalls wired through user→kernel
  ✓ NL → spec prompt (Solar few-shot + heuristic fallback)
  ✓ nl_shell.py host REPL
  ✓ nlrun user program (xv6)
  ✓ End-to-end demo runbook + recording plan

NEXT (W12–W13)
  • Quantitative evaluation: avg turnaround vs queue-hint accuracy
  • Team integration: hand off forkpri/setpri to process & syscall tracks
  • Diagnostic layer (other tracks) talks to getstats over our boundary
```

*Speaker: "Scheduler slice is feature-complete for W11. W12 is measurement,
W13 is integration with the rest of the team. The interfaces I expose
— forkpri, setpri, getstats, settrace — are already documented and
should not change."*

---

## Slide 10 — Q&A primer (do not show; speaker reference only)

Likely questions and prepared answers:

- **"Why not let the LLM call setpri directly?"** Because the kernel
  shouldn't trust an LLM with arbitrary syscall args. Routing through a
  single integer (`queue_hint`) bounds the blast radius — invalid values
  are rejected at the syscall boundary.

- **"What's the OS concept here? Isn't this just calling an API?"** No.
  The LLM only produces a queue hint. The MLFQ algorithm — selection,
  demotion, boost, I/O retention — runs entirely in xv6 kernel C. ~400
  LoC of real kernel modifications, not a wrapper.

- **"How do you measure success?"** Two ways. (1) Hint accuracy: does
  the LLM's queue choice match what a human expert would have picked?
  (2) System metrics: turnaround time, fairness (Jain index), throughput,
  measured by parse_trace.py + evaluator.py.

- **"What if Solar makes a bad call?"** The MLFQ self-corrects: a CPU-
  bound process placed in HIGH will be demoted within a few ticks. The
  LLM only sets the *initial* queue; the scheduler still owns the long-
  term decision.

- **"Why xv6 instead of Linux?"** Course requirement, and xv6 lets us
  modify the actual scheduler in C. On Linux we'd be limited to userspace
  hints via `nice`/`sched_setscheduler`, which is exactly the kind of
  "thin wrapper" the rubric warns against.
