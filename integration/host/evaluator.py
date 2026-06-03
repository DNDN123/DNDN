"""
evaluator.py
============
Compute turnaround/response/throughput/fairness metrics from a trace
JSON, and optionally compare two traces (baseline vs LLM-guided).

Usage:
    # single trace
    python3 evaluator.py --baseline baseline.json

    # comparison
    python3 evaluator.py --baseline baseline.json --llm llm.json [--output metrics.json]
"""

import argparse
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path


SKIP_NAMES = {"wrunner", "nlrun", "sh", "init"}


STARVATION_THRESHOLD = 60   # ticks at LOW without running = starved


def _percentile(sorted_vals, q):
    """Linear-interpolated percentile (no numpy)."""
    if not sorted_vals:
        return 0
    if len(sorted_vals) == 1:
        return sorted_vals[0]
    k = (len(sorted_vals) - 1) * q
    f = int(k)
    c = min(f + 1, len(sorted_vals) - 1)
    if f == c:
        return sorted_vals[f]
    return sorted_vals[f] + (sorted_vals[c] - sorted_vals[f]) * (k - f)


def compute_metrics(trace: dict) -> dict:
    exits = [e for e in trace["events"] if e["kind"] == "exit"
             and e["name"] not in SKIP_NAMES]
    traces = [e for e in trace["events"] if e["kind"] == "trace"]

    if not exits:
        return {
            "n_exits": 0,
            "n_traces": len(traces),
            "warning": "no workload exit events captured",
        }

    tts = [e["completion"] - e["arrival"] for e in exits]

    first_run_tick = {}
    for t in traces:
        if t["pid"] not in first_run_tick:
            first_run_tick[t["pid"]] = t["tick"]
    rts = []
    for e in exits:
        first = first_run_tick.get(e["pid"], e["arrival"])
        rts.append(max(0, first - e["arrival"]))

    if exits:
        span = max(e["completion"] for e in exits) - min(e["arrival"] for e in exits)
        span = max(span, 1)
        throughput = len(exits) / span
    else:
        throughput = 0.0

    if len(tts) > 0 and sum(tts) > 0:
        s1 = sum(tts) ** 2
        s2 = len(tts) * sum(t * t for t in tts)
        jain = s1 / s2
    else:
        jain = 1.0

    # Tail/percentile metrics — important for scheduler quality (avg can
    # hide one extremely-slow process).
    tts_sorted = sorted(tts)
    rts_sorted = sorted(rts)
    p95_tt = _percentile(tts_sorted, 0.95)
    p99_tt = _percentile(tts_sorted, 0.99)
    p95_rt = _percentile(rts_sorted, 0.95)

    # Starvation count: per-pid, find the longest gap between consecutive
    # TRACE entries where pri==2 throughout. If any gap exceeds threshold
    # for any pid, that pid was starved.
    pid_traces = defaultdict(list)
    for t in traces:
        pid_traces[t["pid"]].append(t)
    starved = 0
    longest_low_gap = 0
    for pid, ev in pid_traces.items():
        ev = sorted(ev, key=lambda x: x["tick"])
        # Find longest "gap with no scheduling" while pri==2.
        for i in range(1, len(ev)):
            if ev[i-1]["pri"] == 2 and ev[i]["pri"] == 2:
                gap = ev[i]["tick"] - ev[i-1]["tick"]
                if gap > longest_low_gap:
                    longest_low_gap = gap
                if gap >= STARVATION_THRESHOLD:
                    starved += 1
                    break

    by_name = defaultdict(list)
    for e in exits:
        by_name[e["name"]].append(e["completion"] - e["arrival"])
    per_name = {n: {"avg_tt": round(statistics.mean(v), 2), "n": len(v)}
                for n, v in by_name.items()}

    return {
        "n_exits": len(exits),
        "n_traces": len(traces),
        "avg_turnaround": round(statistics.mean(tts), 2),
        "median_turnaround": round(statistics.median(tts), 2),
        "max_turnaround": max(tts),
        "p95_turnaround": round(p95_tt, 2),
        "p99_turnaround": round(p99_tt, 2),
        "avg_response": round(statistics.mean(rts), 2),
        "p95_response": round(p95_rt, 2),
        "throughput_per_tick": round(throughput, 4),
        "fairness_jain": round(jain, 4),
        "starved_pids": starved,
        "longest_low_gap": longest_low_gap,
        "per_name": per_name,
        "per_pid_tt": {e["pid"]: e["completion"] - e["arrival"] for e in exits},
    }


def _safe_pct(new, old):
    """Percent delta. Returns None when baseline is 0/None — so the caller
    can render 'n/a' without silently dropping the row. Previously the
    callers used `if base.get(k)` which removed starvation regressions
    (baseline=0, llm=5) from the report entirely."""
    if old is None or new is None:
        return None
    if old == 0:
        return None     # delta undefined, but the metric itself is kept
    return round((new - old) / old * 100, 2)


def compare(base: dict, llm: dict) -> dict:
    summary = {}
    keys = ["avg_turnaround", "median_turnaround", "max_turnaround",
            "p95_turnaround", "p99_turnaround",
            "avg_response", "p95_response",
            "throughput_per_tick", "fairness_jain",
            "starved_pids", "longest_low_gap"]
    for k in keys:
        if k in base and k in llm:
            summary[k] = {
                "baseline": base[k],
                "llm": llm[k],
                "delta_pct": _safe_pct(llm[k], base[k]),
            }

    by_name = {}
    base_names = base.get("per_name", {})
    llm_names = llm.get("per_name", {})
    for n in set(list(base_names) + list(llm_names)):
        b = base_names.get(n, {}).get("avg_tt")
        l = llm_names.get(n, {}).get("avg_tt")
        entry = {"baseline_avg_tt": b, "llm_avg_tt": l,
                 "delta_pct": _safe_pct(l, b)}
        by_name[n] = entry

    return {"summary": summary, "per_name": by_name}


def print_report(base: dict, llm: dict, cmp: dict):
    print()
    print("=" * 72)
    print("Smart-MLFQ Evaluation Report")
    print("=" * 72)
    print(f"  Baseline: {base['n_exits']} workload exits, {base['n_traces']} traces")
    print(f"  LLM:      {llm['n_exits']} workload exits, {llm['n_traces']} traces")
    print()
    def _fmt(v):
        """Render delta cell — handles None (baseline=0 case) without crashing."""
        if v is None: return "n/a"
        if isinstance(v, float): return f"{v:.2f}"
        return str(v)

    print(f"  {'Metric':<22} {'Baseline':>12} {'LLM':>12} {'Δ%':>10}")
    print("  " + "-" * 60)
    for k, d in cmp["summary"].items():
        print(f"  {k:<22} {_fmt(d['baseline']):>12} {_fmt(d['llm']):>12} {_fmt(d['delta_pct']):>10}")

    print()
    print(f"  {'Program':<18} {'Base TT':>10} {'LLM TT':>10} {'Δ%':>10}")
    print("  " + "-" * 50)
    for name, d in cmp["per_name"].items():
        print(f"  {name:<18} {_fmt(d.get('baseline_avg_tt')):>10} "
              f"{_fmt(d.get('llm_avg_tt')):>10} {_fmt(d.get('delta_pct')):>10}")
    print()


def print_three_way(base: dict, heur: dict, llm: dict):
    """Side-by-side report for baseline / heuristic / Solar runs."""
    print()
    print("=" * 84)
    print("Smart-MLFQ 3-way Evaluation Report")
    print("=" * 84)
    print(f"  Baseline:  {base['n_exits']} exits, {base['n_traces']} traces")
    print(f"  Heuristic: {heur['n_exits']} exits, {heur['n_traces']} traces")
    print(f"  LLM:       {llm['n_exits']} exits, {llm['n_traces']} traces")
    print()
    metric_keys = ["avg_turnaround", "median_turnaround", "max_turnaround",
                   "p95_turnaround", "p99_turnaround",
                   "avg_response", "p95_response",
                   "throughput_per_tick", "fairness_jain",
                   "starved_pids", "longest_low_gap"]
    print(f"  {'Metric':<22} {'Baseline':>12} {'Heuristic':>12} {'LLM':>12} "
          f"{'Δheur%':>9} {'Δllm%':>9}")
    print("  " + "-" * 80)
    for k in metric_keys:
        bv = base.get(k)
        hv = heur.get(k)
        lv = llm.get(k)
        if bv is None or hv is None or lv is None:
            continue
        dh = _safe_pct(hv, bv)
        dl = _safe_pct(lv, bv)
        dh_s = f"{dh:>8.2f}%" if dh is not None else "     n/a "
        dl_s = f"{dl:>8.2f}%" if dl is not None else "     n/a "
        print(f"  {k:<22} {bv:>12} {hv:>12} {lv:>12} {dh_s} {dl_s}")

    print()
    print(f"  {'Program':<18} {'Base TT':>10} {'Heur TT':>10} {'LLM TT':>10} "
          f"{'Δheur%':>9} {'Δllm%':>9}")
    print("  " + "-" * 75)
    base_n = base.get("per_name", {})
    heur_n = heur.get("per_name", {})
    llm_n  = llm.get("per_name", {})
    all_names = sorted(set(base_n) | set(heur_n) | set(llm_n))
    for name in all_names:
        b = base_n.get(name, {}).get("avg_tt")
        h = heur_n.get(name, {}).get("avg_tt")
        l = llm_n.get(name, {}).get("avg_tt")
        dh = _safe_pct(h, b)
        dl = _safe_pct(l, b)
        bs = "-" if b is None else str(b)
        hs = "-" if h is None else str(h)
        ls = "-" if l is None else str(l)
        dhs = "n/a" if dh is None else f"{dh}%"
        dls = "n/a" if dl is None else f"{dl}%"
        print(f"  {name:<18} {bs:>10} {hs:>10} {ls:>10} {dhs:>9} {dls:>9}")
    print()


def three_way_compare(base: dict, heur: dict, llm: dict) -> dict:
    """Structured comparison: baseline vs heuristic vs Solar."""
    out = {"summary": {}, "per_name": {}}
    metric_keys = ["avg_turnaround", "median_turnaround", "max_turnaround",
                   "p95_turnaround", "p99_turnaround",
                   "avg_response", "p95_response",
                   "throughput_per_tick", "fairness_jain",
                   "starved_pids", "longest_low_gap"]
    for k in metric_keys:
        bv = base.get(k); hv = heur.get(k); lv = llm.get(k)
        if bv is None or hv is None or lv is None:
            continue
        out["summary"][k] = {
            "baseline": bv,
            "heuristic": hv,
            "llm": lv,
            "delta_heur_pct": _safe_pct(hv, bv),
            "delta_llm_pct":  _safe_pct(lv, bv),
        }
    bn = base.get("per_name", {}); hn = heur.get("per_name", {})
    ln = llm.get("per_name", {})
    for name in set(bn) | set(hn) | set(ln):
        b = bn.get(name, {}).get("avg_tt")
        h = hn.get(name, {}).get("avg_tt")
        l = ln.get(name, {}).get("avg_tt")
        out["per_name"][name] = {
            "baseline_avg_tt": b, "heuristic_avg_tt": h, "llm_avg_tt": l,
            "delta_heur_pct": _safe_pct(h, b),
            "delta_llm_pct":  _safe_pct(l, b),
        }
    return out


def _load(path: str) -> dict:
    with open(path) as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True,
                    help="trace JSON from a run without LLM hints")
    ap.add_argument("--llm", default=None,
                    help="trace JSON from a Solar-guided run")
    ap.add_argument("--heuristic", default=None,
                    help="trace JSON from a heuristic-guided run (3-way mode)")
    ap.add_argument("--output", default=None,
                    help="write metrics + comparison to JSON")
    args = ap.parse_args()

    base_m = compute_metrics(_load(args.baseline))

    if args.heuristic and args.llm:
        heur_m = compute_metrics(_load(args.heuristic))
        llm_m  = compute_metrics(_load(args.llm))
        cmp = three_way_compare(base_m, heur_m, llm_m)
        print_three_way(base_m, heur_m, llm_m)
        result = {"baseline": base_m, "heuristic": heur_m, "llm": llm_m,
                  "comparison": cmp}
    elif args.llm:
        llm_m = compute_metrics(_load(args.llm))
        cmp = compare(base_m, llm_m)
        print_report(base_m, llm_m, cmp)
        result = {"baseline": base_m, "llm": llm_m, "comparison": cmp}
    elif args.heuristic:
        heur_m = compute_metrics(_load(args.heuristic))
        cmp = compare(base_m, heur_m)
        print_report(base_m, heur_m, cmp)
        result = {"baseline": base_m, "heuristic": heur_m, "comparison": cmp}
    else:
        result = {"baseline": base_m}
        print(json.dumps(base_m, indent=2))

    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, "w") as f:
            json.dump(result, f, indent=2)
        print(f"[eval] saved {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
