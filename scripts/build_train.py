"""Combine all data sources and produce final train/test split.

Sources:
    ../data/seeds.jsonl              — manual seeds (300)
    ../data/augmented.jsonl          — manual augmentation (Claude in-session)
    ../data/augmented_auto.jsonl     — auto augmentation (Solar/Claude API, optional)

Outputs:
    ../data/train.jsonl              — stratified 80%
    ../data/test.jsonl               — held-out 20%

Usage:
    python build_train.py                       # default split
    python build_train.py --test-ratio 0.15     # 85/15 split
    python build_train.py --seed 7              # different shuffle seed
"""
import argparse
import json
import random
from pathlib import Path
from collections import defaultdict, Counter

parser = argparse.ArgumentParser()
parser.add_argument("--data-dir", default="../data")
parser.add_argument("--sources", nargs="+",
                    default=["seeds.jsonl", "augmented.jsonl", "augmented_auto.jsonl"])
parser.add_argument("--test-ratio", type=float, default=0.2)
parser.add_argument("--seed", type=int, default=42)
parser.add_argument("--min-test-per-cmd", type=int, default=5)
parser.add_argument("--balance", choices=["none", "oversample"], default="none",
                    help="oversample minority queue_hint classes in TRAIN only "
                         "(test set is never touched)")
args = parser.parse_args()

random.seed(args.seed)

# ---------------------------------------------------------------------------
# Load all sources
# ---------------------------------------------------------------------------
data_dir = Path(args.data_dir)
all_lines = []
for src in args.sources:
    path = data_dir / src
    if not path.exists():
        print(f"  skip {src} (not found)")
        continue
    n = 0
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line:
            continue
        # validate JSON
        try:
            obj = json.loads(line)
            assert "user" in obj and "spec" in obj
            all_lines.append(line)
            n += 1
        except (json.JSONDecodeError, AssertionError):
            print(f"  invalid line skipped in {src}")
    print(f"  loaded {n} from {src}")

# ---------------------------------------------------------------------------
# Deduplicate by (user, cmd) pair
# ---------------------------------------------------------------------------
seen = set()
unique = []           # list of parsed dicts
for line in all_lines:
    obj = json.loads(line)
    key = (obj["user"], obj["spec"]["cmd"])
    if key not in seen:
        seen.add(key)
        unique.append(obj)

print(f"Total: {len(all_lines)} → after dedup: {len(unique)}")

# ---------------------------------------------------------------------------
# Group paraphrases by provenance, then stratified split BY GROUP.
#
# group_key = obj["source"] (parent seed, stamped by augment.py) or obj["user"]
# (a seed has no "source", so it groups under its own text; its paraphrases
# carry source == that same text → they land in the same group). Splitting
# whole groups guarantees a seed and all its variants stay on the SAME side,
# eliminating paraphrase leakage between train and test.
#
# NOTE: augmented rows produced before this change lack "source" and therefore
# group as singletons (no worse than the old behavior). Regenerate with the
# updated augment.py for full leakage protection.
# ---------------------------------------------------------------------------
groups = defaultdict(list)            # group_key -> list of dicts
for obj in unique:
    gkey = obj.get("source") or obj["user"]
    groups[gkey].append(obj)

# Stratify groups by cmd (all members of a group share the same spec/cmd).
by_cmd_groups = defaultdict(list)     # cmd -> list of groups
for gkey, members in groups.items():
    cmd = members[0]["spec"]["cmd"]
    by_cmd_groups[cmd].append(members)

train, test = [], []
for cmd, glist in by_cmd_groups.items():
    random.shuffle(glist)
    # choose whole groups for test until the per-cmd row quota is met
    target_test = max(args.min_test_per_cmd,
                      int(sum(len(g) for g in glist) * args.test_ratio))
    n_test_rows = 0
    gi = 0
    while gi < len(glist) and n_test_rows < target_test:
        test.extend(glist[gi])
        n_test_rows += len(glist[gi])
        gi += 1
    for g in glist[gi:]:
        train.extend(g)

# ---------------------------------------------------------------------------
# Optional: balance TRAIN by queue_hint (oversample minority classes).
# Test set is never modified.
# ---------------------------------------------------------------------------
if args.balance == "oversample":
    by_q = defaultdict(list)
    for obj in train:
        by_q[obj["spec"]["queue_hint"]].append(obj)
    target = max(len(v) for v in by_q.values())
    balanced = []
    for q, items in by_q.items():
        balanced.extend(items)
        if len(items) < target:
            extra = random.choices(items, k=target - len(items))  # with replacement
            balanced.extend(extra)
    before = len(train)
    train = balanced
    print(f"Balanced train by queue_hint: {before} → {len(train)} "
          f"(target {target}/class)")

random.shuffle(train)
random.shuffle(test)

# ---------------------------------------------------------------------------
# Write — normalize to {user, spec} only (drop provenance so the downstream
# train/eval schema stays exactly 2 keys).
# ---------------------------------------------------------------------------
def _norm(obj):
    return json.dumps({"user": obj["user"], "spec": obj["spec"]},
                      ensure_ascii=False)

with open(data_dir / "train.jsonl", "w", encoding="utf-8") as f:
    for obj in train:
        f.write(_norm(obj) + "\n")
with open(data_dir / "test.jsonl", "w", encoding="utf-8") as f:
    for obj in test:
        f.write(_norm(obj) + "\n")

print(f"\nWrote train.jsonl: {len(train)}, test.jsonl: {len(test)}")
print("\nTrain cmd distribution:")
print(f"  {dict(Counter(o['spec']['cmd'] for o in train))}")
print("Test cmd distribution:")
print(f"  {dict(Counter(o['spec']['cmd'] for o in test))}")
print("\nTrain queue_hint distribution:")
print(f"  {dict(Counter(o['spec']['queue_hint'] for o in train))}")
print("Test queue_hint distribution:")
print(f"  {dict(Counter(o['spec']['queue_hint'] for o in test))}")
