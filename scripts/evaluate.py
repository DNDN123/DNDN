"""Evaluate a fine-tuned SmartShell model on the held-out test set.

Loads ../data/test.jsonl, queries the model (via Ollama or direct HF),
and reports accuracy broken down by cmd, queue_hint, and language.

Usage (Ollama backend — recommended after `ollama create smartmlfq`):
    python evaluate.py --backend ollama --model smartmlfq

Usage (direct HF backend — uses LoRA adapter):
    python evaluate.py --backend hf --model ../models/smartmlfq-qwen-1.5b/lora
"""
import argparse
import json
import re
import time
from collections import defaultdict, Counter
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--backend", choices=["ollama", "hf"], default="ollama")
parser.add_argument("--model", required=True,
                    help="Ollama model name or HF model path")
parser.add_argument("--test-file", default="../data/test.jsonl")
parser.add_argument("--ollama-host", default="http://localhost:11434")
parser.add_argument("--output", default="../eval_report.md")
parser.add_argument("--limit", type=int, default=0, help="0 = all")
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
def query_ollama(user_text):
    import requests
    r = requests.post(
        f"{args.ollama_host}/v1/chat/completions",
        json={
            "model": args.model,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": user_text},
            ],
            "temperature": 0,
            "response_format": {"type": "json_object"},
        },
        timeout=30,
    )
    r.raise_for_status()
    return r.json()["choices"][0]["message"]["content"]

_hf_model = None
_hf_tok = None
def query_hf(user_text):
    global _hf_model, _hf_tok
    if _hf_model is None:
        from unsloth import FastLanguageModel
        import torch
        _hf_model, _hf_tok = FastLanguageModel.from_pretrained(
            model_name=args.model,
            max_seq_length=2048,
            load_in_4bit=True,
        )
        FastLanguageModel.for_inference(_hf_model)
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": user_text},
    ]
    prompt = _hf_tok.apply_chat_template(messages, tokenize=False,
                                          add_generation_prompt=True)
    import torch
    inputs = _hf_tok(prompt, return_tensors="pt").to("cuda")
    with torch.no_grad():
        outputs = _hf_model.generate(**inputs, max_new_tokens=256,
                                     temperature=0, do_sample=False,
                                     pad_token_id=_hf_tok.eos_token_id)
    text = _hf_tok.decode(outputs[0][inputs.input_ids.shape[1]:],
                          skip_special_tokens=True)
    return text

query = query_ollama if args.backend == "ollama" else query_hf

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def parse_json_safely(raw):
    raw = raw.strip()
    # strip code fence if any
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        # try to extract JSON object substring
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
# Run
# ---------------------------------------------------------------------------
test = [json.loads(line) for line in open(args.test_file)]
if args.limit > 0:
    test = test[:args.limit]
print(f"Evaluating {len(test)} examples against backend={args.backend} "
      f"model={args.model}")

stats = {
    "total": 0,
    "valid_json": 0,
    "correct_cmd": 0,
    "correct_queue": 0,
    "correct_both": 0,
    "by_cmd": defaultdict(lambda: {"total": 0, "cmd_ok": 0, "queue_ok": 0}),
    "by_queue": defaultdict(lambda: {"total": 0, "cmd_ok": 0, "queue_ok": 0}),
    "by_lang": defaultdict(lambda: {"total": 0, "cmd_ok": 0, "queue_ok": 0}),
    "errors": [],
    "latencies": [],
}

for i, ex in enumerate(test, 1):
    user_text = ex["user"]
    truth = ex["spec"]
    t0 = time.perf_counter()
    try:
        raw = query(user_text)
    except Exception as e:
        stats["errors"].append({"input": user_text, "error": str(e)})
        continue
    dt = time.perf_counter() - t0
    stats["latencies"].append(dt)

    pred = parse_json_safely(raw)
    stats["total"] += 1
    truth_cmd = truth.get("cmd")
    truth_q = truth.get("queue_hint")
    lang = "ko" if looks_korean(user_text) else "en"

    stats["by_cmd"][truth_cmd]["total"] += 1
    stats["by_queue"][truth_q]["total"] += 1
    stats["by_lang"][lang]["total"] += 1

    if pred is None:
        stats["errors"].append({"input": user_text, "raw": raw[:200]})
        continue
    stats["valid_json"] += 1

    cmd_ok = pred.get("cmd") == truth_cmd
    q_ok = pred.get("queue_hint") == truth_q
    if cmd_ok:
        stats["correct_cmd"] += 1
        stats["by_cmd"][truth_cmd]["cmd_ok"] += 1
        stats["by_lang"][lang]["cmd_ok"] += 1
    if q_ok:
        stats["correct_queue"] += 1
        stats["by_queue"][truth_q]["queue_ok"] += 1
        stats["by_lang"][lang]["queue_ok"] += 1
    if cmd_ok and q_ok:
        stats["correct_both"] += 1

    if i % 20 == 0:
        print(f"  [{i}/{len(test)}] cmd={stats['correct_cmd']}/{stats['total']} "
              f"queue={stats['correct_queue']}/{stats['total']}")

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
n = stats["total"]
if n == 0:
    print("No successful evaluations.")
    raise SystemExit(1)

pct = lambda x: f"{x/n*100:5.1f}%"
print()
print("=" * 60)
print(f"Overall: total={n}")
print(f"  valid JSON   : {pct(stats['valid_json'])}")
print(f"  correct cmd  : {pct(stats['correct_cmd'])}")
print(f"  correct queue: {pct(stats['correct_queue'])}")
print(f"  correct both : {pct(stats['correct_both'])}")
if stats["latencies"]:
    import statistics
    lats = stats["latencies"]
    print(f"  latency p50/p95: {statistics.median(lats)*1000:.0f}ms / "
          f"{sorted(lats)[int(len(lats)*0.95)]*1000:.0f}ms")
print("=" * 60)

# Markdown report
with open(args.output, "w") as f:
    f.write(f"# Evaluation Report — {args.model}\n\n")
    f.write(f"Backend: {args.backend}, test set: {args.test_file} (n={n})\n\n")
    f.write("## Overall\n\n")
    f.write(f"| Metric | Value |\n|---|---:|\n")
    f.write(f"| Valid JSON | {pct(stats['valid_json'])} |\n")
    f.write(f"| Correct cmd | {pct(stats['correct_cmd'])} |\n")
    f.write(f"| Correct queue_hint | {pct(stats['correct_queue'])} |\n")
    f.write(f"| Both correct | {pct(stats['correct_both'])} |\n")
    if stats["latencies"]:
        import statistics
        lats = stats["latencies"]
        f.write(f"| Latency p50 | {statistics.median(lats)*1000:.0f} ms |\n")
        f.write(f"| Latency p95 | {sorted(lats)[int(len(lats)*0.95)]*1000:.0f} ms |\n")

    f.write("\n## By cmd\n\n| cmd | n | cmd acc | queue acc |\n|---|---:|---:|---:|\n")
    for cmd, s in sorted(stats["by_cmd"].items()):
        if s["total"] == 0: continue
        f.write(f"| {cmd} | {s['total']} | "
                f"{s['cmd_ok']/s['total']*100:.1f}% | "
                f"{s['queue_ok']/s['total']*100:.1f}% |\n")

    f.write("\n## By queue_hint\n\n| queue | n | queue acc |\n|---|---:|---:|\n")
    for q, s in sorted(stats["by_queue"].items()):
        if s["total"] == 0: continue
        f.write(f"| {q} | {s['total']} | {s['queue_ok']/s['total']*100:.1f}% |\n")

    f.write("\n## By language\n\n| lang | n | cmd acc | queue acc |\n|---|---:|---:|---:|\n")
    for lang, s in sorted(stats["by_lang"].items()):
        if s["total"] == 0: continue
        f.write(f"| {lang} | {s['total']} | "
                f"{s['cmd_ok']/s['total']*100:.1f}% | "
                f"{s['queue_ok']/s['total']*100:.1f}% |\n")

    if stats["errors"]:
        f.write(f"\n## Errors / non-JSON ({len(stats['errors'])})\n\n")
        for e in stats["errors"][:30]:
            f.write(f"- `{e.get('input','?')[:80]}` → "
                    f"{e.get('error', e.get('raw',''))[:100]}\n")

print(f"\nReport written to {args.output}")
