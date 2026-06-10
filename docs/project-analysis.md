# Project Analysis — From vanilla xv6 to an OS that "speaks natural language"

> Read this one file to understand **what this project is and how it was built**.
> No code knowledge needed — it goes big-picture first, then into detail.
> (Korean version: [`프로젝트-분석.md`](프로젝트-분석.md))

---

## 0. One-line summary

> **On top of xv6 (a tiny teaching OS), we built a system where you speak plain
> language and the OS understands and executes it.**
> The "understanding" is done by a small AI model we fine-tuned ourselves; the
> **actual work (process management, scheduling) is done by OS features we
> implemented directly in the kernel.**

---

## 1. What is this? (by analogy)

A normal OS is commanded like this:
```
$ killall cpu_burner        ← you must memorize the exact command
```

What we built works like this:
```
$ 무거운 프로세스 정리해줘     ← just say it in plain language ("clean up the heavy processes")
killed heaviest pid 8 ...    ← the OS understands and executes
```

**Analogy**: if the OS is an "employee who only understands a foreign language,"
we placed a **translator (AI)** in front of them.
- The boss (user) says "clean up the heavy stuff" in Korean/English,
- the translator (AI) renders it into the employee's language: `killheavy`,
- a **security guard (safety guard)** checks it isn't dangerous,
- and the employee (OS kernel) actually does the work.

The key: **the translator never enters the building (kernel).** The translator
stays outside the door; only the short translated command crosses in.

---

## 2. Vanilla xv6 vs what we built

| | **Vanilla xv6 (starting point)** | **Our system (result)** |
|---|---|---|
| Scheduler | Round-robin (everyone one turn each) | **3-level MLFQ** (CPU-heavy procs auto-demote) |
| Syscalls | 21 (fork, exec, kill...) | **+15 new** (ps, setpri, reap, threads, futex...) |
| Command input | Exact commands only | **Natural language → auto-translate & run** |
| Process mgmt | kill one by one | ps / killall / killheavy / reap (bulk cleanup) |
| Safety | None | **Safety guard** (protect init, gate destructive ops) |
| Concurrency | Processes only | + **user threads + futex synchronization** |
| AI | None | **Our own fine-tuned 3B model** + chat |

> The starting point was a "textbook mini-OS." We **implemented real OS features
> (scheduler, syscalls, synchronization) directly**, then layered a **natural-
> language interface** on top.

---

## 3. Overall architecture (at a glance)

```
┌──────────────────────── My computer (MacBook) ────────────────────────┐
│                                                                        │
│   User: "무거운 프로세스 정리해줘"  (clean up the heavy processes)        │
│        │                                                                │
│        ▼                                                                │
│   ┌──────────────┐   translate  ┌──────────────────┐                   │
│   │  AI model     │◀────────────│  Bridge           │                  │
│   │ (our 3B LoRA) │─────────────▶│ (nlbridge.py)     │                 │
│   └──────────────┘   intent JSON └──────────────────┘                  │
│   → "killheavy"            │  passes the safety guard                   │
│                            ▼  (protect pid 0/1, confirm destructive)    │
│                  ┌───────────────────────────────┐                     │
│                  │  QEMU (virtual machine)        │                     │
│                  │   ┌─────────────────────────┐  │                     │
│   ─ ─ ─ ─ ─ ─ ─ ─│─ ─│  xv6 kernel (real OS)    │  │ ← syscall boundary │
│   above = host    │   │  • 3-level MLFQ sched.   │  │                    │
│   below = kernel   │   │  • ps/killheavy/reap... │  │                    │
│                  │   │  • threads + futex        │  │                    │
│                  │   └─────────────────────────┘  │                     │
│                  └───────────────────────────────┘                     │
└────────────────────────────────────────────────────────────────────────┘

★ The AI lives ONLY above the dotted line (host); never inside the kernel.
  The kernel only ever receives a tiny command like "killheavy" — it has no
  idea an AI was involved.
```

---

## 4. Components, one by one

### A. Kernel (the real OS part — `os/kernel/`)
The OS core we **wrote directly in C**. A complete operating system even without the AI.
- **3-level MLFQ scheduler** (`proc.c`): manages processes in HIGH/MID/LOW queues.
  Heavy CPU users auto-demote downward; a periodic boost lifts them back up.
- **15 new syscalls**: `ps` (process list), `setpri` (change priority), `reap`
  (clean zombies), `forkpri` (spawn into a specific queue), `thread_create/join`
  (threads), `futex_wait/wake` (synchronization), and more.
- **Multicore-safe**: spinlocks (`wait_lock`, `boost_lock`) protect concurrent access from multiple CPUs.

### B. User programs (`os/user/`)
Commands that use the kernel features: `ps`, `killall`, `killheavy`, `reap`,
`uptime`, `sysinfo`, plus **`ask`** (the bridge to natural language), `nlrun`
(run a workload in a chosen queue), and `cpu_burner`/`io_burner` (load generators).

### C. AI model (`ml/`)
- Base: **Qwen2.5-3B** (a small general-purpose language model)
- We **fine-tuned it ourselves** (LoRA) into a "natural language → OS intent" specialist.
- Accuracy: command classification **98.2%**, queue hint **99.4%** (measured on 1,854 test items).
- 20 intents: `cpu_burner` / `ps` / `kill` / `killheavy` / `reap` / `setpri` / `explain` / `reject` ...

### D. Host bridge (`host/`)
The Python glue connecting AI and kernel (outside the kernel).
- `nlos.py` — one-command launcher (backend select + model + bridge)
- `nlbridge.py` — wraps the xv6 console, intercepts natural language, auto-types the translated command
- `model_server.py` — serves the fine-tuned model via an OpenAI-compatible API (Mac MPS / Windows GPU both)
- `executor.py` — converts intent → xv6 command + **safety guard** + conversational reply (`chat`)

---

## 4-A. AI model fine-tuning details (presentation core)

### Why fine-tune
- The base **Qwen2.5-3B-Instruct** is a general chat model — used as-is for a
  **narrow, fixed task** like "natural language → OS intent JSON," its format drifts.
- So we **fine-tuned it (LoRA) on task-specific data** to reliably emit the 20
  intents in a **fixed JSON format**.

### Method — LoRA (Low-Rank Adaptation)
- Instead of training all 3 billion parameters, we **train only a small adapter
  (low-rank matrices)** → saves memory and time; the base weights stay **frozen**.
- Artifact: a LoRA adapter of **~477 MB** (stored separately from the 6 GB base).

### Training configuration (actual values used)

| Item | Value |
|---|---|
| Base model | `Qwen/Qwen2.5-3B-Instruct` (3B) |
| Method | LoRA (PEFT) + SFT, **base frozen** |
| LoRA rank (r) | **64** |
| LoRA alpha (α) | **64** |
| LoRA dropout | 0.05 |
| Target modules | q/k/v/o_proj + gate/up/down_proj (**all 7**: attention + MLP) |
| Epochs | **10** |
| Per-device batch | 4 |
| Gradient accumulation | 8 → **effective batch 32** |
| Learning rate | **1e-4** (cosine schedule, warmup 0.05) |
| Max sequence length | 2048 |
| Precision (dtype) | **bf16** |
| Optimizer | AdamW |
| Framework | HuggingFace Transformers + PEFT + TRL (SFTTrainer) |
| Training hardware | **RTX 5070 Ti (Blackwell)**, CUDA 12.8 (cu128) |

### Data pipeline (all built by us)
```
gen_intent_seeds.py   →   augment_offline.py   →   build_train.py   →   train.py
(write per-intent seeds)  (deterministic x8 aug, free)  (group split, 0 leak)  (LoRA train)
```
- **Dataset size**: **7,049 train** / **1,854 test**
- **20 intents**, roughly half Korean / half English
- **Group split**: paraphrases of the same meaning never appear in both train and
  test → **zero data leakage** (so the accuracy numbers are trustworthy)

### Results (measured on 1,854 test items)

| Metric | Accuracy |
|---|---|
| Command (cmd) classification | **98.2%** (1820/1854) |
| Queue hint (queue_hint) | **99.4%** (1842/1854) |
| Both correct | 97.7% |
| 18 of 20 intents | ~100% |

> A small 3B model fine-tuned on a narrow task reached **near-perfect accuracy** —
> evidence that "you don't need a huge model; a small specialist is enough for a
> well-defined task."

---

## 4-B. Why our model instead of Solar Pro 3 (presentation core)

> The assignment recommends Solar Pro 3 (a large cloud model) as the backend, but
> we used our **own fine-tuned on-device model** as the primary. The advantages:

| Comparison | **Our model (on-device 3B)** | **Solar Pro 3 (cloud)** |
|---|---|---|
| Accuracy on this task | **98.2% / 99.4%** (task-trained) | High (general, not fine-tuned) |
| Cost | **Free** | Per-call API cost |
| Internet | **Not needed (offline)** | Required (dies if it drops) |
| Privacy | **Local — zero data sent out** | Sent to the cloud |
| Latency | No network round-trip | Network on every call |
| API key / rate limits | **None** | Key management, call limits |
| Control / reproducibility | **Full control** (we trained & froze it) | Provider can change the model |
| "Built it ourselves" | **Our own trained model** | Using an external model |

### Three-line takeaway
1. **Specialization = accuracy**: on the narrow OS-intent task, a fine-tuned small
   model is **on par with or more accurate than** a large general model.
2. **On-device = free, offline, private**: no key, no internet, no data leakage —
   the demo doesn't break on a network failure.
3. **Hybrid supports both**: one env var switches Solar ↔ on-device, so Solar is
   kept as a *safety net / comparison baseline*.

> **Presentation one-liner**: "Instead of relying on a large cloud model (Solar),
> we **embedded our own small, self-trained model inside the OS** — running it for
> free, offline, and more accurately."

---

## 5. Example flow — following "clean up the heavy processes"

```
1. User types in the xv6 console:  무거운 프로세스 정리해줘
2. xv6 shell: there's no program named "무거운" → prints "exec 무거운 failed"
3. Bridge: intercepts that line, decides "this is natural language" → asks the AI
4. AI model:  {"cmd":"killheavy", ...}  (returns an intent JSON)
5. Safety guard: killheavy is destructive → warn, then allow (never kills init)
6. Bridge: auto-types "killheavy" into the xv6 console
7. Kernel: actually finds and terminates the heaviest process
   → "killed heaviest pid 8 (cpu_burner, run_ticks=405)"
```

**General conversation** works too (if it's not a command, the model answers):
```
타임 슬라이스 설명해줘  → [assistant] A time slice is the unit of time a process runs at once.
```

---

## 6. Where the OS concepts live (assignment core)

The assignment requires you to design/implement OS concepts directly. We
implemented **5+** of them in the kernel.

| OS concept | Where |
|---|---|
| **Scheduling** | 3-level MLFQ (`scheduler()` in `proc.c`) — demotion / promotion / round-robin |
| **Processes** | fork/exec/exit + ps/kill/killall/killheavy/reap |
| **System calls** | 15 added on top of the base 21 (registered in 6 places) |
| **Synchronization** | spinlocks (`wait_lock`/`boost_lock`) + **futex** (user-level sync) |
| **Threads** | `thread_create/join/exit` (shared address space) |
| **IPC** | host↔guest console round-trip channel |

> The AI is not a "runs on Linux" thin wrapper — **the OS core is something we
> wrote ourselves**. Remove the AI and the xv6 kernel + commands are a complete OS.

---

## 7. Key design principles

1. **The AI never enters the kernel.** Translation happens on the host; only a
   tiny command (intent JSON) crosses into the kernel → the kernel stays small
   and fast. (xv6 runs in 128 MB while the AI needs 6 GB — it can't and shouldn't go in.)
2. **The OS self-corrects the AI's mistakes.** If the AI puts a job in the wrong
   queue, MLFQ demotes it to the correct queue within a few ticks.
3. **The guard blocks danger.** Refuses to kill init (pid 0/1); gates destructive
   ops (kill/rm/killall) behind confirmation.
4. **Works without the AI.** If the model/network dies, a rule-based fallback keeps the demo alive.

---

## 8. What we verified

- **Kernel correctness**: xv6's official `usertests` — **all 61 passed** (stable across 3 clean builds).
- **A real bug we found & fixed**: an MLFQ scheduler **starvation** bug — higher-
  index runnable processes at a level were never scheduled; we fixed it with
  **round-robin within each level** (fork-storm test now passes).
- **AI accuracy**: 98.2% command / 99.4% queue over 1,854 test items.
- **Host logic**: 55 offline automated checks pass (guard / mapping / chat / etc.).
- Full verification log: [`verification-report.md`](verification-report.md)

---

## 9. Honest limitations

- **If the natural language starts with a real command word** (e.g. `ls 정리해줘`),
  the shell runs that command → use the `ask` prefix in that case.
- **General chat is only "partial"** — "explain X" and small talk get model
  answers, but "what is a process?" may be classified as `ps` and run (a property
  of the intent-classification model).
- **The xv6 console occasionally drops an input byte under heavy load** — the
  model recovers so the result is correct, but the display can flicker (an
  inherent xv6 console limit).
- **Out of scope** (xv6 simply lacks the feature): networking, users/permissions,
  package managers, process suspension.

---

## Suggested slide outline (when turning this into a deck)

1. **Title** — "An xv6 OS you talk to: a hand-built kernel + a self-trained AI"
2. **Problem / motivation** — memorizing exact commands → natural language (analogy, §1)
3. **Vanilla xv6 vs our result** — before/after table (§2)
4. **Overall architecture** — diagram, emphasize "AI outside the kernel" (§3)
5. **OS concepts implemented in the kernel** — MLFQ / syscalls / sync / threads / IPC (§6)
6. **MLFQ scheduler deep-dive** — 3 queues, demotion/promotion, the starvation bug we fixed (§4, §8)
7. **AI model fine-tuning** — Qwen2.5-3B + LoRA, hyperparameter table, data pipeline (§4-A)
8. **Accuracy results** — 98.2% / 99.4% bar chart (§4-A)
9. **Why our model vs Solar Pro 3** — comparison table + 3-line takeaway (§4-B)
10. **Live demo flow** — walking through "clean up the heavy processes" (§5)
11. **Verification** — 61 usertests passed + bug fix (§8)
12. **Limitations & future work** — partial chat, retraining option (§9)
13. **Closing one-liner** (below)

> Charts to make: ① accuracy (98.2/99.4) bar, ② per-intent accuracy, ③ our-model
> vs Solar table, ④ architecture diagram, ⑤ MLFQ demotion flow.

---

## Closing one-liner

> **"A small teaching OS (xv6) with real OS features implemented by hand, topped
> with a natural-language interface powered by an AI we trained ourselves."**
> The OS is the core we built; the AI is the translator in front of it — and the
> clean separation of those two roles is the heart of this project.
