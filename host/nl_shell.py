"""
nl_shell.py
===========
Natural-language shell front-end for the Smart-MLFQ xv6 system.

User types a request in plain English/Korean. Solar Pro (or a local
heuristic fallback) translates it into an execution spec:
    {cmd, args, queue_hint, reason}
which becomes an xv6 command line like:
    nlrun 2 cpu_burner 1000000

In --dry-run mode (default) the command is printed for the user to
paste into the xv6 shell themselves — robust for live demos.
In --exec mode (optional) the command is written to QEMU's stdin via
subprocess; useful for automated tests but more fragile.

Usage:
    python3 nl_shell.py                       # interactive REPL, dry-run
    python3 nl_shell.py --once "do X fast"    # one-shot
    python3 nl_shell.py --exec                # forward commands to QEMU

Env:
    UPSTAGE_API_KEY           (optional — falls back to heuristic if missing)
    UPSTAGE_MODEL             (default: solar-pro3)
    UPSTAGE_BASE_URL          (default: https://api.upstage.ai/v1)
    UPSTAGE_REASONING_EFFORT  (default: low — "low" | "medium" | "high")
"""

import argparse
import hashlib
import json
import os
import re
import sys
import time
from pathlib import Path

from prompts import NL_TO_SPEC_PROMPT, DIAGNOSE_PROMPT, TRACETOOL_ANALYZE_PROMPT


# Cache for natural-language → spec results so repeated requests skip
# the Solar round-trip (latency + cost). Cache key = sha1(normalized text).
CACHE_PATH = Path(os.getenv("SMART_MLFQ_CACHE",
                            str(Path.home() / ".smart-mlfq" / "hints_cache.json")))
CACHE_TTL_SEC = int(os.getenv("SMART_MLFQ_CACHE_TTL", "604800"))   # 7 days


def _cache_key(text: str) -> str:
    return hashlib.sha1(text.strip().lower().encode("utf-8")).hexdigest()


def _cache_load() -> dict:
    try:
        return json.loads(CACHE_PATH.read_text())
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def _cache_lookup(text: str) -> dict | None:
    cache = _cache_load()
    entry = cache.get(_cache_key(text))
    if not entry:
        return None
    if time.time() - entry.get("cached_at", 0) > CACHE_TTL_SEC:
        return None
    return entry.get("spec")


def _cache_store(text: str, spec: dict):
    cache = _cache_load()
    cache[_cache_key(text)] = {
        "spec": {k: v for k, v in spec.items() if not k.startswith("_")},
        "text_preview": text[:80],
        "cached_at": int(time.time()),
    }
    CACHE_PATH.parent.mkdir(parents=True, exist_ok=True)
    CACHE_PATH.write_text(json.dumps(cache, indent=2, ensure_ascii=False))

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass

try:
    from openai import OpenAI
    HAVE_OPENAI = True
except ImportError:
    HAVE_OPENAI = False


KNOWN_PROGRAMS = {"cpu_burner", "io_burner", "mixed_burner", "echo", "cat", "ls"}
VALID_QUEUES = (0, 1, 2)
QUEUE_NAME = {0: "HIGH", 1: "MID", 2: "LOW"}


def heuristic_spec(text: str) -> dict:
    t = text.lower()
    if any(k in t for k in ("background", "heavy", "long", "batch", "compute",
                             "백그라운드", "무거운", "오래")):
        cmd, args, q = "cpu_burner", ["1000000"], 2
        reason = "Heuristic: background/heavy keywords -> LOW"
    elif any(k in t for k in ("fast", "quick", "interactive", "now", "hello",
                               "빠르", "짧", "응답")):
        cmd, args, q = "echo", ["hello"], 0
        reason = "Heuristic: fast/short keywords -> HIGH"
    elif any(k in t for k in ("io", "read", "file", "읽")):
        cmd, args, q = "io_burner", ["50"], 0
        reason = "Heuristic: I/O keywords -> HIGH"
    else:
        cmd, args, q = "cpu_burner", ["500000"], 1
        reason = "Heuristic: ambiguous -> MID"
    return {"cmd": cmd, "args": args, "queue_hint": q, "reason": reason}


def call_solar(text: str) -> dict | None:
    if not HAVE_OPENAI:
        return None
    key = os.getenv("UPSTAGE_API_KEY")
    if not key or key == "your_api_key_here":
        return None
    base = os.getenv("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
    model = os.getenv("UPSTAGE_MODEL", "solar-pro3")
    effort = os.getenv("UPSTAGE_REASONING_EFFORT", "low")

    client = OpenAI(api_key=key, base_url=base)
    prompt = NL_TO_SPEC_PROMPT.format(user_request=text)
    try:
        resp = client.chat.completions.create(
            model=model,
            messages=[{"role": "user", "content": prompt}],
            temperature=0.0,
            max_tokens=1024,
            extra_body={"reasoning_effort": effort},
        )
        raw = resp.choices[0].message.content.strip()
        return _parse_spec(raw)
    except Exception as e:
        print(f"[nl_shell] solar call failed ({type(e).__name__}: {e})",
              file=sys.stderr)
        return None


def _parse_spec(raw: str) -> dict | None:
    s = raw.strip()
    if s.startswith("```"):
        s = re.sub(r"^```(?:json)?", "", s).strip()
        s = re.sub(r"```$", "", s).strip()
    try:
        return json.loads(s)
    except json.JSONDecodeError:
        m = re.search(r"\{.*\}", s, re.DOTALL)
        if m:
            try:
                return json.loads(m.group(0))
            except json.JSONDecodeError:
                return None
        return None


def validate_spec(spec: dict) -> tuple[bool, str]:
    if not isinstance(spec, dict):
        return False, "spec is not a dict"
    for key in ("cmd", "args", "queue_hint"):
        if key not in spec:
            return False, f"missing key: {key}"
    if spec["cmd"] not in KNOWN_PROGRAMS:
        return False, f"unknown program: {spec['cmd']!r}"
    if not isinstance(spec["args"], list):
        return False, "args must be a list"
    if spec["queue_hint"] not in VALID_QUEUES:
        return False, f"queue_hint must be 0/1/2, got {spec['queue_hint']!r}"
    return True, "ok"


def spec_to_xv6_cmd(spec: dict) -> str:
    parts = ["nlrun", str(spec["queue_hint"]), spec["cmd"]]
    parts.extend(str(a) for a in spec["args"])
    return " ".join(parts)


def translate(text: str, use_cache: bool = True) -> dict:
    # 0) Cache hit? Avoids Solar round-trip for repeated identical requests.
    if use_cache:
        cached = _cache_lookup(text)
        if cached is not None:
            ok, _ = validate_spec(cached)
            if ok:
                spec = dict(cached)
                spec["_source"] = "cache"
                return spec

    # 1) Try Solar (returns None on missing key / network failure / parse error).
    spec = call_solar(text)
    source = "solar"
    if spec is None:
        spec = heuristic_spec(text)
        source = "heuristic"
    ok, msg = validate_spec(spec)
    if not ok:
        print(f"[nl_shell] spec invalid ({msg}), falling back to heuristic",
              file=sys.stderr)
        spec = heuristic_spec(text)
        source = "heuristic-after-bad-llm"

    # 2) Cache successful Solar results (but not heuristic fallbacks — those
    #    are deterministic anyway and we don't want to memoize them as if
    #    they were the LLM answer).
    if use_cache and source == "solar":
        try:
            _cache_store(text, spec)
        except OSError as e:
            print(f"[nl_shell] cache write failed: {e}", file=sys.stderr)

    spec["_source"] = source
    return spec


def render(spec: dict) -> str:
    q = spec["queue_hint"]
    cmd = spec_to_xv6_cmd(spec)
    reason = spec.get("reason", "(no reason given)")
    src = spec.get("_source", "?")
    return (
        f"\n  source     : {src}\n"
        f"  program    : {spec['cmd']} {' '.join(str(a) for a in spec['args'])}\n"
        f"  queue_hint : {q} ({QUEUE_NAME[q]})\n"
        f"  reason     : {reason}\n"
        f"  xv6 cmd    : $ {cmd}\n"
    )


def repl(exec_mode: bool):
    print("[nl_shell] Smart-MLFQ natural-language shell")
    print("[nl_shell] type a request in plain language, or 'exit' to quit.")
    print("[nl_shell] mode: " + ("exec (forward to xv6)" if exec_mode else "dry-run (print only)"))
    while True:
        try:
            line = input("\nnl> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return
        if not line:
            continue
        if line.lower() in {"exit", "quit", ":q"}:
            return
        spec = translate(line)
        print(render(spec))
        if exec_mode:
            print("[nl_shell] --exec mode is a stub; paste the xv6 cmd"
                  " into your QEMU shell manually for now.")


SKIP_DIAG_NAMES = {"wrunner", "nlrun", "diagprog", "sh", "init"}
STATS_LINE_RE = re.compile(
    r"STATS\s+pid=(\d+)\s+priority=(\d+)\s+run=(\d+)\s+io=(\d+)"
    r"\s+arrival=(\d+)\s+completion=(\d+)\s+final_pri=(\d+)"
)


def parse_diag_output(text: str) -> list[dict]:
    """Pick STATS lines out of diagprog output. Robust to surrounding noise."""
    procs = []
    for line in text.splitlines():
        m = STATS_LINE_RE.search(line)
        if not m:
            continue
        procs.append({
            "pid": int(m.group(1)),
            "priority": int(m.group(2)),
            "run": int(m.group(3)),
            "io": int(m.group(4)),
            "arrival": int(m.group(5)),
            "completion": int(m.group(6)),
            "final_pri": int(m.group(7)),
        })
    return procs


def diagnose(stats_path: str | None) -> dict:
    if stats_path:
        with open(stats_path) as f:
            raw = f.read()
    else:
        print("[nl_shell] paste diagprog output, end with empty line + Ctrl-D:")
        raw = sys.stdin.read()

    procs = parse_diag_output(raw)
    if not procs:
        return {"error": "no STATS lines found in input"}

    # Format a small table for the LLM.
    rows = []
    for p in procs:
        rows.append(
            f"  pid={p['pid']:3d}  pri={p['priority']}  "
            f"run={p['run']:4d}  io={p['io']:4d}  "
            f"arr={p['arrival']:4d}  comp={p['completion']:4d}  "
            f"final_pri={p['final_pri']}"
        )
    table = "\n".join(rows)

    if not HAVE_OPENAI:
        return {"error": "openai SDK not installed", "raw_stats": procs}
    key = os.getenv("UPSTAGE_API_KEY")
    if not key or key == "your_api_key_here":
        return {
            "summary": "No API key — falling back to mechanical readout.",
            "concerns": [
                f"pid={p['pid']} is at LOW (priority=2) with run={p['run']}"
                for p in procs if p['priority'] == 2 and p['completion'] == 0
            ],
            "healthy": [p['pid'] for p in procs
                         if p['priority'] == 0 and p['completion'] == 0],
            "recommended": [],
            "raw_stats": procs,
        }

    base = os.getenv("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
    model = os.getenv("UPSTAGE_MODEL", "solar-pro3")
    effort = os.getenv("UPSTAGE_REASONING_EFFORT", "low")
    client = OpenAI(api_key=key, base_url=base)
    prompt = DIAGNOSE_PROMPT.format(stats_table=table)

    try:
        resp = client.chat.completions.create(
            model=model,
            messages=[{"role": "user", "content": prompt}],
            temperature=0.0,
            max_tokens=1024,
            extra_body={"reasoning_effort": effort},
        )
        raw_text = resp.choices[0].message.content.strip()
        parsed = _parse_spec(raw_text)
        if not isinstance(parsed, dict):
            return {"error": "Solar response not parseable",
                    "raw": raw_text, "raw_stats": procs}
        parsed["raw_stats"] = procs
        return parsed
    except Exception as e:
        return {"error": f"Solar call failed: {type(e).__name__}: {e}",
                "raw_stats": procs}


def render_diagnosis(result: dict) -> str:
    if "error" in result:
        return f"[diagnose] error: {result['error']}\n"
    lines = [""]
    if "summary" in result:
        lines.append(f"  summary    : {result['summary']}")
    for c in result.get("concerns", []):
        lines.append(f"  ⚠ concern  : {c}")
    healthy = result.get("healthy", [])
    if healthy:
        lines.append(f"  ✓ healthy  : pids {healthy}")
    for r in result.get("recommended", []):
        lines.append(f"  → action   : {r}")
    lines.append(f"  ({len(result.get('raw_stats', []))} live processes inspected)")
    return "\n".join(lines) + "\n"


TRACETOOL_LINE_RE = re.compile(
    r'(\{"pid"\s*:\s*\d+[^\n}]*"calls"\s*:\s*\{[^}]*\}\s*\})'
)


def _extract_tracetool_json(raw: str) -> str | None:
    """Find the tracetool dump JSON line in raw text (may be surrounded by
    QEMU console noise). Returns the JSON substring or None."""
    m = TRACETOOL_LINE_RE.search(raw)
    return m.group(1) if m else None


def analyze_tracetool(input_path: str | None) -> dict:
    """Read tracetool dump JSON (from file or stdin), call Solar with
    TRACETOOL_ANALYZE_PROMPT, return the structured verdict."""
    if input_path:
        with open(input_path) as f:
            raw = f.read()
    else:
        print("[nl_shell] paste tracetool dump JSON, end with Ctrl-D:")
        raw = sys.stdin.read()

    snippet = _extract_tracetool_json(raw) or raw.strip()
    try:
        trace = json.loads(snippet)
    except json.JSONDecodeError as e:
        return {"error": f"could not parse tracetool JSON: {e}",
                "raw_preview": snippet[:200]}

    if "calls" not in trace:
        return {"error": "input has no 'calls' field — not a tracetool dump?",
                "trace": trace}

    if not HAVE_OPENAI:
        return {"error": "openai SDK not installed", "trace": trace}
    key = os.getenv("UPSTAGE_API_KEY")
    if not key or key == "your_api_key_here":
        # Mechanical fallback when no API key
        calls = trace.get("calls", {})
        verdict = "normal"
        io_share = sum(calls.get(k, 0) for k in ("read", "write", "pipe"))
        spawn_share = sum(calls.get(k, 0) for k in ("fork", "exec"))
        if trace.get("errors", 0) * 2 > trace.get("total", 1):
            verdict = "failing"
        elif spawn_share > 4 and spawn_share * 2 > trace.get("total", 0):
            verdict = "spawner"
        elif io_share * 2 > trace.get("total", 1):
            verdict = "io_heavy"
        return {
            "verdict": verdict,
            "summary": "no API key — mechanical classification only",
            "concerns": [],
            "queue_hint": 0 if verdict == "io_heavy" else 1,
            "reason": "fallback heuristic",
            "trace": trace,
        }

    base = os.getenv("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
    model = os.getenv("UPSTAGE_MODEL", "solar-pro3")
    effort = os.getenv("UPSTAGE_REASONING_EFFORT", "low")
    client = OpenAI(api_key=key, base_url=base)
    prompt = TRACETOOL_ANALYZE_PROMPT.format(trace_json=json.dumps(trace))

    try:
        resp = client.chat.completions.create(
            model=model,
            messages=[{"role": "user", "content": prompt}],
            temperature=0.0,
            max_tokens=512,
            extra_body={"reasoning_effort": effort},
        )
        text = resp.choices[0].message.content.strip()
        if text.startswith("```"):
            text = text.strip("`")
            if text.lower().startswith("json"):
                text = text[4:].strip()
        result = json.loads(text)
        result["trace"] = trace
        return result
    except Exception as e:
        return {"error": f"Solar call failed: {type(e).__name__}: {e}",
                "trace": trace}


def render_tracetool_analysis(result: dict) -> str:
    if "error" in result:
        return f"[nl_shell] analyze-tracetool error: {result['error']}\n"
    lines = ["=" * 55, "  Tracetool LLM Analysis"]
    trace = result.get("trace", {})
    lines.append(f"  pid={trace.get('pid','?')}  "
                 f"total={trace.get('total','?')}  "
                 f"errors={trace.get('errors','?')}")
    lines.append("-" * 55)
    lines.append(f"  verdict : {result.get('verdict','?')}")
    lines.append(f"  summary : {result.get('summary','?')}")
    for c in result.get("concerns", []):
        lines.append(f"  ⚠ concern : {c}")
    if "queue_hint" in result:
        lines.append(f"  → suggested queue: {result['queue_hint']} "
                     f"({result.get('reason','')})")
    return "\n".join(lines) + "\n"


def translate_process_intent(text: str) -> str:
    """Use adapters.process_bridge to classify NL into Intent and render the
    resulting xv6 command. Single-entry-point bridge to haneol's domain."""
    from adapters import process_bridge
    intent = process_bridge.parse_nl(text)
    reject = process_bridge.guard(intent)
    if reject:
        return f"# REJECT — {reject}"
    if intent.type == "PS":
        return "ps"
    if intent.type == "SETPRIO":
        return f"setprio {intent.args['pid']} {intent.args['prio']}"
    if intent.type == "SPAWN":
        return intent.args.get("cmd", "# SPAWN — empty cmd")
    if intent.type == "KILL":
        return f"kill {intent.args['pid']}"
    if intent.type == "EXPLAIN":
        return f"# EXPLAIN — {intent.args.get('about','')}"
    return f"# {intent.type} — {intent.reason}"


def translate_thread_intent(text: str) -> str:
    """Use adapters.thread_bridge.call_solar to translate NL into a single
    xv6 shell command. Single-entry-point bridge to jinhwan's domain."""
    from adapters import thread_bridge
    return thread_bridge.call_solar(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--once", help="single request, then exit")
    ap.add_argument("--exec", action="store_true",
                    help="(stub) forward commands to QEMU instead of printing")
    ap.add_argument("--mode", choices=("scheduler", "process", "thread"),
                    default="scheduler",
                    help="which slice's NL bridge to use (default: scheduler)")
    ap.add_argument("--no-cache", action="store_true",
                    help="bypass hint cache (force a fresh Solar call)")
    ap.add_argument("--clear-cache", action="store_true",
                    help="delete the hint cache and exit")
    ap.add_argument("--diagnose-from", metavar="FILE",
                    help="run system diagnosis using a diagprog output file"
                         " (use '-' for stdin)")
    ap.add_argument("--analyze-tracetool", metavar="FILE", dest="analyze_tracetool",
                    help="analyze tracetool dump JSON (use '-' for stdin)")
    args = ap.parse_args()

    if args.diagnose_from:
        path = None if args.diagnose_from == "-" else args.diagnose_from
        result = diagnose(path)
        print(render_diagnosis(result))
        return 0

    if args.analyze_tracetool:
        path = None if args.analyze_tracetool == "-" else args.analyze_tracetool
        result = analyze_tracetool(path)
        print(render_tracetool_analysis(result))
        return 0

    if args.clear_cache:
        if CACHE_PATH.exists():
            CACHE_PATH.unlink()
            print(f"[nl_shell] removed {CACHE_PATH}")
        else:
            print(f"[nl_shell] no cache at {CACHE_PATH}")
        return 0

    if args.once:
        if args.mode == "process":
            cmd = translate_process_intent(args.once)
            print(f"\n  mode    : process (haneol slice via adapters/process_bridge)\n"
                  f"  xv6 cmd : $ {cmd}\n")
            return 0
        if args.mode == "thread":
            cmd = translate_thread_intent(args.once)
            print(f"\n  mode    : thread (jinhwan slice via adapters/thread_bridge)\n"
                  f"  xv6 cmd : $ {cmd}\n")
            return 0
        spec = translate(args.once, use_cache=not args.no_cache)
        print(render(spec))
        return 0
    repl(exec_mode=args.exec)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
