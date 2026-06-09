"""Capture a clean on-device demo transcript for slides.

Loads the fine-tuned model ONCE and runs a set of representative prompts,
printing each request -> JSON spec with per-prompt wall-clock time. CPU-tuned
(short max_new_tokens, all cores) so it is bearable without a GPU.

  ../../host/venv/bin/python capture_ondevice.py            # default prompts
  ../../host/venv/bin/python capture_ondevice.py "내 문장"   # one custom prompt
"""
import json
import os
import re
import sys
import time

# Use all CPU cores for matmuls (no GPU here).
os.environ.setdefault("OMP_NUM_THREADS", str(os.cpu_count() or 8))

MODEL = os.environ.get("MODEL", "../models/smartmlfq-qwen-3b-r64-e10/lora")

# MUST match scripts/train.py SYSTEM_PROMPT (and ask.py / executor.py).
SYSTEM_PROMPT = (
    "You are an xv6 natural-language OS shell. Convert the user request into a "
    "single JSON object with keys cmd, args, queue_hint, reason.\n"
    "cmd is one of:\n"
    "  workloads: cpu_burner | io_burner | mixed_burner (args=[iterations]); "
    "set queue_hint 0=HIGH (interactive/IO), 1=MID, 2=LOW (heavy/background).\n"
    "  files: ls (args=[path?]) | cat (args=[path]) | rm (args=[path]) | "
    "mkdir (args=[path]) | ln (args=[target,linkname]) | echo (args=[text]).\n"
    "  process: ps (args=[]) | kill (args=[pid]) | setpri (args=[pid,prio 0..2]) | "
    "trace (args=[pid,\"on\"|\"off\"]).\n"
    "  cleanup: killall (args=[name]) | killheavy (args=[count?]) | reap (args=[]).\n"
    "  system: uptime (args=[]) | sysinfo (args=[]).\n"
    "  info: explain (args=[topic]) — answer only, no OS action.\n"
    "  reject — unsafe/unsupported/out-of-scope (killing pid 0 or 1, networking, "
    "sudo, gibberish).\n"
    "For every non-workload command set queue_hint to 0. args is a list of "
    "strings. reason is a short English sentence. Output JSON only, no markdown."
)

DEFAULT_PROMPTS = [
    "무거운 작업 백그라운드로 돌려줘",
    "7번 프로세스 꺼줘",
    "좀비 정리해줘",
    "무거운 프로세스 다 정리해줘",
    "프로세스 목록 보여줘",
    "run a heavy job in the background",
    "init 프로세스 죽여줘",          # safety: should reject (pid 1)
]

prompts = sys.argv[1:] or DEFAULT_PROMPTS

print(f"[capture] loading {MODEL} (CPU, {os.environ['OMP_NUM_THREADS']} threads) ...",
      flush=True)
t0 = time.time()
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

torch.set_num_threads(int(os.environ["OMP_NUM_THREADS"]))
dtype = torch.float16 if not torch.cuda.is_available() else torch.bfloat16
device = "cuda" if torch.cuda.is_available() else "cpu"

if os.path.isfile(os.path.join(MODEL, "adapter_config.json")):
    from peft import PeftConfig, PeftModel
    base_id = PeftConfig.from_pretrained(MODEL).base_model_name_or_path
    tok = AutoTokenizer.from_pretrained(base_id)
    base = AutoModelForCausalLM.from_pretrained(base_id, torch_dtype=dtype, device_map=device)
    model = PeftModel.from_pretrained(base, MODEL)
else:
    tok = AutoTokenizer.from_pretrained(MODEL)
    model = AutoModelForCausalLM.from_pretrained(MODEL, torch_dtype=dtype, device_map=device)
model.eval()
if tok.pad_token is None:
    tok.pad_token = tok.eos_token
print(f"[capture] model ready in {time.time()-t0:.1f}s  device={device} dtype={dtype}\n",
      flush=True)


def ask(text):
    messages = [{"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": text}]
    prompt = tok.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
    inputs = tok(prompt, return_tensors="pt").to(model.device)
    with torch.no_grad():
        out = model.generate(**inputs, max_new_tokens=96, do_sample=False,
                             pad_token_id=tok.eos_token_id)
    raw = tok.decode(out[0][inputs.input_ids.shape[1]:], skip_special_tokens=True).strip()
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    return raw


print("=" * 64)
print("  On-device model (Qwen2.5-3B + our LoRA) — NL -> xv6 command")
print("=" * 64)
for text in prompts:
    t = time.time()
    raw = ask(text)
    dt = time.time() - t
    try:
        raw = json.dumps(json.loads(raw), ensure_ascii=False)
    except json.JSONDecodeError:
        pass
    print(f"\n$ {text}")
    print(f"  -> {raw}")
    print(f"  ({dt:.1f}s)", flush=True)
print("\n" + "=" * 64)
