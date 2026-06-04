"""M3 — Intent executor + safety guard for the xv6 NL shell.

Pipeline:  natural language --(model)--> spec --(guard)--> xv6 command --(run)

- classify():     NL -> spec {cmd,args,queue_hint,reason} via the OpenAI-compatible
                  endpoint (Solar Pro 3, or a local fine-tuned model via Ollama —
                  swap with UPSTAGE_BASE_URL / UPSTAGE_MODEL, same as the rest of
                  the project).
- build_command():spec -> the exact xv6 console command string (all 20 intents).
- SafetyGuard:    blocks killing pid 0/1, requires confirmation for destructive
                  ops (kill/rm/killall/killheavy), validates args.

The pure logic (build_command + SafetyGuard) needs no model and no QEMU, so it
is unit-tested offline via:  python executor.py --selftest

Live REPL (needs a running QEMU + a reachable model):  python executor.py
"""
from __future__ import annotations
import os
import re
import json
from dataclasses import dataclass, field
from typing import Optional

# Must match the training-time system prompt (scripts/train.py SYSTEM_PROMPT).
SYSTEM_PROMPT = (
    "You are an xv6 natural-language OS shell. Convert the user request into a "
    "single JSON object with keys cmd, args, queue_hint, reason.\n"
    "cmd is one of:\n"
    "  workloads: cpu_burner | io_burner | mixed_burner (args=[iterations]); "
    "set queue_hint 0=HIGH (interactive/IO), 1=MID, 2=LOW (heavy/background).\n"
    "  files: ls (args=[path?]) | cat (args=[path]) | rm (args=[path]) | "
    "mkdir (args=[path]) | ln (args=[target,linkname]) | echo (args=[text]).\n"
    "  process: ps (args=[]) | kill (args=[pid]) | setpri (args=[pid,prio 0..2]) | "
    "trace (args=[pid,\"on\"|\"off\"]).\n"
    "  cleanup: killall (args=[name]) | killheavy (args=[count?]) | reap (args=[]).\n"
    "  system: uptime (args=[]) | sysinfo (args=[]).\n"
    "  info: explain (args=[topic]) — answer only, no OS action.\n"
    "  reject — unsafe/unsupported/out-of-scope (killing pid 0 or 1, networking, "
    "sudo, gibberish).\n"
    "For every non-workload command set queue_hint to 0. args is a list of "
    "strings. reason is a short English sentence. Output JSON only, no markdown."
)

# Programs that only run a workload (queue-aware via nlrun).
_WORKLOADS = {"cpu_burner", "io_burner", "mixed_burner"}
# Destructive ops — require explicit confirmation before executing.
_DESTRUCTIVE = {"kill", "rm", "killall", "killheavy"}
# Intents that produce no xv6 command (answered/handled on the host).
_HOST_ONLY = {"explain", "reject"}
PROTECTED_PIDS = {0, 1}


@dataclass
class Decision:
    verdict: str                 # "allow" | "confirm" | "reject"
    command: Optional[str]       # xv6 command to run (None for host-only/reject)
    reason: str = ""


def _arg(args, i, default=None):
    return args[i] if i is not None and 0 <= i < len(args) else default


def build_command(spec: dict):
    """Map a validated spec to an xv6 console command string.
    Returns (command|None, error|None). command is None for host-only intents.
    """
    cmd = spec.get("cmd")
    args = spec.get("args") or []
    q = spec.get("queue_hint", 0)

    if cmd in _HOST_ONLY:
        return None, None
    if cmd in _WORKLOADS:
        if q not in (0, 1, 2):
            q = 1
        tail = (" " + " ".join(map(str, args))) if args else ""
        return f"nlrun {q} {cmd}{tail}", None
    if cmd == "echo":
        return "echo " + " ".join(map(str, args)), None
    if cmd == "ls":
        return ("ls " + str(args[0])).strip() if args else "ls", None
    if cmd == "cat":
        if not args:
            return None, "cat needs a file path"
        return f"cat {args[0]}", None
    if cmd == "rm":
        if not args:
            return None, "rm needs a file path"
        return f"rm {args[0]}", None
    if cmd == "mkdir":
        if not args:
            return None, "mkdir needs a path"
        return f"mkdir {args[0]}", None
    if cmd == "ln":
        if len(args) < 2:
            return None, "ln needs <target> <linkname>"
        return f"ln {args[0]} {args[1]}", None
    if cmd == "ps":
        return "ps", None
    if cmd == "kill":
        if not args:
            return None, "kill needs a pid"
        return f"kill {args[0]}", None
    if cmd == "setpri":
        if len(args) < 2:
            return None, "setpri needs <pid> <prio>"
        return f"setprio {args[0]} {args[1]}", None       # user program is `setprio`
    if cmd == "trace":
        if not args:
            return None, "trace needs a pid"
        on = 1 if str(_arg(args, 1, "on")).lower() in ("on", "1", "true") else 0
        return f"tracepid {args[0]} {on}", None
    if cmd == "killall":
        if not args:
            return None, "killall needs a name"
        return f"killall {args[0]}", None
    if cmd == "killheavy":
        return (f"killheavy {args[0]}" if args else "killheavy"), None
    if cmd == "reap":
        return "reap", None
    if cmd == "uptime":
        return "uptime", None
    if cmd == "sysinfo":
        return "sysinfo", None
    return None, f"unknown cmd: {cmd!r}"


class SafetyGuard:
    """Last line of defense before a command reaches xv6."""

    def evaluate(self, spec: dict) -> Decision:
        cmd = spec.get("cmd")
        args = spec.get("args") or []

        if cmd == "reject":
            return Decision("reject", None, spec.get("reason", "rejected by model"))
        if cmd == "explain":
            return Decision("allow", None, "informational; no OS action")

        # kill: protect init/scheduler pids
        if cmd == "kill" and args:
            try:
                pid = int(args[0])
            except (ValueError, TypeError):
                return Decision("reject", None, f"invalid pid {args[0]!r}")
            if pid in PROTECTED_PIDS:
                return Decision("reject", None, f"pid {pid} is protected (init)")

        # setpri: validate priority range
        if cmd == "setpri" and len(args) >= 2:
            try:
                prio = int(args[1])
            except (ValueError, TypeError):
                return Decision("reject", None, f"invalid prio {args[1]!r}")
            if prio not in (0, 1, 2):
                return Decision("reject", None, f"prio {prio} out of range 0..2")

        command, err = build_command(spec)
        if err:
            return Decision("reject", None, err)
        if command is None:                       # host-only (explain handled above)
            return Decision("allow", None, "no command")

        if cmd in _DESTRUCTIVE:
            return Decision("confirm", command,
                            f"destructive ({cmd}) — confirm before running")
        return Decision("allow", command, "ok")


# ---------------------------------------------------------------------------
# Model call (NL -> spec)
# ---------------------------------------------------------------------------
def classify(text: str, timeout: float = 30.0) -> dict:
    import requests
    base = os.environ.get("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
    model = os.environ.get("UPSTAGE_MODEL", "solar-pro3")
    key = (os.environ.get("UPSTAGE_API_KEY")
           or os.environ.get("SOLAR_API_KEY") or "not-needed")
    r = requests.post(
        f"{base}/chat/completions",
        headers={"Authorization": f"Bearer {key}"},
        json={
            "model": model,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": text},
            ],
            "temperature": 0,
            "response_format": {"type": "json_object"},
        },
        timeout=timeout,
    )
    r.raise_for_status()
    raw = r.json()["choices"][0]["message"]["content"].strip()
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        m = re.search(r"\{.*\}", raw, re.DOTALL)
        return json.loads(m.group(0)) if m else {"cmd": "reject", "args": [],
                                                  "queue_hint": 0, "reason": "unparseable"}


# ---------------------------------------------------------------------------
# Offline self-test (no model, no QEMU)
# ---------------------------------------------------------------------------
def _selftest():
    guard = SafetyGuard()
    cases = [
        {"cmd": "cpu_burner", "args": ["2000000"], "queue_hint": 2, "reason": ""},
        {"cmd": "ps", "args": [], "queue_hint": 0, "reason": ""},
        {"cmd": "kill", "args": ["7"], "queue_hint": 0, "reason": ""},
        {"cmd": "kill", "args": ["1"], "queue_hint": 0, "reason": ""},     # must reject
        {"cmd": "setpri", "args": ["5", "2"], "queue_hint": 0, "reason": ""},
        {"cmd": "setpri", "args": ["5", "9"], "queue_hint": 0, "reason": ""},  # bad prio
        {"cmd": "rm", "args": ["foo.txt"], "queue_hint": 0, "reason": ""},
        {"cmd": "killall", "args": ["cpu_burner"], "queue_hint": 0, "reason": ""},
        {"cmd": "killheavy", "args": [], "queue_hint": 0, "reason": ""},
        {"cmd": "reap", "args": [], "queue_hint": 0, "reason": ""},
        {"cmd": "trace", "args": ["6", "on"], "queue_hint": 0, "reason": ""},
        {"cmd": "uptime", "args": [], "queue_hint": 0, "reason": ""},
        {"cmd": "explain", "args": ["MLFQ"], "queue_hint": 0, "reason": "info"},
        {"cmd": "reject", "args": [], "queue_hint": 0, "reason": "no-op"},
    ]
    print(f"{'cmd':10} {'verdict':8} command")
    print("-" * 50)
    for s in cases:
        d = guard.evaluate(s)
        print(f"{s['cmd']:10} {d.verdict:8} {d.command if d.command else '('+d.reason+')'}")


# ---------------------------------------------------------------------------
# Live REPL (model + QEMU). Kept minimal; prints the command and asks for
# confirmation on destructive ops. Wire a QEMU driver here when running live.
# ---------------------------------------------------------------------------
def repl(execute_fn=None):
    """execute_fn(command:str) actually runs it in xv6; if None, just prints."""
    guard = SafetyGuard()
    print("xv6 NL shell (M3). Type a request, or 'quit'.")
    while True:
        try:
            text = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not text or text.lower() in ("quit", "exit"):
            break
        try:
            spec = classify(text)
        except Exception as e:
            print(f"  [classify failed] {e}")
            continue
        print(f"  spec: {json.dumps(spec, ensure_ascii=False)}")
        d = guard.evaluate(spec)
        if d.verdict == "reject":
            print(f"  REJECTED: {d.reason}")
            continue
        if d.command is None:
            print(f"  (no OS action) {d.reason}")
            continue
        if d.verdict == "confirm":
            ans = input(f"  run destructive `{d.command}`? [y/N] ").strip().lower()
            if ans != "y":
                print("  cancelled")
                continue
        print(f"  -> {d.command}")
        if execute_fn:
            execute_fn(d.command)


if __name__ == "__main__":
    import sys
    if "--selftest" in sys.argv:
        _selftest()
    else:
        repl()
