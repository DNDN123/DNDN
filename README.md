# Smart-MLFQ on xv6 — Scheduler Slice

> An LLM-guided Multi-Level Feedback Queue scheduler implemented **inside the
> xv6-riscv kernel**, plus a host-side Python bridge that uses Upstage Solar
> Pro 3 to convert natural-language requests into kernel-level scheduling
> hints.
>
> **Team project · Direction B (LLM for OS) · 2026 Spring**
> Scheduler track: 조현성

This repository holds the **scheduler slice** of a four-person team project
whose larger goal is an intelligent shell on xv6 (natural-language commands +
custom task manager + system diagnosis). The other three slices
(process / syscall / thread) live in teammates' repositories; they are
integrated in Week 13. See `docs/planning/` for the historical decision
trail.

> **Canonical source of truth:** this Windows-side tree is the authoritative
> copy. WSL working copies are derived from it via `cp` or the project's
> sync helper. **Do not edit kernel/host files directly on WSL** — edit
> here, then re-sync. This prevents the kind of split-brain bugs we hit in
> W11 (two `viz.py` versions, a `.env` directory accident, etc.).
>
> **Public GitHub mirror (deliverable):** `<TEAM_REPO_URL>` — replace this
> placeholder with the actual team repository URL before submission. The
> repo must be **public** with no secret committed; verify with
> `git check-ignore .env` before pushing.

---

## Repository layout

```
.
├── README.md                    ← you are here
├── .gitignore
├── smart-mlfq-host/             ← host Python: LLM bridge, parsers, charts
│   ├── nl_shell.py              ← W11 natural-language REPL  ⭐
│   ├── prompts.py               ← Solar Pro 3 prompt templates
│   ├── parse_trace.py           ← xv6 log → JSON
│   ├── llm_hint.py              ← stats → Solar → hints.txt
│   ├── evaluator.py             ← turnaround / fairness metrics
│   ├── viz.py                   ← Gantt + bar charts
│   ├── DEMO_W11.md              ← runbook for the W11 progress demo
│   └── docs/                    ← architecture, slides, integration
├── smart-mlfq-xv6-patches/      ← xv6 kernel + user-program patches
│   ├── apply_patches.sh         ← drop on a clean xv6-riscv tree
│   ├── kernel/                  ← proc.c, trap.c, sysproc.c, ...
│   └── user/                    ← nlrun.c, wrunner.c, cpu_burner.c, ...
├── lecture-repo/                ← course materials (not modified by us)
└── docs/
    └── planning/                ← archived design / decision docs
```

The two **active** directories are `smart-mlfq-host/` and
`smart-mlfq-xv6-patches/`. Everything in `docs/planning/` is historical.

---

## Quick start

### 1. Apply patches and build xv6
```bash
cd smart-mlfq-xv6-patches
./apply_patches.sh /path/to/xv6-riscv
cd /path/to/xv6-riscv
make clean && make
```

### 2. Host Python
```bash
cd smart-mlfq-host
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
cp .env.example .env             # then put your real UPSTAGE_API_KEY
python hello_solar.py            # smoke test
```

### 3. Try the W11 natural-language flow
```bash
# host: get an xv6 command line from a natural request
python nl_shell.py --once "Run a heavy job in background"

# xv6 (in QEMU):
$ nlrun 2 cpu_burner 1000000
TRACE tick=... pid=... pri=2 slice=...      # MLFQ honoured the LLM's hint
EXIT  pid=... name=cpu_burner ... final_pri=2
```

Full runbook: [`smart-mlfq-host/DEMO_W11.md`](smart-mlfq-host/DEMO_W11.md).

---

## What's actually built

| Concern | Where | Status |
|---|---|---|
| 3-level MLFQ + demotion + boost | `kernel/proc.c`, `kernel/trap.c` | ✅ |
| I/O-aware queue retention | `kernel/proc.c::sleep` | ✅ |
| Multi-CPU safe priority changes | `boost_lock`, per-proc lock | ✅ |
| New syscalls (4) | `setpri` / `getstats` / `settrace` / `forkpri` | ✅ |
| Natural language → execution spec | `nl_shell.py` + Solar Pro 3 | ✅ |
| Heuristic fallback (no API key) | `nl_shell.py` | ✅ |
| Trace parser + evaluator + charts | `parse_trace.py`, `evaluator.py`, `viz.py` | ✅ |
| Quantitative 3-way comparison | (W12 — in progress) | 🟡 |
| Team integration | (W13) | ⏳ |
| Final report + demo video (English) | (W14) | ⏳ |

For the full OS-concept-to-file mapping (a course requirement), see
[`smart-mlfq-host/docs/architecture-scheduler.md`](smart-mlfq-host/docs/architecture-scheduler.md).

---

## Why this design

- **The LLM never enters kernel space.** Solar Pro 3 produces JSON; the
  kernel only ever sees an integer in `{0, 1, 2}` arriving via the
  `forkpri()` syscall. Any invalid value is rejected at the syscall
  boundary.
- **The OS layer self-corrects bad hints.** A CPU-bound process placed in
  HIGH will be demoted by the standard MLFQ rules within a few ticks. The
  LLM only nudges the *initial* queue.
- **Graceful degradation when the LLM is unavailable.** `nl_shell.py`
  falls back to a deterministic keyword heuristic that produces the same
  spec shape, so the demo cannot be broken by a network blip.

---

## Course requirements check

| Requirement (Week 9 README) | Met by |
|---|---|
| OS concepts as **substantive** part of design | MLFQ scheduler, syscall path, spinlock sync, sleep/wakeup all in real xv6 kernel C |
| **Not** a thin LLM wrapper | LLM output is reduced to a 2-bit integer before reaching kernel |
| Public GitHub repo with setup + run + demo | This README + `DEMO_W11.md` |
| Final deliverables in English | Slides and report use English (in progress) |
| Solar Pro 3 backend | Yes — `UPSTAGE_MODEL=solar-pro3` default in `.env.example` |

---

## License & credits

xv6-riscv: MIT (Massachusetts Institute of Technology). Our patches
build on top of the unmodified MIT xv6-riscv tree provided by the course.

Solar Pro 3 is a product of Upstage. We use it via the
OpenAI-compatible Chat Completions endpoint.
