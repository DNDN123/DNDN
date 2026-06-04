"""M4 — abstract-request agent. Decomposes a vague, multi-step request like
"정리해줘 / clean up / 최적화" into a sequence of CONCRETE intents, then runs
each through M3's SafetyGuard (executor.py).

Flow:  abstract NL --(observe ps)--> snapshot --(plan)--> [intent, intent, ...]
       --> M3 SafetyGuard --> xv6 commands

The planner is DETERMINISTIC (rule-based), so it needs no model and no QEMU and
is fully unit-tested offline:   python agent.py --selftest

Live use (needs QEMU + ps output):  python agent.py     (routes abstract
requests here; concrete one-shot requests fall back to executor.classify).
"""
from __future__ import annotations
import re
import json
from dataclasses import dataclass
from typing import List, Callable, Optional

from executor import SafetyGuard, build_command, classify

# ---------------------------------------------------------------------------
# Observe — parse `ps` output (user/ps.c format)
#   PID  PPID  STATE  PRIO  SZ  NAME
# ---------------------------------------------------------------------------
_PS_RE = re.compile(r"^\s*(\d+)\s+(\d+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\S+)\s*$")


@dataclass
class ProcRow:
    pid: int
    ppid: int
    state: str
    prio: int
    sz: int
    name: str


def parse_ps(text: str) -> List[ProcRow]:
    rows: List[ProcRow] = []
    for line in text.splitlines():
        if line.strip().startswith("PID") or line.strip().startswith("TOTAL"):
            continue
        m = _PS_RE.match(line)
        if m:
            rows.append(ProcRow(int(m.group(1)), int(m.group(2)), m.group(3),
                                int(m.group(4)), int(m.group(5)), m.group(6)))
    return rows


# ---------------------------------------------------------------------------
# Recognize abstract / multi-step requests
# ---------------------------------------------------------------------------
_GENERIC = ["정리", "청소", "치워", "최적화", "clean", "tidy", "optimize", "cleanup"]
_ZOMBIE = ["좀비", "zombie"]
_HEAVY = ["무거운", "무겁", "느린", "느려", "heavy", "slow", "laggy", "cpu 많이", "cpu많이"]


def looks_abstract(text: str) -> bool:
    t = text.lower()
    return any(k in text or k in t for k in _GENERIC)


def _extract_count(text: str) -> Optional[int]:
    m = re.search(r"(\d+)\s*(개|ea|processes|procs)?", text)
    if m:
        try:
            n = int(m.group(1))
            return n if n > 0 else None
        except ValueError:
            return None
    return None


# ---------------------------------------------------------------------------
# Plan — abstract request + snapshot -> list of concrete intents
# ---------------------------------------------------------------------------
def plan(text: str, procs: List[ProcRow]) -> List[dict]:
    t = text.lower()
    want_zombie = any(k in text or k in t for k in _ZOMBIE)
    want_heavy = any(k in text or k in t for k in _HEAVY)
    generic = any(k in text or k in t for k in _GENERIC)

    # non-protected user processes currently alive (for deciding load relief)
    live = [p for p in procs
            if p.pid > 1 and p.name not in ("init", "sh") and p.state != "zombie"]

    specs: List[dict] = []

    def add(cmd, args, reason):
        specs.append({"cmd": cmd, "args": [str(a) for a in args],
                      "queue_hint": 0, "reason": reason})

    # 1. zombies: always safe to reap when asked, or as part of generic cleanup
    if want_zombie or generic:
        add("reap", [], "clear abandoned zombies")

    # 2. heavy load: kill the heaviest process(es) only when explicitly asked
    #    ("무거운/느린 거 정리"), or generic cleanup with a crowded table.
    if want_heavy:
        n = _extract_count(text)
        add("killheavy", [n] if n else [], "relieve CPU load (heaviest)")
    elif generic and not want_zombie and len(live) >= 3:
        # bare "정리/clean up" on a crowded table: offer load relief (gated by
        # confirm). Skip this for zombie-specific requests ("좀비 정리").
        add("killheavy", [], "system busy — relieve heaviest process")

    return specs


# ---------------------------------------------------------------------------
# Run the plan through M3's guard
# ---------------------------------------------------------------------------
def run_plan(text: str, ps_text: str,
             execute_fn: Optional[Callable[[str], None]] = None,
             confirm_fn: Optional[Callable[[str], bool]] = None) -> List[str]:
    """Returns the list of commands that were approved (and executed if
    execute_fn given). confirm_fn(command)->bool gates destructive steps."""
    procs = parse_ps(ps_text)
    specs = plan(text, procs)
    guard = SafetyGuard()
    approved: List[str] = []

    print(f"[agent] observed {len(procs)} procs; plan = "
          f"{[s['cmd'] for s in specs] or 'nothing to do'}")

    for spec in specs:
        d = guard.evaluate(spec)
        if d.verdict == "reject" or d.command is None:
            print(f"  skip {spec['cmd']}: {d.reason}")
            continue
        if d.verdict == "confirm":
            ok = confirm_fn(d.command) if confirm_fn else False
            if not ok:
                print(f"  cancelled: {d.command}")
                continue
        print(f"  -> {d.command}")
        approved.append(d.command)
        if execute_fn:
            execute_fn(d.command)

    return approved


# ---------------------------------------------------------------------------
# Offline self-test (no model, no QEMU)
# ---------------------------------------------------------------------------
def _selftest():
    sample_ps = (
        "PID  PPID  STATE    PRIO  SZ        NAME\n"
        "1  0  sleep  0  16384  init\n"
        "2  1  run  0  20480  sh\n"
        "3  2  run  2  40960  cpu_burner\n"
        "4  2  run  2  40960  cpu_burner\n"
        "5  2  zombie  0  0  io_burner\n"
        "6  2  runble  1  30000  mixed_burner\n"
        "TOTAL 6\n"
    )
    procs = parse_ps(sample_ps)
    assert len(procs) == 6, procs
    cases = [
        "정리해줘",
        "좀비 정리해줘",
        "무거운 거 정리해줘",
        "무거운 거 2개 정리해줘",
        "시스템 좀비랑 무거운 거 다 청소해줘",
        "clean up the system",
        "프로세스 목록 보여줘",     # not abstract -> no plan
    ]
    for c in cases:
        abstract = looks_abstract(c)
        p = plan(c, procs) if abstract else []
        steps = [f"{s['cmd']}{s['args']}" for s in p]
        print(f"abstract={int(abstract)}  {c!r:42} -> {steps or '(single-shot via executor)'}")


# ---------------------------------------------------------------------------
# Live REPL: route abstract -> agent plan, concrete -> executor single-shot
# ---------------------------------------------------------------------------
def repl(run_ps_fn: Optional[Callable[[], str]] = None,
         execute_fn: Optional[Callable[[str], None]] = None):
    guard = SafetyGuard()

    def confirm(cmd: str) -> bool:
        return input(f"  run destructive `{cmd}`? [y/N] ").strip().lower() == "y"

    print("xv6 NL agent (M4). Abstract requests are decomposed; type 'quit'.")
    while True:
        try:
            text = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print(); break
        if not text or text.lower() in ("quit", "exit"):
            break

        if looks_abstract(text):
            ps_text = run_ps_fn() if run_ps_fn else ""
            run_plan(text, ps_text, execute_fn=execute_fn, confirm_fn=confirm)
        else:
            try:
                spec = classify(text)
            except Exception as e:
                print(f"  [classify failed] {e}"); continue
            d = guard.evaluate(spec)
            print(f"  spec: {json.dumps(spec, ensure_ascii=False)}")
            if d.verdict == "reject" or d.command is None:
                print(f"  {'REJECTED: ' if d.verdict=='reject' else ''}{d.reason}")
                continue
            if d.verdict == "confirm" and not confirm(d.command):
                print("  cancelled"); continue
            print(f"  -> {d.command}")
            if execute_fn:
                execute_fn(d.command)


if __name__ == "__main__":
    import sys
    if "--selftest" in sys.argv:
        _selftest()
    else:
        repl()
