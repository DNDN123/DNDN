import argparse
import json
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch


SKIP_NAMES = {"wrunner", "nlrun", "sh", "init"}
COLORS = {0: "#4caf50", 1: "#ff9800", 2: "#f44336"}
CLR_BASE = "#1976d2"
CLR_LLM = "#388e3c"


def avg_tt_by_name(events):
    by_name = defaultdict(list)
    for e in events:
        if e["kind"] != "exit" or e["name"] in SKIP_NAMES:
            continue
        by_name[e["name"]].append(e["completion"] - e["arrival"])
    return {n: sum(v) / len(v) for n, v in by_name.items()}


def chart_avg_turnaround(base, llm, out_path):
    b = avg_tt_by_name(base["events"])
    l = avg_tt_by_name(llm["events"])
    names = sorted(set(list(b.keys()) + list(l.keys())))
    if not names:
        print("[viz] no programs to chart")
        return
    bs = [b.get(n, 0) for n in names]
    ls = [l.get(n, 0) for n in names]
    x = list(range(len(names)))
    w = 0.35
    fig, ax = plt.subplots(figsize=(8, 5))
    ax.bar([i - w / 2 for i in x], bs, w, label="Baseline MLFQ", color=CLR_BASE)
    ax.bar([i + w / 2 for i in x], ls, w, label="LLM-guided MLFQ", color=CLR_LLM)
    for i, (vb, vl) in enumerate(zip(bs, ls)):
        ax.text(i - w/2, vb, f"{vb:.1f}", ha="center", va="bottom", fontsize=8)
        ax.text(i + w/2, vl, f"{vl:.1f}", ha="center", va="bottom", fontsize=8)
        if vb > 0:
            delta = (vl - vb) / vb * 100
            ymax = max(vb, vl) * 1.18
            color = "#2a9d3a" if delta < 0 else "#c83232"
            ax.text(i, ymax, f"{delta:+.1f}%",
                    ha="center", fontsize=10, fontweight="bold", color=color)
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=15)
    ax.set_ylabel("Avg turnaround time (ticks)")
    ax.set_title("Average Turnaround: Baseline vs LLM-guided")
    if max(max(bs), max(ls)) > 0:
        ax.set_ylim(0, max(max(bs), max(ls)) * 1.3)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[viz] {out_path}")


def chart_per_pid(base, llm, out_path):
    def pull(trace):
        return sorted(
            [(e["pid"], e["completion"] - e["arrival"], e["name"])
             for e in trace["events"]
             if e["kind"] == "exit" and e["name"] not in SKIP_NAMES],
            key=lambda x: x[0],
        )
    bs, ls = pull(base), pull(llm)
    if not bs and not ls:
        print("[viz] no exits for per_pid chart")
        return
    fig, ax = plt.subplots(figsize=(9, 5))
    if bs:
        ax.plot([x[0] for x in bs], [x[1] for x in bs], "o-",
                label="Baseline", color=CLR_BASE)
        for pid, tt, name in bs:
            ax.annotate(name, (pid, tt), fontsize=7, alpha=0.6)
    if ls:
        ax.plot([x[0] for x in ls], [x[1] for x in ls], "s-",
                label="LLM-guided", color=CLR_LLM)
    ax.set_xlabel("PID")
    ax.set_ylabel("Turnaround time (ticks)")
    ax.set_title("Per-process Turnaround")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[viz] {out_path}")


def chart_gantt(trace, out_path, title):
    pid_segs = defaultdict(list)
    last = {}
    last_tick = 0
    sorted_traces = sorted(
        [e for e in trace["events"] if e["kind"] == "trace"],
        key=lambda x: x["tick"],
    )
    if not sorted_traces:
        print(f"[viz] (no traces) skip {title}")
        return
    for ev in sorted_traces:
        pid, tick, pri = ev["pid"], ev["tick"], ev["pri"]
        last_tick = max(last_tick, tick)
        for op, (s, p) in list(last.items()):
            if op != pid:
                pid_segs[op].append((s, tick, p))
                del last[op]
        if pid not in last:
            last[pid] = (tick, pri)
    for pid, (s, p) in last.items():
        pid_segs[pid].append((s, last_tick + 1, p))
    pid_to_name = {e["pid"]: e["name"]
                   for e in trace["events"] if e["kind"] == "exit"}
    pids = sorted(p for p in pid_segs
                  if pid_to_name.get(p, "") not in SKIP_NAMES)
    if not pids:
        pids = sorted(pid_segs.keys())
    fig, ax = plt.subplots(figsize=(10, max(3, 0.6 * len(pids) + 1)))
    for y, pid in enumerate(pids):
        for (s, e, pri) in pid_segs[pid]:
            ax.barh(y, e - s, left=s, color=COLORS.get(pri, "gray"),
                    edgecolor="black", linewidth=0.3)
    name_labels = [f"pid {p}" +
                   (f" ({pid_to_name[p]})" if p in pid_to_name else "")
                   for p in pids]
    ax.set_yticks(range(len(pids)))
    ax.set_yticklabels(name_labels)
    ax.set_xlabel("Tick")
    ax.set_title(title)
    legend = [
        Patch(facecolor=COLORS[0], label="HIGH (0)"),
        Patch(facecolor=COLORS[1], label="MID (1)"),
        Patch(facecolor=COLORS[2], label="LOW (2)"),
    ]
    ax.legend(handles=legend, loc="upper right")
    ax.grid(axis="x", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"[viz] {out_path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline", required=True)
    ap.add_argument("--llm", required=True)
    ap.add_argument("--out-dir", default="charts")
    args = ap.parse_args()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    with open(args.baseline) as f:
        base = json.load(f)
    with open(args.llm) as f:
        llm = json.load(f)
    chart_avg_turnaround(base, llm, out / "avg_turnaround.png")
    chart_per_pid(base, llm, out / "per_pid_tt.png")
    chart_gantt(base, out / "gantt_baseline.png", "Baseline MLFQ Gantt")
    chart_gantt(llm, out / "gantt_llm.png", "LLM-guided MLFQ Gantt")
    print(f"[viz] all charts saved to {out}")


if __name__ == "__main__":
    main()
