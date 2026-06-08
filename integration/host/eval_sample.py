#!/usr/bin/env python3
"""Quick accuracy probe for the NL->intent model over a random test sample.

Uses executor.classify (executor.SYSTEM_PROMPT, identical to training) against
whatever backend UPSTAGE_BASE_URL points at. Reports cmd accuracy, queue_hint
accuracy, both-correct, a per-cmd breakdown, and the first mismatches.

  UPSTAGE_BASE_URL=http://127.0.0.1:11434/v1 UPSTAGE_MODEL=smartmlfq \
  SOLAR_API_KEY=local python3 eval_sample.py --n 200 --seed 7
"""
import argparse
import json
import os
import random
from collections import defaultdict

import executor

ap = argparse.ArgumentParser()
ap.add_argument("--test", default=os.path.join(os.path.dirname(__file__),
                "..", "ml", "data", "test.jsonl"))
ap.add_argument("--n", type=int, default=200)
ap.add_argument("--seed", type=int, default=7)
args = ap.parse_args()

rows = [json.loads(l) for l in open(args.test, encoding="utf-8") if l.strip()]
random.seed(args.seed)
sample = random.sample(rows, min(args.n, len(rows)))

cmd_ok = q_ok = both = errs = 0
by_cmd = defaultdict(lambda: {"n": 0, "cmd": 0})
mism = []

for i, r in enumerate(sample):
    truth = r["spec"]
    tcmd, tq = truth.get("cmd"), truth.get("queue_hint")
    by_cmd[tcmd]["n"] += 1
    try:
        pred = executor.classify(r["user"], timeout=60)
    except Exception as e:
        errs += 1
        mism.append((r["user"], tcmd, f"ERROR {type(e).__name__}"))
        continue
    pc, pq = pred.get("cmd"), pred.get("queue_hint")
    c = pc == tcmd
    q = pq == tq
    if c: cmd_ok += 1; by_cmd[tcmd]["cmd"] += 1
    if q: q_ok += 1
    if c and q: both += 1
    if not c and len(mism) < 25:
        mism.append((r["user"], tcmd, pc))
    if (i + 1) % 25 == 0:
        print(f"...{i+1}/{len(sample)}  cmd={cmd_ok/(i+1):.3f}", flush=True)

n = len(sample)
print("\n===== ACCURACY (n=%d) =====" % n)
print(f"cmd:        {cmd_ok}/{n} = {cmd_ok/n:.1%}")
print(f"queue_hint: {q_ok}/{n} = {q_ok/n:.1%}")
print(f"both:       {both}/{n} = {both/n:.1%}")
print(f"errors:     {errs}")
print("\n--- per-cmd (cmd accuracy) ---")
for k in sorted(by_cmd):
    d = by_cmd[k]
    print(f"  {k:12} {d['cmd']}/{d['n']}")
print("\n--- first mismatches (truth -> pred) ---")
for u, t, p in mism[:20]:
    print(f"  [{t} -> {p}] {u[:60]}")
