# W11 Demo Runbook — Natural-Language Shell over Smart-MLFQ

This runbook gives you the exact step-by-step for the W11 presentation
demo: from booting xv6 to capturing the TRACE log that proves the
MLFQ scheduler honoured the LLM's queue hint.

Roles: **You = scheduler track**. The demo only exercises the
scheduler slice (NL → queue hint → forkpri → MLFQ). Other team members'
modules are out of scope for this demo.

---

## 0. Prereqs (one-time)

WSL / Linux side (`~/smart-mlfq-host` and the xv6 build tree):

```bash
# 0.1 Patch xv6 with the latest workload programs (now includes nlrun)
cd ~/smart-mlfq-xv6-patches    # wherever your patches dir is
./apply_patches.sh ~/lecture-repo/xv6-riscv

# 0.2 Build xv6
cd ~/lecture-repo/xv6-riscv
make clean
make

# 0.3 Confirm nlrun built
ls user/_nlrun         # should exist
```

Host Python side:

```bash
cd ~/smart-mlfq-host
source venv/bin/activate            # or .venv/bin/activate
python nl_shell.py --once "test"    # heuristic path works without API key
```

If you want Solar LLM (not heuristic) during the demo:
```bash
echo "UPSTAGE_API_KEY=sk-..." > .env
echo "UPSTAGE_MODEL=solar-pro3" >> .env
echo "UPSTAGE_REASONING_EFFORT=low" >> .env
```

---

## 1. Demo storyline (3 scenarios, ~3 minutes total)

### Scenario A — Interactive request (HIGH queue)
**Goal:** show that a "quick / interactive" NL request lands in queue 0.

Host:
```bash
python nl_shell.py --once "Just print hello quickly, I'm waiting"
```

Expected output (heuristic mode shown — Solar will look similar):
```
  source     : heuristic
  program    : echo hello
  queue_hint : 0 (HIGH)
  reason     : Heuristic: fast/short keywords -> HIGH
  xv6 cmd    : $ nlrun 0 echo hello
```

In QEMU xv6:
```
$ settrace 0 1      # turn on TRACE if not already on (depends on your patch)
$ nlrun 0 echo hello
TRACE tick=... pid=... pri=0 slice=...
hello
nlrun: child pid=... done (queue=0, status=0)
```

Talking point: *"LLM decided HIGH because user wants low latency. The
scheduler accepted that hint through `forkpri(0)` — TRACE confirms
`pri=0` from the first tick."*

---

### Scenario B — Background heavy job (LOW queue)
**Goal:** show that a "background / heavy" NL request lands in queue 2.

Host:
```bash
python nl_shell.py --once "Run a heavy computation in the background"
```

Expected:
```
  program    : cpu_burner 1000000
  queue_hint : 2 (LOW)
  xv6 cmd    : $ nlrun 2 cpu_burner 1000000
```

In QEMU:
```
$ nlrun 2 cpu_burner 1000000
TRACE tick=... pid=... pri=2 slice=...
cpu_burner pid=... done acc=...
nlrun: child pid=... done (queue=2, status=0)
```

Talking point: *"Same LLM, same forkpri syscall, different decision —
because the user signalled it's a background job. The scheduler doesn't
care WHY; it just acts on the queue hint."*

---

### Scenario C — Contention (HIGH preempts LOW)
**Goal:** show MLFQ priority preemption visually.

Run both in xv6 (the second one while the first is still going):
```
$ nlrun 2 cpu_burner 2000000 &     # background heavy
$ nlrun 0 echo "urgent"            # interactive, should preempt
```

Capture the TRACE output into a log on the host side:

```bash
# host — start QEMU with logging
make qemu 2>&1 | tee ~/smart-mlfq-host/w11_contention.log
```

Then parse + visualize:
```bash
cd ~/smart-mlfq-host
python parse_trace.py w11_contention.log --output w11_contention.json
python viz.py --baseline w11_contention.json --llm w11_contention.json \
              --out-dir charts_w11
```

(Using the same file twice in viz just produces the Gantt; we don't need a
baseline comparison for this scenario.)

Talking point: *"Look at the Gantt — green bar (HIGH) cuts in front of
red bar (LOW). That's the MLFQ priority order in action."*

---

## 2. Recording the demo

Use one of:
- WSL: `script -c 'make qemu' demo.cast`  (typescript recording)
- Native terminal recording: `asciinema rec w11_demo.cast`
- Screen capture: OBS / Windows built-in Game Bar (Win+G)

Aim for ~90 seconds total. Don't film typing — pre-type the commands
into a notes file and paste during recording.

---

## 3. What to say in 3 minutes

1. (20s) *"Problem: users don't speak `nlrun 2 cpu_burner 1000000`. They
   say 'do something in the background.' Goal: bridge that gap, but
   keep the actual scheduling decision in the kernel."*
2. (20s) Block diagram: NL input → Solar API → `{cmd, args, queue_hint}` →
   `nlrun` (user prog) → `forkpri()` syscall → MLFQ scheduler.
3. (60s) Scenario A and B back to back. Show host output, show TRACE in
   QEMU.
4. (45s) Scenario C — the contention case with the Gantt chart.
5. (15s) *"Solar only does NL → JSON. Every scheduling decision happens
   in the xv6 kernel — `proc.c` `scheduler()`, `trap.c` for demotion,
   `forkpri()` syscall in `sysproc.c`. That's the OS layer."*

---

## 4. Fallback if Solar API is down

The `nl_shell.py` automatically falls back to a keyword heuristic.
For the demo, you can intentionally **leave `UPSTAGE_API_KEY` unset** to
demonstrate the heuristic — same JSON shape, same downstream behaviour.
Mention this as a "robustness" bullet.

---

## 5. Known caveats (mention in Q&A if asked)

- xv6 has no networking, so Solar must be called from the host. The
  `nlrun` user program only sees the queue hint, not the natural-language
  text. This is deliberate: the kernel never trusts the LLM directly.
- `forkpri` was added in our Smart-MLFQ patches (it's not stock xv6).
- The current heuristic / few-shot prompt covers ~5 programs. Adding new
  programs requires editing `prompts.py` only — kernel side is unchanged.

---

## 6. Files involved (point to these in the slides)

| Layer | File | Purpose |
|---|---|---|
| Host UX | `smart-mlfq-host/nl_shell.py` | REPL, calls Solar, prints xv6 cmd |
| Host LLM prompt | `smart-mlfq-host/prompts.py::NL_TO_SPEC_PROMPT` | NL → spec JSON |
| xv6 user | `user/nlrun.c` | parse queue + program, `forkpri` + `exec` |
| xv6 kernel | `kernel/sysproc.c::sys_forkpri` | priority-aware fork syscall |
| xv6 kernel | `kernel/proc.c::scheduler` | 3-level MLFQ selection |
| xv6 kernel | `kernel/trap.c` | tick-driven demotion |
