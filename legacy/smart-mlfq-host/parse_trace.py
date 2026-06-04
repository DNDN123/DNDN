"""
parse_trace.py
==============
Extract TRACE/EXIT events from a captured QEMU/xv6 log text file
and emit a JSON file usable by llm_hint.py / evaluator.py / viz.py.

The log file can contain ANY surrounding noise (boot messages, user
printf interleaved, etc.). We just grep for the well-formed event
lines.

Usage:
    python3 parse_trace.py <log.txt> [--output <trace.json>]

If --output is omitted, JSON goes to stdout.

Event line formats (emitted by patched xv6 kernel):

    TRACE tick=<N> pid=<P> pri=<L> slice=<S>
    EXIT  pid=<P> name=<NAME> arrival=<A> completion=<C> run=<R> ioblock=<I> final_pri=<L>
"""
import argparse
import json
import re
import sys
from pathlib import Path

TRACE_RE = re.compile(
    r"TRACE\s+tick=(\d+)\s+pid=(\d+)\s+pri=(\d+)\s+slice=(\d+)"
)

EXIT_RE = re.compile(
    r"EXIT\s+pid=(\d+)\s+name=(\S+)\s+arrival=(\d+)\s+completion=(\d+)\s+"
    r"run=(\d+)\s+ioblock=(\d+)\s+final_pri=(\d+)"
)


def parse(log_path: Path) -> dict:
    traces = []
    exits = []
    with open(log_path, "r", errors="replace") as f:
        for line in f:
            m = TRACE_RE.search(line)
            if m:
                traces.append({
                    "kind": "trace",
                    "tick": int(m.group(1)),
                    "pid": int(m.group(2)),
                    "pri": int(m.group(3)),
                    "slice": int(m.group(4)),
                })
                continue
            m = EXIT_RE.search(line)
            if m:
                exits.append({
                    "kind": "exit",
                    "pid": int(m.group(1)),
                    "name": m.group(2),
                    "arrival": int(m.group(3)),
                    "completion": int(m.group(4)),
                    "run": int(m.group(5)),
                    "ioblock": int(m.group(6)),
                    "final_pri": int(m.group(7)),
                })
    return {
        "source_log": str(log_path),
        "events": traces + exits,
        "n_traces": len(traces),
        "n_exits": len(exits),
    }


def summarize(result: dict):
    print(f"[parse] traces: {result['n_traces']}", file=sys.stderr)
    print(f"[parse] exits:  {result['n_exits']}", file=sys.stderr)
    for e in result["events"]:
        if e["kind"] == "exit":
            tt = e["completion"] - e["arrival"]
            print(
                f"  pid={e['pid']:3d} {e['name']:14s} "
                f"arr={e['arrival']:4d} comp={e['completion']:4d} "
                f"TT={tt:4d} run={e['run']:3d} io={e['ioblock']:3d} "
                f"final_pri={e['final_pri']}",
                file=sys.stderr,
            )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log", help="captured xv6/QEMU log file")
    ap.add_argument("--output", default=None, help="output JSON (default: stdout)")
    args = ap.parse_args()

    result = parse(Path(args.log))
    summarize(result)

    blob = json.dumps(result, indent=2)
    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, "w") as f:
            f.write(blob)
        print(f"[parse] wrote {args.output}", file=sys.stderr)
    else:
        sys.stdout.write(blob)


if __name__ == "__main__":
    main()
