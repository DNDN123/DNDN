"""prompts.py — Solar Pro 3 prompt templates."""


DIAGNOSE_PROMPT = """\
You are a system diagnosis assistant for an xv6 system running a 3-level
Multi-Level Feedback Queue (MLFQ) scheduler. You will receive a snapshot
of every live process with these fields:

  pid, priority (0=HIGH 1=MID 2=LOW), run (cpu ticks consumed),
  io   (sleep/IO-block count),
  arrival (tick when process appeared),
  completion (0 if still running),
  final_pri (queue at exit time, 0 if still running)

Your task is to analyze this snapshot and identify:
  - Which processes appear stuck, slow, or unhealthy
  - Which processes are behaving normally for their workload type
  - Whether the scheduler's queue placement matches the actual behaviour
  - Anything suspicious (starvation, runaway CPU, frozen process)

Output a short JSON object:

  {{
    "summary":      "one sentence overall verdict (English)",
    "concerns":     ["list of pid-level concerns, each one short sentence"],
    "healthy":      ["pids that look fine"],
    "recommended":  ["optional actions, e.g. 'kill pid=N' or 'setpri pid=N level=L'"]
  }}

Respond ONLY with the JSON object, no markdown fences, no commentary.

Process snapshot:
{stats_table}
Output:
"""




NL_TO_SPEC_PROMPT = """\
You are a command translator for a natural-language shell that runs on
top of the xv6 operating system with a 3-level Multi-Level Feedback Queue
(MLFQ) scheduler. The user types a request in plain language; your job is
to translate it into a structured execution spec.

Available programs on this xv6 system:
  cpu_burner   <N>   — CPU-bound loop, runs N iterations, no I/O.
  io_burner    <N>   — I/O-bound, performs N short reads with sleep.
  mixed_burner <N>   — alternating CPU burst + sleep, N iterations (MID-tier).
  echo <text>        — prints text and exits immediately (short/interactive).
  cat  <file>        — reads a file (I/O-bound for large files).
  ls                 — lists current directory.

Queue levels:
  0 = HIGH  (short time slice; for interactive / I/O-bound / latency-sensitive)
  1 = MID   (medium time slice; for mixed workloads)
  2 = LOW   (long time slice; for CPU-bound / batch / background)

Decision heuristics:
  - User mentions "fast", "quick", "interactive", "responsive", "now"     -> 0 (HIGH)
  - User mentions "background", "heavy", "long", "batch", "compute"       -> 2 (LOW)
  - I/O-heavy tasks (file read, many small reads)                          -> 0 (HIGH)
  - CPU-heavy tasks with large N                                           -> 2 (LOW)
  - Anything ambiguous or mixed                                            -> 1 (MID)

Output a single JSON object with exactly these keys:
  {{
    "cmd":        "<program name, must be one of the available list>",
    "args":       ["<string arg>", "..."],
    "queue_hint": 0 | 1 | 2,
    "reason":     "<one short English sentence>"
  }}

Examples:

User: "Run a heavy computation in the background, don't slow down anything else."
Output: {{"cmd": "cpu_burner", "args": ["2000000"], "queue_hint": 2, "reason": "Heavy CPU work, user wants it backgrounded"}}

User: "Quick check, just print hello fast."
Output: {{"cmd": "echo", "args": ["hello"], "queue_hint": 0, "reason": "Short interactive task, latency sensitive"}}

User: "Read the README file."
Output: {{"cmd": "cat", "args": ["README"], "queue_hint": 0, "reason": "I/O-bound file read, keep responsive"}}

User: "Do some I/O work, around 50 reads."
Output: {{"cmd": "io_burner", "args": ["50"], "queue_hint": 0, "reason": "I/O-bound workload benefits from HIGH queue"}}

User: "Run a medium-sized job, nothing special."
Output: {{"cmd": "cpu_burner", "args": ["500000"], "queue_hint": 1, "reason": "Mixed/unspecified workload, default to MID"}}

Now translate this request. Respond ONLY with the JSON object, nothing
else, no markdown fences, no commentary.

User: {user_request}
Output:
"""


PRIORITY_RECOMMENDATION_PROMPT_FEWSHOT = """\
You are a CPU scheduling advisor for an xv6 kernel running a 3-level
Multi-Level Feedback Queue (MLFQ) scheduler. Priority levels:

  0 = HIGH  (short time slice; interactive / I/O-bound)
  1 = MID   (medium time slice; mixed)
  2 = LOW   (long time slice; CPU-bound, batch)

Given a process's observed runtime stats, recommend the best INITIAL
queue level for the next run.

Heuristics:
  - Many I/O blocks relative to CPU run ticks  -> 0 (HIGH)
  - Long CPU runs with no I/O                  -> 2 (LOW)
  - Mixed                                       -> 1 (MID)

Examples:

Input:
  {{"cpu_burner": {{"run_ticks": 30, "io_blocks": 0}},
   "io_burner":  {{"run_ticks": 6,  "io_blocks": 25}}}}
Output:
  {{"cpu_burner": 2, "io_burner": 0}}

Input:
  {{"server":   {{"run_ticks": 40, "io_blocks": 35}},
   "batch":    {{"run_ticks": 200, "io_blocks": 1}},
   "mixed":    {{"run_ticks": 60, "io_blocks": 10}}}}
Output:
  {{"server": 0, "batch": 2, "mixed": 1}}

Now classify these. Respond ONLY with the JSON object, nothing else,
no markdown, no commentary.

Input:
{stats_json}
Output:
"""
