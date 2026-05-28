"""Autonomous OS Agent main loop.

Closed loop:
  1. Sample snapshot every Δt
  2. Run cheap rule-based anomaly detector
  3. If anomalies: ask LLM for recommended actions
  4. Pass through safety guard
  5. Apply via xv6 setpri
  6. Log everything

Usage:
    # Solar API backend (current LLM)
    export UPSTAGE_API_KEY=up_...
    python main.py

    # Local fine-tuned model (after Phase 1)
    export UPSTAGE_BASE_URL=http://localhost:11434/v1
    export UPSTAGE_MODEL=smartmlfq
    export UPSTAGE_API_KEY=not-needed
    python main.py

    # Dry-run (don't actually apply actions)
    python main.py --dry-run

    # Soak test (limit time)
    python main.py --duration 600        # run 10 minutes

    # Different baselines for comparison:
    python main.py --baseline rule       # rule-based, no LLM
    python main.py --baseline random     # random setpri (sanity check)
    python main.py --baseline llm        # full LLM agent (default)
"""
from __future__ import annotations
import argparse
import json
import random
import signal
import sys
import time
from pathlib import Path

from observer import QemuDriver, take_snapshot
from anomaly_detector import detect, summarize, Concern
from llm_advisor import advise, Action
from safety_guard import SafetyGuard

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument("--xv6-dir",
                    default="../integration/xv6-riscv",
                    help="path to integrated xv6 source")
parser.add_argument("--interval", type=float, default=1.0,
                    help="snapshot period (seconds)")
parser.add_argument("--duration", type=float, default=0,
                    help="auto-stop after N seconds (0 = forever)")
parser.add_argument("--dry-run", action="store_true",
                    help="log actions but don't apply")
parser.add_argument("--baseline",
                    choices=["llm", "rule", "random", "none"],
                    default="llm",
                    help="agent strategy")
parser.add_argument("--log", default="../agent_log.jsonl",
                    help="write JSONL trace here")
args = parser.parse_args()

random.seed(42)

# ---------------------------------------------------------------------------
# Strategy functions
# ---------------------------------------------------------------------------
def strategy_rule(snap, concerns):
    """Static rule: starving procs → HIGH, runaway → LOW."""
    actions = []
    for c in concerns[:3]:
        if c.kind == "starving":
            actions.append(Action(op="setpri", pid=c.pid, new_pri=0,
                                   reason="rule: boost starving"))
        elif c.kind == "runaway":
            actions.append(Action(op="setpri", pid=c.pid, new_pri=2,
                                   reason="rule: demote runaway"))
        elif c.kind == "io_misclassified":
            actions.append(Action(op="setpri", pid=c.pid, new_pri=0,
                                   reason="rule: boost I/O-bound"))
    return actions

def strategy_random(snap, concerns):
    """Random setpri (sanity check baseline)."""
    if not snap.procs or random.random() > 0.3:
        return []
    p = random.choice([p for p in snap.procs if p.pid > 1] or [None])
    if p is None:
        return []
    return [Action(op="setpri", pid=p.pid,
                   new_pri=random.choice([0, 1, 2]),
                   reason="random")]

def strategy_llm(snap, concerns):
    return advise(snap, concerns)

def strategy_none(snap, concerns):
    return []

STRATEGIES = {
    "llm": strategy_llm,
    "rule": strategy_rule,
    "random": strategy_random,
    "none": strategy_none,
}

# ---------------------------------------------------------------------------
# Driver + log
# ---------------------------------------------------------------------------
print(f"[agent] starting (baseline={args.baseline}, dry_run={args.dry_run})")
driver = QemuDriver(args.xv6_dir)
driver.start()
print("[agent] xv6 booted")

guard = SafetyGuard()
log_f = open(args.log, "a")

def graceful_exit(sig, frame):
    print("\n[agent] shutting down...")
    driver.shutdown()
    log_f.close()
    sys.exit(0)

signal.signal(signal.SIGINT, graceful_exit)
signal.signal(signal.SIGTERM, graceful_exit)

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
start = time.time()
tick = 0
stats = {"snapshots": 0, "anomalies": 0, "actions_proposed": 0,
         "actions_applied": 0, "actions_rejected": 0}

while True:
    tick += 1
    if args.duration > 0 and (time.time() - start) > args.duration:
        print(f"[agent] duration {args.duration}s reached")
        break

    # 1. Snapshot
    try:
        snap = take_snapshot(driver, settle=0.4)
    except Exception as e:
        print(f"[agent] snapshot failed: {e}")
        time.sleep(args.interval)
        continue
    stats["snapshots"] += 1

    # 2. Anomaly detection (cheap rule filter)
    concerns = detect(snap)
    if concerns:
        stats["anomalies"] += 1
        print(f"[tick {tick}] {len(snap.procs)} procs, "
              f"{len(concerns)} concerns")
        print(summarize(concerns))

    # 3. Strategy decides what to do
    proposed = STRATEGIES[args.baseline](snap, concerns)
    stats["actions_proposed"] += len(proposed)

    # 4. Guard filters
    if proposed:
        gr = guard.check(proposed)
        if gr.rejected:
            stats["actions_rejected"] += len(gr.rejected)
            for a, why in gr.rejected:
                print(f"  [guard] REJECT pid={a.pid} prio={a.new_pri}: {why}")

        # 5. Apply
        for a in gr.accepted:
            stats["actions_applied"] += 1
            print(f"  [apply] {a.to_xv6_cmd()}  ({a.reason[:60]})")
            if not args.dry_run:
                driver.send(a.to_xv6_cmd())

        # 6. Log
        log_entry = {
            "tick": tick,
            "ts": snap.ts,
            "baseline": args.baseline,
            "n_procs": len(snap.procs),
            "concerns": [{"pid": c.pid, "kind": c.kind, "detail": c.detail}
                         for c in concerns],
            "actions_applied": [{"pid": a.pid, "new_pri": a.new_pri,
                                  "reason": a.reason} for a in gr.accepted],
            "actions_rejected": [{"pid": a.pid, "new_pri": a.new_pri,
                                   "why": w} for a, w in gr.rejected],
        }
        log_f.write(json.dumps(log_entry, ensure_ascii=False) + "\n")
        log_f.flush()

    # Periodic status
    if tick % 30 == 0:
        print(f"[agent] tick {tick} stats: {stats}")

    time.sleep(args.interval)

# ---------------------------------------------------------------------------
# Wrap up
# ---------------------------------------------------------------------------
elapsed = time.time() - start
print()
print("=" * 60)
print(f"Agent run complete (baseline={args.baseline})")
print(f"  Duration:        {elapsed:.0f}s")
print(f"  Snapshots:       {stats['snapshots']}")
print(f"  Anomalies seen:  {stats['anomalies']}")
print(f"  Actions proposed:{stats['actions_proposed']}")
print(f"  Actions applied: {stats['actions_applied']}")
print(f"  Actions rejected:{stats['actions_rejected']}")
print(f"  Log:             {args.log}")
print("=" * 60)

driver.shutdown()
log_f.close()
