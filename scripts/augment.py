"""Automated data augmentation via Solar/Claude API.

For each seed example, generate N paraphrases that preserve the spec.
Saves to ../data/augmented_auto.jsonl (separate from manual augmented.jsonl).

Usage:
    # Use Solar (uses UPSTAGE_API_KEY)
    python augment.py --backend solar --multiplier 10

    # Use Claude (uses ANTHROPIC_API_KEY)
    python augment.py --backend claude --multiplier 10

    # Test on a small subset first
    python augment.py --backend solar --multiplier 5 --limit 20

The generated file is then combined with manual data in build_train.py.
"""
import argparse
import json
import os
import re
import time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed

parser = argparse.ArgumentParser()
parser.add_argument("--backend", choices=["solar", "claude"], default="solar")
parser.add_argument("--multiplier", type=int, default=10,
                    help="paraphrases per seed")
parser.add_argument("--seeds-file", default="../data/seeds.jsonl")
parser.add_argument("--output", default="../data/augmented_auto.jsonl")
parser.add_argument("--limit", type=int, default=0,
                    help="0 = all seeds; >0 = first N seeds (for testing)")
parser.add_argument("--workers", type=int, default=4,
                    help="parallel API calls")
parser.add_argument("--retry", type=int, default=3)
args = parser.parse_args()

# ---------------------------------------------------------------------------
# Backend
# ---------------------------------------------------------------------------
PROMPT_TEMPLATE = """You generate paraphrases for a natural-language OS shell dataset.

Given an original user request and its target spec, produce {n} alternative
user expressions that mean the SAME THING (same spec). Vary:
- formality (formal vs casual vs abbreviated)
- vocabulary (synonyms, slang)
- word order
- language register (Korean particles, English contractions)
- length (terse vs verbose)
- include 1-2 with minor typos or unusual phrasing

DO NOT change the spec. Only vary the user text.

Original user: "{user}"
Target spec: {spec_json}

Output a JSON array of {n} strings (the new user expressions only, no spec).
Output JSON only, no markdown, no explanation.
"""

def call_solar(prompt):
    import requests
    api_key = os.environ.get("UPSTAGE_API_KEY") or os.environ.get("SOLAR_API_KEY")
    if not api_key:
        raise RuntimeError("UPSTAGE_API_KEY not set")
    r = requests.post(
        "https://api.upstage.ai/v1/solar/chat/completions",
        headers={"Authorization": f"Bearer {api_key}"},
        json={
            "model": "solar-pro2",
            "messages": [{"role": "user", "content": prompt}],
            "temperature": 0.8,
            "response_format": {"type": "json_object"},
        },
        timeout=60,
    )
    r.raise_for_status()
    return r.json()["choices"][0]["message"]["content"]

def call_claude(prompt):
    import requests
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        raise RuntimeError("ANTHROPIC_API_KEY not set")
    r = requests.post(
        "https://api.anthropic.com/v1/messages",
        headers={
            "x-api-key": api_key,
            "anthropic-version": "2023-06-01",
            "content-type": "application/json",
        },
        json={
            "model": "claude-opus-4-7",
            "max_tokens": 2000,
            "messages": [{"role": "user", "content": prompt}],
            "temperature": 0.8,
        },
        timeout=60,
    )
    r.raise_for_status()
    return r.json()["content"][0]["text"]

backend_fn = call_solar if args.backend == "solar" else call_claude

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def parse_array_response(raw):
    raw = raw.strip()
    # strip code fences
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    # try direct array first
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        pass
    # if it's wrapped in {"variations": [...]} or similar
    try:
        obj = json.loads(raw)
        for key in ("variations", "paraphrases", "outputs", "results", "items"):
            if isinstance(obj.get(key), list):
                return obj[key]
        # last resort: any list value
        for v in obj.values():
            if isinstance(v, list):
                return v
    except json.JSONDecodeError:
        pass
    # extract first JSON array in text
    m = re.search(r"\[.*\]", raw, re.DOTALL)
    if m:
        try:
            return json.loads(m.group(0))
        except json.JSONDecodeError:
            return None
    return None

def paraphrase_seed(seed, n_attempts=3):
    user = seed["user"]
    spec_json = json.dumps(seed["spec"], ensure_ascii=False)
    prompt = PROMPT_TEMPLATE.format(n=args.multiplier, user=user,
                                     spec_json=spec_json)
    for attempt in range(n_attempts):
        try:
            raw = backend_fn(prompt)
            variants = parse_array_response(raw)
            if variants and isinstance(variants, list):
                # filter: must be strings, non-empty, different from original
                clean = [
                    v.strip() for v in variants
                    if isinstance(v, str) and v.strip() and v.strip() != user
                ]
                return clean[:args.multiplier]
        except Exception as e:
            print(f"  retry ({attempt+1}/{n_attempts}) for '{user[:30]}': {e}")
            time.sleep(2 * (attempt + 1))
    return []

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
seeds = [json.loads(line) for line in open(args.seeds_file)]
if args.limit > 0:
    seeds = seeds[:args.limit]
print(f"Augmenting {len(seeds)} seeds × {args.multiplier} variants each "
      f"(backend={args.backend})")

# Append-only output (so we can resume)
out_f = open(args.output, "a")
existing = 0
if Path(args.output).exists():
    existing = sum(1 for _ in open(args.output))
    print(f"  {existing} lines already in {args.output}; appending")

total_new = 0
errors = 0

def worker(i_seed):
    i, seed = i_seed
    variants = paraphrase_seed(seed)
    return i, seed, variants

with ThreadPoolExecutor(max_workers=args.workers) as ex:
    futures = {ex.submit(worker, (i, s)): i for i, s in enumerate(seeds, 1)}
    for fut in as_completed(futures):
        try:
            i, seed, variants = fut.result()
        except Exception as e:
            errors += 1
            print(f"  worker error: {e}")
            continue
        if not variants:
            errors += 1
            continue
        for v in variants:
            new_line = {"user": v, "spec": seed["spec"]}
            out_f.write(json.dumps(new_line, ensure_ascii=False) + "\n")
            out_f.flush()
            total_new += 1
        if i % 20 == 0:
            print(f"  [{i}/{len(seeds)}] +{total_new} new examples "
                  f"(errors: {errors})")

out_f.close()
print(f"\nDone. Wrote {total_new} new examples to {args.output} "
      f"(total lines: {existing + total_new}, errors: {errors})")
