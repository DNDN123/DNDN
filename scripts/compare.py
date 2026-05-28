"""Head-to-head comparison: fine-tuned model vs Solar Pro 3 (or any baseline).

Runs the same test set against two backends, computes win/tie/loss per
example, and writes a markdown comparison report.

Usage:
    # Compare fine-tuned vs Solar
    export UPSTAGE_API_KEY=up_...
    python compare.py \\
        --a "Solar Pro 3"    --a-backend solar   --a-model solar-pro2 \\
        --b "Ours (7B)"      --b-backend ollama  --b-model smartmlfq \\
        --output ../compare_solar_vs_ours.md

    # Compare ours vs Claude
    export ANTHROPIC_API_KEY=sk-ant-...
    python compare.py \\
        --a "Claude Opus 4.7" --a-backend claude --a-model claude-opus-4-7 \\
        --b "Ours (7B)"       --b-backend ollama --b-model smartmlfq

    # Compare two of our checkpoints
    python compare.py \\
        --a "Ours 1.5B" --a-backend ollama --a-model smartmlfq-qwen-1.5b \\
        --b "Ours 7B"   --b-backend ollama --b-model smartmlfq-qwen-7b
"""
from __future__ import annotations
import argparse
import json
import os
import re
import time
from collections import defaultdict, Counter
from pathlib import Path
from typing import Optional

parser = argparse.ArgumentParser()
parser.add_argument("--a", required=True, help="display name for backend A")
parser.add_argument("--a-backend", required=True,
                    choices=["solar", "claude", "ollama", "hf"])
parser.add_argument("--a-model", required=True)
parser.add_argument("--b", required=True, help="display name for backend B")
parser.add_argument("--b-backend", required=True,
                    choices=["solar", "claude", "ollama", "hf"])
parser.add_argument("--b-model", required=True)
parser.add_argument("--test-file", default="../data/test.jsonl")
parser.add_argument("--ollama-host", default="http://localhost:11434")
parser.add_argument("--output", default="../compare_report.md")
parser.add_argument("--limit", type=int, default=0)
args = parser.parse_args()

SYSTEM_PROMPT = (
    "You are an xv6 NL-to-spec classifier. Given a user request, output a "
    "single JSON object with keys cmd, args, queue_hint, reason. "
    "cmd in {cpu_burner, io_burner, mixed_burner, echo, cat, ls, reject}. "
    "queue_hint in {0=HIGH, 1=MID, 2=LOW}. "
    "args is a list of strings. reason is a short English sentence. "
    "Output JSON only, no markdown fences."
)

# ---------------------------------------------------------------------------
# Backends
# ---------------------------------------------------------------------------
def make_solar_client():
    import requests
    api_key = os.environ.get("UPSTAGE_API_KEY") or os.environ.get("SOLAR_API_KEY")
    if not api_key:
        raise RuntimeError("UPSTAGE_API_KEY not set (needed for solar backend)")
    def call(user_text, model):
        r = requests.post(
            "https://api.upstage.ai/v1/solar/chat/completions",
            headers={"Authorization": f"Bearer {api_key}"},
            json={"model": model,
                  "messages": [{"role": "system", "content": SYSTEM_PROMPT},
                               {"role": "user", "content": user_text}],
                  "temperature": 0,
                  "response_format": {"type": "json_object"}},
            timeout=30,
        )
        r.raise_for_status()
        return r.json()["choices"][0]["message"]["content"]
    return call

def make_claude_client():
    import requests
    api_key = os.environ.get("ANTHROPIC_API_KEY")
    if not api_key:
        raise RuntimeError("ANTHROPIC_API_KEY not set (needed for claude backend)")
    def call(user_text, model):
        r = requests.post(
            "https://api.anthropic.com/v1/messages",
            headers={"x-api-key": api_key,
                     "anthropic-version": "2023-06-01",
                     "content-type": "application/json"},
            json={"model": model,
                  "max_tokens": 256,
                  "system": SYSTEM_PROMPT,
                  "messages": [{"role": "user", "content": user_text}],
                  "temperature": 0},
            timeout=30,
        )
        r.raise_for_status()
        return r.json()["content"][0]["text"]
    return call

def make_ollama_client():
    import requests
    def call(user_text, model):
        r = requests.post(
            f"{args.ollama_host}/v1/chat/completions",
            json={"model": model,
                  "messages": [{"role": "system", "content": SYSTEM_PROMPT},
                               {"role": "user", "content": user_text}],
                  "temperature": 0,
                  "response_format": {"type": "json_object"}},
            timeout=30,
        )
        r.raise_for_status()
        return r.json()["choices"][0]["message"]["content"]
    return call

_hf_models = {}
def make_hf_client():
    def call(user_text, model_path):
        if model_path not in _hf_models:
            from unsloth import FastLanguageModel
            m, t = FastLanguageModel.from_pretrained(
                model_name=model_path, max_seq_length=2048, load_in_4bit=True)
            FastLanguageModel.for_inference(m)
            _hf_models[model_path] = (m, t)
        import torch
        m, t = _hf_models[model_path]
        messages = [{"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": user_text}]
        prompt = t.apply_chat_template(messages, tokenize=False,
                                        add_generation_prompt=True)
        inputs = t(prompt, return_tensors="pt").to("cuda")
        with torch.no_grad():
            out = m.generate(**inputs, max_new_tokens=256, temperature=0,
                             do_sample=False, pad_token_id=t.eos_token_id)
        return t.decode(out[0][inputs.input_ids.shape[1]:],
                        skip_special_tokens=True)
    return call

BACKEND_MAKERS = {
    "solar":  make_solar_client,
    "claude": make_claude_client,
    "ollama": make_ollama_client,
    "hf":     make_hf_client,
}

# ---------------------------------------------------------------------------
# Eval helpers
# ---------------------------------------------------------------------------
def parse_json_safely(raw):
    raw = raw.strip()
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        m = re.search(r"\{.*\}", raw, re.DOTALL)
        if m:
            try:
                return json.loads(m.group(0))
            except json.JSONDecodeError:
                return None
        return None

def looks_korean(s):
    return any('가' <= ch <= '힣' for ch in s)

# ---------------------------------------------------------------------------
# Run both backends on the test set
# ---------------------------------------------------------------------------
print(f"Comparing:")
print(f"  A: {args.a}  ({args.a_backend} / {args.a_model})")
print(f"  B: {args.b}  ({args.b_backend} / {args.b_model})")
print()

backend_a = BACKEND_MAKERS[args.a_backend]()
backend_b = BACKEND_MAKERS[args.b_backend]()

test = [json.loads(line) for line in open(args.test_file)]
if args.limit > 0:
    test = test[:args.limit]
print(f"Test set: {len(test)} examples\n")

def score(pred, truth):
    if pred is None:
        return {"valid": False, "cmd_ok": False, "queue_ok": False, "both": False}
    cmd_ok = pred.get("cmd") == truth.get("cmd")
    q_ok = pred.get("queue_hint") == truth.get("queue_hint")
    return {"valid": True, "cmd_ok": cmd_ok, "queue_ok": q_ok,
            "both": cmd_ok and q_ok}

results = []
for i, ex in enumerate(test, 1):
    user = ex["user"]
    truth = ex["spec"]
    lang = "ko" if looks_korean(user) else "en"

    # Query A
    t0 = time.perf_counter()
    try:
        raw_a = backend_a(user, args.a_model)
        pred_a = parse_json_safely(raw_a)
        dt_a = time.perf_counter() - t0
    except Exception as e:
        pred_a, dt_a = None, None

    # Query B
    t0 = time.perf_counter()
    try:
        raw_b = backend_b(user, args.b_model)
        pred_b = parse_json_safely(raw_b)
        dt_b = time.perf_counter() - t0
    except Exception as e:
        pred_b, dt_b = None, None

    sa = score(pred_a, truth)
    sb = score(pred_b, truth)
    results.append({
        "user": user, "truth_cmd": truth["cmd"], "truth_q": truth["queue_hint"],
        "lang": lang, "sa": sa, "sb": sb, "lat_a": dt_a, "lat_b": dt_b,
    })

    if i % 20 == 0:
        a_acc = sum(r["sa"]["both"] for r in results) / len(results) * 100
        b_acc = sum(r["sb"]["both"] for r in results) / len(results) * 100
        print(f"  [{i}/{len(test)}] A: {a_acc:.1f}%  B: {b_acc:.1f}%")

# ---------------------------------------------------------------------------
# Aggregate
# ---------------------------------------------------------------------------
n = len(results)
def acc(side, metric):
    return sum(r[side][metric] for r in results) / n * 100

a_metrics = {k: acc("sa", k) for k in ["valid", "cmd_ok", "queue_ok", "both"]}
b_metrics = {k: acc("sb", k) for k in ["valid", "cmd_ok", "queue_ok", "both"]}

# Head-to-head
wins_a = sum(1 for r in results if r["sa"]["both"] and not r["sb"]["both"])
wins_b = sum(1 for r in results if r["sb"]["both"] and not r["sa"]["both"])
ties = sum(1 for r in results if r["sa"]["both"] == r["sb"]["both"])

# Per-language
by_lang = defaultdict(lambda: {"n": 0, "a": 0, "b": 0})
for r in results:
    by_lang[r["lang"]]["n"] += 1
    by_lang[r["lang"]]["a"] += int(r["sa"]["both"])
    by_lang[r["lang"]]["b"] += int(r["sb"]["both"])

# Per-cmd
by_cmd = defaultdict(lambda: {"n": 0, "a": 0, "b": 0})
for r in results:
    by_cmd[r["truth_cmd"]]["n"] += 1
    by_cmd[r["truth_cmd"]]["a"] += int(r["sa"]["both"])
    by_cmd[r["truth_cmd"]]["b"] += int(r["sb"]["both"])

# Latency
lat_a = [r["lat_a"] for r in results if r["lat_a"]]
lat_b = [r["lat_b"] for r in results if r["lat_b"]]

import statistics
def med(xs): return statistics.median(xs) * 1000 if xs else 0
def p95(xs): return sorted(xs)[int(len(xs)*0.95)] * 1000 if xs else 0

# ---------------------------------------------------------------------------
# Console summary
# ---------------------------------------------------------------------------
print("\n" + "=" * 70)
print(f"Test set: {n} examples")
print()
print(f"{'Metric':<20} {args.a:>20} {args.b:>20}")
print("-" * 70)
for k in ["valid", "cmd_ok", "queue_ok", "both"]:
    print(f"{k:<20} {a_metrics[k]:>19.1f}% {b_metrics[k]:>19.1f}%")
print(f"{'latency p50 (ms)':<20} {med(lat_a):>20.0f} {med(lat_b):>20.0f}")
print(f"{'latency p95 (ms)':<20} {p95(lat_a):>20.0f} {p95(lat_b):>20.0f}")
print("-" * 70)
print(f"Head-to-head: {args.a} wins {wins_a} | {args.b} wins {wins_b} | ties {ties}")
diff = b_metrics["both"] - a_metrics["both"]
print(f"Δ accuracy (B - A): {diff:+.1f} percentage points")
print("=" * 70)

# ---------------------------------------------------------------------------
# Markdown report
# ---------------------------------------------------------------------------
out = open(args.output, "w")
def w(s=""): out.write(s + "\n")

w(f"# Head-to-Head Comparison\n")
w(f"- **A**: {args.a}  ({args.a_backend} / `{args.a_model}`)")
w(f"- **B**: {args.b}  ({args.b_backend} / `{args.b_model}`)")
w(f"- **Test set**: `{args.test_file}` ({n} examples)\n")

w("## Overall accuracy\n")
w(f"| Metric | {args.a} | {args.b} | Δ (B−A) |")
w(f"|---|---:|---:|---:|")
for k, lbl in [("valid","Valid JSON"), ("cmd_ok","Correct cmd"),
                ("queue_ok","Correct queue_hint"), ("both","Both correct")]:
    diff = b_metrics[k] - a_metrics[k]
    w(f"| {lbl} | {a_metrics[k]:.1f}% | {b_metrics[k]:.1f}% | {diff:+.1f}pp |")
w(f"| latency p50 | {med(lat_a):.0f} ms | {med(lat_b):.0f} ms | "
   f"{(med(lat_b)-med(lat_a)):+.0f} ms |")
w(f"| latency p95 | {p95(lat_a):.0f} ms | {p95(lat_b):.0f} ms | "
   f"{(p95(lat_b)-p95(lat_a)):+.0f} ms |\n")

w(f"## Head-to-head\n")
w(f"| | Count | % |\n|---|---:|---:|")
w(f"| **{args.b}** wins | {wins_b} | {wins_b/n*100:.1f}% |")
w(f"| **{args.a}** wins | {wins_a} | {wins_a/n*100:.1f}% |")
w(f"| ties | {ties} | {ties/n*100:.1f}% |\n")

w("## By language (both correct)\n")
w(f"| lang | n | {args.a} | {args.b} | Δ |")
w(f"|---|---:|---:|---:|---:|")
for lang, s in sorted(by_lang.items()):
    a = s["a"]/s["n"]*100
    b = s["b"]/s["n"]*100
    w(f"| {lang} | {s['n']} | {a:.1f}% | {b:.1f}% | {b-a:+.1f}pp |")

w("\n## By cmd (both correct)\n")
w(f"| cmd | n | {args.a} | {args.b} | Δ |")
w(f"|---|---:|---:|---:|---:|")
for cmd, s in sorted(by_cmd.items()):
    a = s["a"]/s["n"]*100
    b = s["b"]/s["n"]*100
    w(f"| {cmd} | {s['n']} | {a:.1f}% | {b:.1f}% | {b-a:+.1f}pp |")

# Cases where A correct but B wrong (errors to learn from)
b_loses = [r for r in results if r["sa"]["both"] and not r["sb"]["both"]]
if b_loses:
    w(f"\n## Cases where {args.b} lost to {args.a} (sample 20)\n")
    w(f"| input | truth_cmd | truth_q |")
    w(f"|---|---|---:|")
    for r in b_loses[:20]:
        w(f"| `{r['user'][:60]}` | {r['truth_cmd']} | {r['truth_q']} |")

out.close()
print(f"\nReport written to {args.output}")

# Also dump raw results for further analysis
raw_path = args.output.replace(".md", "_raw.jsonl")
with open(raw_path, "w") as f:
    for r in results:
        f.write(json.dumps(r, ensure_ascii=False, default=str) + "\n")
print(f"Raw results: {raw_path}")
