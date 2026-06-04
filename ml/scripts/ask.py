"""Quick model spot-check — natural language -> spec, using the fine-tuned
model DIRECTLY (HF backend, no server needed). Use this to verify the trained
model interactively without serving it.

  python ask.py "7번 프로세스 꺼줘"
  python ask.py "clean up zombies"
  python ask.py                      # interactive loop (type 'quit' to exit)
  python ask.py --model ../models/smartmlfq-qwen-3b-r64-e10/lora   # default
"""
import argparse
import json
import os
import re

parser = argparse.ArgumentParser()
parser.add_argument("prompt", nargs="*", help="NL request (omit for interactive)")
parser.add_argument("--model", default="../models/smartmlfq-qwen-3b-r64-e10/lora")
args = parser.parse_args()

# MUST match scripts/train.py SYSTEM_PROMPT (and evaluate.py / executor.py).
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

print(f"Loading {args.model} ...")
import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

dtype = torch.bfloat16 if (torch.cuda.is_available()
                           and torch.cuda.is_bf16_supported()) else torch.float16
device = "cuda" if torch.cuda.is_available() else "cpu"

if os.path.isfile(os.path.join(args.model, "adapter_config.json")):
    from peft import PeftConfig, PeftModel
    base_id = PeftConfig.from_pretrained(args.model).base_model_name_or_path
    tok = AutoTokenizer.from_pretrained(base_id)
    base = AutoModelForCausalLM.from_pretrained(base_id, torch_dtype=dtype, device_map=device)
    model = PeftModel.from_pretrained(base, args.model)
else:
    tok = AutoTokenizer.from_pretrained(args.model)
    model = AutoModelForCausalLM.from_pretrained(args.model, torch_dtype=dtype, device_map=device)
model.eval()
if tok.pad_token is None:
    tok.pad_token = tok.eos_token


def ask(text: str) -> str:
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": text},
    ]
    prompt = tok.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
    inputs = tok(prompt, return_tensors="pt").to(model.device)
    with torch.no_grad():
        out = model.generate(**inputs, max_new_tokens=256, do_sample=False,
                             pad_token_id=tok.eos_token_id)
    raw = tok.decode(out[0][inputs.input_ids.shape[1]:], skip_special_tokens=True).strip()
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    return raw


def show(text: str):
    raw = ask(text)
    try:
        spec = json.loads(raw)
        print(f"  {json.dumps(spec, ensure_ascii=False)}")
    except json.JSONDecodeError:
        print(f"  [non-JSON] {raw}")


if args.prompt:
    show(" ".join(args.prompt))
else:
    print("Model ready. Type a request ('quit' to exit).")
    while True:
        try:
            t = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print(); break
        if not t or t.lower() in ("quit", "exit"):
            break
        show(t)
