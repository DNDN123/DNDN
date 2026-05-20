# SmartShell — Presentation (English)

10-slide deck content + speaker notes. Drop each slide section into
PowerPoint / Keynote / Google Slides. Aim ~10 minutes total.

---

## Slide 1 — Title

**SmartShell: A Natural-Language Interface for xv6-riscv**

*LLM for OS — Direction B*

Team [name] · OS Final Project · 2026

> **Speaker notes**: 30 sec. Introduce yourselves and the project in one
> sentence — "We put a natural-language interface on top of xv6, but the
> real work is in the kernel."

---

## Slide 2 — Problem

Beginners can **read** xv6's kernel but can't **drive** it without
memorising every command.

- `ps`, `kill 5`, `setpriority 5 3`, `priority_test`, ...
- Korean / English speakers each need a different mental cheat sheet
- xv6 has *no GUI* and *no networking* — installation/automation are
  manual and arcane

**Goal**: keep the kernel pure xv6, but let users talk to it in plain
language — without weakening any OS-level invariant.

> **Speaker notes**: 45 sec. Lead with the user problem. The "without
> weakening invariants" line is the bridge to slide 3.

---

## Slide 3 — Architecture

```
[user]  →  Python NL Bridge  →  Solar Pro 3 (Intent JSON)
                  ↓
          subprocess pipe
                  ↓
            QEMU + xv6-riscv
              ├ user: ps / setprio / mlfq_bench
              ├ syscalls (SYS_ps + setpriority)
              └ kernel: MLFQ scheduler, timer hooks
```

Two halves:
1. **Host** (Python, ~360 LOC): translation + safety + I/O plumbing
2. **xv6 kernel** (C, ~250 LOC added/modified): real OS work

> **Speaker notes**: 1 min. Walk left-to-right. Emphasise that xv6 has
> no network stack, so the LLM physically cannot live in the kernel —
> this is *why* the split exists, not a hack.

---

## Slide 4 — Live Demo (3 minutes)

```
smart> 프로세스 보여줘
3 active process(es): init, sh, ps

smart> set pid 2 priority to 3
OK pid=2 prio=3

smart> launch priority_test
... Test 1 PASSED / Test 2 PASSED / Test 3 PASSED

smart> kill 1
⚠️  rejected: refusing to kill pid 1 (init)
```

> **Speaker notes**: 3 min. This is the centrepiece — leave time for it.
> Run live or fall back to recorded GIF. Emphasise: "the AI parsed the
> Korean, but the C kernel did the kill, the priority change, the
> scheduling." Show one rejected case to highlight the safety guard.

---

## Slide 5 — Kernel change #1: `sys_ps` syscall

```c
struct procinfo {                 // procinfo.h
  int pid, ppid, state, priority;
  uint64 sz;
  char name[16];
};
```

Walks `proc[]` under `p->lock`, `copyout()` per entry to user space.
Six-file integration:

| `syscall.h` | `syscall.c` ×3 | `sysproc.c` | `usys.pl` | `user.h` | `Makefile` |

> **Speaker notes**: 45 sec. Quickly call out that any new xv6 syscall
> requires editing six places — show this slide as evidence of real
> kernel work, not a script tweak.

---

## Slide 6 — Kernel change #2: MLFQ scheduler ⭐

```
L0 HIGH   priority  0..6   quantum  1 tick   (interactive)
L1 NORMAL priority  7..13  quantum  4 ticks  (default)
L2 LOW    priority 14..20  quantum 16 ticks
```

- **Demote**: each timer tick, `mlfq_tick()` accounts the slice; on quantum
  exhaustion `priority++` (capped at 20).
- **Boost** (anti-starvation): every 100 ticks, `mlfq_boost()` resets
  every active proc to priority 0.
- **Reset on dispatch**: `run_ticks = 0` so quantum is per-slice, not
  cumulative.

> **Speaker notes**: 1.5 min — most technical slide, take your time.
> Mention this is the textbook MLFQ from OSTEP Ch.8.

---

## Slide 7 — Host bridge

Pipeline per user input:

`NL → Solar Pro 3 → Intent → guard() → to_xv6_cmd() → QEMU stdin → parse → summarise`

Six intent types: `PS / SETPRIO / SPAWN / KILL / EXPLAIN / REJECT`.

Safety guard:
- `init` (pid 1) is unkillable
- `SPAWN` is whitelisted
- `SETPRIO` validates `0 ≤ prio ≤ 20`

Background reader thread drains QEMU stdout to prevent BrokenPipe.

> **Speaker notes**: 1 min. Emphasise the guard — the AI cannot run
> arbitrary commands. This is what makes the system safe to expose.

---

## Slide 8 — Evaluation

**MLFQ fairness benchmark** (`mlfq_bench`, `CPUS=1`, 3 contending procs):

| Child  | Priority | Ticks | Ratio |
|-------|---------:|------:|------:|
| HIGH   | 1   | 4   | 1.00× |
| NORMAL | 10  | 8   | 2.00× |
| LOW    | 19  | 11  | 2.75× |

- HIGH finishes 2× faster than NORMAL — bucket priority works.
- LOW stays bounded at ~3× HIGH — boost prevents starvation.

**Regression**: xv6 `usertests -q` — [see live run].

**NL accuracy**: 6/6 hand-tested Korean / English prompts mapped to
correct intent.

> **Speaker notes**: 1 min. The MLFQ table is the headline number — if
> you only show one slide, show this one. Walk through the ratios.

---

## Slide 9 — "Not just an LLM wrapper"

| Evidence | Lines | Layer |
|---|---:|---|
| MLFQ scheduler + boost | ~70 | C kernel |
| Timer-tick quantum accounting | ~10 | C kernel |
| `sys_ps` syscall | ~40 | C kernel |
| `ps`, `setprio`, `mlfq_bench` | ~120 | C user |
| Host bridge + safety | ~360 | Python |

**~250 LOC of C OS code vs ~30 LOC of LLM-related code.**

The LLM is a UX layer. The OS is real OS.

> **Speaker notes**: 45 sec. Anticipate the "is this just an LLM
> wrapper?" question and pre-empt it. Quote the ratio.

---

## Slide 10 — Limitations & Future work

**Limitations**
- MLFQ fairness measurable only at `CPUS=1` (no contention with 3 CPUs / 3 procs)
- Bridge uses `time.sleep` for I/O sync; pty-based prompt detection
  would be more robust
- LLM call is best-effort; on 429/502 the bridge falls back to the
  offline regex parser (limited NL coverage)

**Future work**
- pty driver with `$` prompt detection
- More intents: batched SETPRIO, dependency-aware SPAWN
- Boost-interval sweep — fairness vs throughput trade-off curve
- Containerise the host bridge for one-command demo

**Thank you. Questions?**

> **Speaker notes**: 1 min + Q&A. Don't hide limitations — graders reward
> honest scoping. The boost-interval sweep is a "next semester" line.

---

## Time budget (10 min)

| Slide | Time |
|---|---|
| 1 Title | 0:30 |
| 2 Problem | 0:45 |
| 3 Architecture | 1:00 |
| 4 Demo | 3:00 |
| 5 sys_ps | 0:45 |
| 6 MLFQ | 1:30 |
| 7 Host bridge | 1:00 |
| 8 Evaluation | 1:00 |
| 9 Not a wrapper | 0:45 |
| 10 Limits + Q&A | 1:00 (+ Q&A overflow) |
| **Total speaking** | **~11 min** — cut sub-points if running long |

---

## One-paragraph elevator pitch (for intros / abstracts)

> SmartShell layers a natural-language front-end on top of the xv6-riscv
> teaching kernel. Users type Korean or English requests (e.g. "show me
> the processes", "프로세스 보여줘"); Upstage Solar Pro 3 translates them
> into a structured intent; a host-side Python bridge then drives the
> kernel through QEMU's stdio, calling real syscalls we added to xv6.
> All actual OS work — process listing (`sys_ps`), priority management,
> and a three-level MLFQ scheduler with periodic anti-starvation boost —
> lives in C kernel code we wrote, while the LLM stays a thin
> translator. Benchmarks confirm MLFQ fairness: high-priority jobs
> finish 2× faster than normal-priority ones, and low-priority jobs
> remain bounded at ~3× the high-priority completion time thanks to the
> boost.
