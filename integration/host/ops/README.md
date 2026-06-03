# Supervisor LLM — OS-grounded Audit / Diagnosis Layer

`host/ops/supervisor.py` is the **meta layer** over the integration tree.
It is **NOT** part of the hot path (`nl_shell.py`, `nlrun`, `tracetool`)
and never runs during the live demo or quantitative evaluation. It exists
to make the project's *artifacts* — eval JSONs, MERGE_NOTES, syscall table,
K1~K4 fixes — queryable in natural language without becoming a "thin LLM
wrapper" (which the syllabus forbids).

## Why this is allowed under "LLM for OS"

| Principle (from `_sources/hyunsung/README.md`) | How supervisor honors it |
|---|---|
| LLM never enters the kernel | Pure host-side, READ-only on artifacts |
| LLM output is reduced to 2 bits at the syscall boundary | N/A — supervisor is cold path |
| OS layer auto-corrects bad hints | Supervisor only recommends, never executes |
| Demos work without the LLM | Supervisor is optional; main demo path unchanged |
| **No thin LLM wrappers** | **Free chat is REJECTED**. Every answer must call ≥1 tool grounded in xv6 data |

The supervisor refuses any input that isn't grounded in this tree's data.
If you ask "what's the weather", you get `REJECT`.

## Files

```
host/ops/
├── __init__.py
├── tools.py        ← 7 read-only data-access functions
├── supervisor.py   ← LLM tool-calling loop + CLI
└── README.md       ← this file
```

## Tool catalog

| Tool | What it returns |
|---|---|
| `list_workloads()` | spec files in `fs.img` and which have eval data |
| `read_eval_report(workload)` | metrics.json for one workload, or combined report |
| `read_trace_dump(json_text)` | parse `tracetool dump` output + classify pid |
| `read_merge_notes(section)` | full MERGE_NOTES.md or one numbered section |
| `grep_integration(pattern)` | recursive grep across `integration/` |
| `verify_k_fix(label)` | confirm K1~K4 fix labels live in the code |
| `read_syscall_table()` | parse syscall.h + usys.pl + user.h, detect drift |

To extend: add a function to `tools.py` and register it in `TOOLS`.
**Do not** give the supervisor shell access or write capability.

## Usage

```bash
cd integration/host
python ops/supervisor.py --once "이번 평가에서 io_heavy 가 왜 회귀됐어?"
python ops/supervisor.py --once "K1~K4 fix 가 코드에 다 적용됐어?"
python ops/supervisor.py --once "syscall.h vs usys.pl 일관성 점검"

# Interactive REPL
python ops/supervisor.py
supv> ...
```

`--verbose` shows the tool-call trace on stderr.

## How rejection works (two layers)

1. **Local guard** in `is_obviously_offtopic()` — pattern match on greetings,
   weather, jokes, etc. Rejects before the API call (saves quota).
2. **Server-side guard** in the system prompt — Solar is instructed to emit
   `{"action": "reject", ...}` for anything off-topic.
3. **No-evidence guard** — even if Solar attempts to answer without calling
   any tool, the loop rejects with reason
   "supervisor refused to answer without tool evidence".

Bypass: extending the tool list. If you add a `read_kernel_log()` tool, the
supervisor can now answer kernel-log questions. The supervisor's scope is
exactly the union of its tools' data.

## Architecture diagram

```
                 ┌──────────────────────────────────────┐
   user q ─────► │  is_obviously_offtopic? → REJECT     │
                 └──────────────────────────────────────┘
                                │
                                ▼
                 ┌──────────────────────────────────────┐
                 │  Solar Pro 3 (loop, ≤6 turns)        │
                 │   1) emit {action: call_tool}        │ ◄─┐
                 │   2) wait for result                 │   │ tool result
                 │   3) call again or answer/reject     │   │ fed back
                 └──────────────────────────────────────┘   │
                                │                            │
                                ▼                            │
                 ┌──────────────────────────────────────┐    │
                 │  execute_tool(name, args)            ├────┘
                 │   reads MERGE_NOTES / metrics.json / │
                 │   syscall.h / source files (RO)      │
                 └──────────────────────────────────────┘
                                │
                                ▼
                 ┌──────────────────────────────────────┐
                 │  final {action: answer, evidence:[…]}│
                 └──────────────────────────────────────┘
```

`MAX_TURNS = 6` caps the recursion. If the model can't reach a grounded
answer in 6 tool calls, the request is rejected.

## What it is NOT for

- Running xv6 commands (use `nl_shell.py --mode process/thread`)
- Generating LLM hints for the scheduler (use `llm_hint.py`)
- Translating NL to xv6 commands (use `nl_shell.py --once`)
- Anything that touches the live QEMU / kernel

If you find yourself wanting to add execution capability here, that's a sign
the question belongs in a different host module. Keep `ops/` read-only.
