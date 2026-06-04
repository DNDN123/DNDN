"""Offline, rule-based data augmentation — NO API, NO cost, deterministic.

For each seed, generate up to N meaning-preserving surface variants and write
them to ../data/augmented_auto.jsonl (the file build_train.py already reads).
Every variant records `source` (the parent seed text) so build_train.py keeps
a seed and its variants on the same side of the train/test split (no leakage).

Design rule: ONLY surface transforms that cannot change the label.
  - No reordering, no semantic word swaps that could flip cmd/queue_hint.
  - Synonyms are a small, curated, domain-equivalent set, and are NOT applied
    to `reject` examples (so an adversarial input never gets reworded into a
    different intent).
  - Korean variants use safe leading fillers + punctuation only (no verb
    surgery), so grammar stays intact regardless of the sentence.

Usage:
    python augment_offline.py                      # 8 variants/seed
    python augment_offline.py --multiplier 12
    python augment_offline.py --seeds-file ../data/seeds.jsonl --typo-rate 0.15
"""
from __future__ import annotations
import argparse
import json
import random
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--seeds-file", default="../data/seeds.jsonl")
parser.add_argument("--output", default="../data/augmented_auto.jsonl")
parser.add_argument("--multiplier", type=int, default=8,
                    help="max variants generated per seed")
parser.add_argument("--typo-rate", type=float, default=0.2,
                    help="fraction of variants that get one light typo")
parser.add_argument("--seed", type=int, default=42)
args = parser.parse_args()

rng = random.Random(args.seed)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def has_hangul(s: str) -> bool:
    return any('가' <= ch <= '힣' for ch in s)

# Korean leading fillers that are grammatical before a casual request/phrase.
KO_PREFIX = ["", "", "좀 ", "한번 ", "일단 ", "지금 ", "그 "]
KO_SUFFIX = ["", "", "!", ".", "~"]

# English politeness wrappers (prefix) + suffix.
EN_PREFIX = ["", "", "please ", "can you ", "could you ", "pls "]
EN_SUFFIX = ["", "", " please", "!", "."]

# Curated domain-equivalent synonyms. Whole-word, case-insensitive. Each inner
# list is a set of mutually interchangeable surface forms in THIS domain.
EN_SYNONYMS = [
    ["run", "execute", "start", "launch"],
    ["quickly", "fast", "rapidly"],
    ["quick", "fast"],
    ["heavy", "intensive", "huge"],
    ["background", "the background"],
    ["computation", "calculation", "compute job"],
    ["job", "task", "workload"],
]

def apply_one_synonym(text: str) -> str:
    """Swap one whole-word occurrence using one synonym set (case-insensitive)."""
    sets = EN_SYNONYMS[:]
    rng.shuffle(sets)
    for syn in sets:
        for i, w in enumerate(syn):
            pat = re.compile(rf"\b{re.escape(w)}\b", re.IGNORECASE)
            if pat.search(text):
                repl = rng.choice([x for x in syn if x != w])
                # preserve leading capitalization of the matched word
                def _sub(m):
                    return repl[0].upper() + repl[1:] if m.group(0)[0].isupper() else repl
                return pat.sub(_sub, text, count=1)
    return text

_TOKEN_RE = re.compile(r"[A-Za-z]{4,}")
def inject_typo(text: str) -> str:
    """Swap two interior letters of one alphabetic token (>=4 letters)."""
    tokens = [(m.start(), m.group(0)) for m in _TOKEN_RE.finditer(text)]
    if not tokens:
        return text
    start, tok = rng.choice(tokens)
    if len(tok) < 4:
        return text
    i = rng.randint(1, len(tok) - 3)         # interior, keep first/last stable-ish
    swapped = tok[:i] + tok[i+1] + tok[i] + tok[i+2:]
    return text[:start] + swapped + text[start+len(tok):]

def make_variant(user: str, is_reject: bool, allow_typo: bool) -> str:
    ko = has_hangul(user)
    out = user
    # 1. synonym swap (English, non-reject only)
    if not ko and not is_reject and rng.random() < 0.6:
        out = apply_one_synonym(out)
    # 2. case (English only, occasionally)
    if not ko and rng.random() < 0.3:
        out = out.lower() if rng.random() < 0.5 else (out[0:1].upper() + out[1:])
    # 3. prefix / suffix wrappers
    if ko:
        out = rng.choice(KO_PREFIX) + out + rng.choice(KO_SUFFIX)
    else:
        out = rng.choice(EN_PREFIX) + out + rng.choice(EN_SUFFIX)
    # 4. light typo
    if allow_typo:
        out = inject_typo(out)
    # 5. normalize whitespace
    out = re.sub(r"\s+", " ", out).strip()
    return out

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
seeds = [json.loads(l) for l in open(args.seeds_file, encoding="utf-8") if l.strip()]
print(f"Loaded {len(seeds)} seeds; generating up to {args.multiplier} variants each")

total = 0
seen_global = set()
with open(args.output, "w", encoding="utf-8") as out_f:
    for seed in seeds:
        user = seed["user"]
        spec = seed["spec"]
        is_reject = spec.get("cmd") == "reject"
        seen_local = {user}
        produced = 0
        tries = 0
        max_tries = args.multiplier * 15
        while produced < args.multiplier and tries < max_tries:
            tries += 1
            allow_typo = rng.random() < args.typo_rate
            v = make_variant(user, is_reject, allow_typo)
            if not v or v in seen_local or v in seen_global:
                continue
            seen_local.add(v)
            seen_global.add(v)
            out_f.write(json.dumps(
                {"user": v, "spec": spec, "source": user},
                ensure_ascii=False) + "\n")
            produced += 1
            total += 1

print(f"Wrote {total} offline variants to {args.output}")
print("Next: python build_train.py   (it auto-picks up augmented_auto.jsonl,")
print("      groups variants with their seed, and re-splits with no leakage)")
