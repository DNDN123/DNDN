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
    for line in open(path):
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
unique = []
for line in all_lines:
    obj = json.loads(line)
    key = (obj["user"], obj["spec"]["cmd"])
    if key not in seen:
        seen.add(key)
        unique.append(line)

print(f"Total: {len(all_lines)} → after dedup: {len(unique)}")

# ---------------------------------------------------------------------------
# Stratified split by cmd
# ---------------------------------------------------------------------------
by_cmd = defaultdict(list)
for line in unique:
    cmd = json.loads(line)["spec"]["cmd"]
    by_cmd[cmd].append(line)

train, test = [], []
for cmd, lines in by_cmd.items():
    random.shuffle(lines)
    n_test = max(args.min_test_per_cmd, int(len(lines) * args.test_ratio))
    test.extend(lines[:n_test])
    train.extend(lines[n_test:])

random.shuffle(train)
random.shuffle(test)

# ---------------------------------------------------------------------------
# Write
# ---------------------------------------------------------------------------
with open(data_dir / "train.jsonl", "w") as f:
    for line in train:
        f.write(line + "\n")
with open(data_dir / "test.jsonl", "w") as f:
    for line in test:
        f.write(line + "\n")

print(f"\nWrote train.jsonl: {len(train)}, test.jsonl: {len(test)}")
print("\nTrain cmd distribution:")
print(f"  {dict(Counter(json.loads(l)['spec']['cmd'] for l in train))}")
print("Test cmd distribution:")
print(f"  {dict(Counter(json.loads(l)['spec']['cmd'] for l in test))}")
print("\nTrain queue_hint distribution:")
print(f"  {dict(Counter(json.loads(l)['spec']['queue_hint'] for l in train))}")
print("Test queue_hint distribution:")
print(f"  {dict(Counter(json.loads(l)['spec']['queue_hint'] for l in test))}")
