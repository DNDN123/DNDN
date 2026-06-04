"""Fine-tune Qwen/Llama on the SmartShell NL-to-spec dataset.

Windows-native friendly: by default uses **plain transformers + peft** (LoRA,
bf16) — no unsloth, no bitsandbytes, no triton. This is the most robust combo
on Windows + Blackwell (RTX 50xx). On Linux/WSL2 you can opt into the faster
unsloth path with --use-unsloth.

Reads ../data/train.jsonl, saves a LoRA adapter (+ merged model) under
../models/. Run from the scripts/ directory.

Usage:
    python train.py                       # qwen-1.5b, bf16, plain peft (16GB-safe)
    python train.py --model qwen-3b       # bigger, still fits 16GB
    python train.py --quick               # 3 epochs, r=16 (fast smoke test)
    python train.py --use-unsloth         # Linux/WSL2 fast path (needs unsloth)
    python train.py --load-4bit           # 4-bit (needs bitsandbytes; for 7B+)
"""
import argparse
import json
import os
import sys
import inspect
from pathlib import Path

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument("--model", default="qwen-3b",
                    choices=["qwen-0.5b", "qwen-1.5b", "qwen-3b", "qwen-7b",
                             "qwen-14b", "llama-3b", "llama-8b"])
parser.add_argument("--epochs", type=int, default=10)
parser.add_argument("--lora-r", type=int, default=64)
parser.add_argument("--lora-alpha", type=int, default=64)
parser.add_argument("--batch-size", type=int, default=4,
                    help="lowered for 3B on 16GB; raise if VRAM allows")
parser.add_argument("--grad-accum", type=int, default=8,
                    help="effective batch = batch-size * grad-accum (=32)")
parser.add_argument("--lr", type=float, default=1e-4)
parser.add_argument("--warmup-ratio", type=float, default=0.05)
parser.add_argument("--weight-decay", type=float, default=0.05)
parser.add_argument("--max-seq-length", type=int, default=2048)
parser.add_argument("--seed", type=int, default=42)
parser.add_argument("--data-dir", default="../data")
parser.add_argument("--output-dir", default="../models")
parser.add_argument("--use-unsloth", action="store_true",
                    help="use the unsloth fast path (Linux/WSL2)")
parser.add_argument("--load-4bit", action="store_true",
                    help="4-bit quantized base (needs bitsandbytes)")
parser.add_argument("--skip-merge", action="store_true",
                    help="don't merge LoRA into base after training")
parser.add_argument("--early-stop-patience", type=int, default=0)
parser.add_argument("--quick", action="store_true",
                    help="quick mode: epochs=3, r=16")
args = parser.parse_args()

if args.quick:
    args.epochs = 3
    args.lora_r = 16
    args.lora_alpha = 16

# HF model ids (plain path). The unsloth path swaps to the *-bnb-4bit repos.
HF_MODEL_MAP = {
    "qwen-0.5b": "Qwen/Qwen2.5-0.5B-Instruct",
    "qwen-1.5b": "Qwen/Qwen2.5-1.5B-Instruct",
    "qwen-3b":   "Qwen/Qwen2.5-3B-Instruct",
    "qwen-7b":   "Qwen/Qwen2.5-7B-Instruct",
    "qwen-14b":  "Qwen/Qwen2.5-14B-Instruct",
    "llama-3b":  "meta-llama/Llama-3.2-3B-Instruct",   # gated: needs HF login
    "llama-8b":  "meta-llama/Llama-3.1-8B-Instruct",   # gated: needs HF login
}
UNSLOTH_MODEL_MAP = {
    "qwen-0.5b": "unsloth/Qwen2.5-0.5B-Instruct-bnb-4bit",
    "qwen-1.5b": "unsloth/Qwen2.5-1.5B-Instruct-bnb-4bit",
    "qwen-3b":   "unsloth/Qwen2.5-3B-Instruct-bnb-4bit",
    "qwen-7b":   "unsloth/Qwen2.5-7B-Instruct-bnb-4bit",
    "qwen-14b":  "unsloth/Qwen2.5-14B-Instruct-bnb-4bit",
    "llama-3b":  "unsloth/Llama-3.2-3B-Instruct-bnb-4bit",
    "llama-8b":  "unsloth/Llama-3.1-8B-Instruct-bnb-4bit",
}
model_name = (UNSLOTH_MODEL_MAP if args.use_unsloth else HF_MODEL_MAP)[args.model]

output_subdir = Path(args.output_dir) / f"smartmlfq-{args.model}-r{args.lora_r}-e{args.epochs}"
output_subdir.mkdir(parents=True, exist_ok=True)
(output_subdir / "config.json").write_text(json.dumps(vars(args), indent=2),
                                            encoding="utf-8")

# ---------------------------------------------------------------------------
# Heavy imports (after arg parsing)
# ---------------------------------------------------------------------------
import torch
import random
random.seed(args.seed)
torch.manual_seed(args.seed)
if torch.cuda.is_available():
    torch.cuda.manual_seed_all(args.seed)
else:
    print("WARNING: CUDA not available — training on CPU will be extremely slow.")

# bf16 if supported (Ampere+; Blackwell yes), else fp16
USE_BF16 = torch.cuda.is_available() and torch.cuda.is_bf16_supported()
DTYPE = torch.bfloat16 if USE_BF16 else torch.float16

TARGET_MODULES = ["q_proj", "k_proj", "v_proj", "o_proj",
                  "gate_proj", "up_proj", "down_proj"]

# ---------------------------------------------------------------------------
# Load model + LoRA
# ---------------------------------------------------------------------------
print(f"[1/5] Loading model {model_name} "
      f"(path={'unsloth' if args.use_unsloth else 'plain'}, "
      f"4bit={args.load_4bit}, dtype={'bf16' if USE_BF16 else 'fp16'}) ...")

if args.use_unsloth:
    from unsloth import FastLanguageModel
    model, tokenizer = FastLanguageModel.from_pretrained(
        model_name=model_name, max_seq_length=args.max_seq_length,
        load_in_4bit=True,
    )
    model = FastLanguageModel.get_peft_model(
        model, r=args.lora_r, lora_alpha=args.lora_alpha,
        target_modules=TARGET_MODULES, lora_dropout=0.05, bias="none",
        use_gradient_checkpointing="unsloth", random_state=args.seed,
    )
else:
    from transformers import AutoModelForCausalLM, AutoTokenizer
    from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training

    load_kwargs = dict(torch_dtype=DTYPE, device_map="cuda")
    if args.load_4bit:
        from transformers import BitsAndBytesConfig
        load_kwargs["quantization_config"] = BitsAndBytesConfig(
            load_in_4bit=True, bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=DTYPE, bnb_4bit_use_double_quant=True,
        )

    tokenizer = AutoTokenizer.from_pretrained(model_name)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token
    model = AutoModelForCausalLM.from_pretrained(model_name, **load_kwargs)
    model.config.use_cache = False                 # required for grad checkpointing
    if args.load_4bit:
        model = prepare_model_for_kbit_training(model)
    peft_config = LoraConfig(
        r=args.lora_r, lora_alpha=args.lora_alpha, lora_dropout=0.05,
        bias="none", task_type="CAUSAL_LM", target_modules=TARGET_MODULES,
    )
    model = get_peft_model(model, peft_config)
    model.print_trainable_parameters()

# ---------------------------------------------------------------------------
# Dataset
# ---------------------------------------------------------------------------
print(f"[2/5] Loading dataset from {args.data_dir} ...")
from datasets import Dataset

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

def format_example(ex):
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": ex["user"]},
        {"role": "assistant", "content": json.dumps(ex["spec"], ensure_ascii=False)},
    ]
    return {"text": tokenizer.apply_chat_template(messages, tokenize=False)}

raw = [json.loads(l) for l in open(Path(args.data_dir) / "train.jsonl", encoding="utf-8")]
random.shuffle(raw)
dataset = Dataset.from_list(raw).map(format_example, remove_columns=["user", "spec"])
print(f"   loaded {len(dataset)} training examples")

eval_path = Path(args.data_dir) / "test.jsonl"
eval_dataset = None
if eval_path.exists():
    eval_raw = [json.loads(l) for l in open(eval_path, encoding="utf-8")][:100]
    eval_dataset = Dataset.from_list(eval_raw).map(
        format_example, remove_columns=["user", "spec"])
    print(f"   eval subset for loss monitoring: {len(eval_dataset)} examples")

# ---------------------------------------------------------------------------
# Trainer (trl SFTTrainer — version-robust construction)
# ---------------------------------------------------------------------------
print("[3/5] Starting fine-tune ...")
print(f"   epochs={args.epochs}, lora_r={args.lora_r}, "
      f"effective batch={args.batch_size * args.grad_accum}, lr={args.lr}")

from trl import SFTTrainer, SFTConfig

# transformers renamed evaluation_strategy -> eval_strategy; pick what exists.
_cfg_params = inspect.signature(SFTConfig.__init__).parameters
_eval_key = "eval_strategy" if "eval_strategy" in _cfg_params else "evaluation_strategy"
# adamw_8bit needs bitsandbytes; only use it on the 4bit/unsloth path.
_optim = "adamw_8bit" if (args.load_4bit or args.use_unsloth) else "adamw_torch"

cfg_kwargs = {
    "per_device_train_batch_size": args.batch_size,
    "gradient_accumulation_steps": args.grad_accum,
    "warmup_ratio": args.warmup_ratio,
    "num_train_epochs": args.epochs,
    "learning_rate": args.lr,
    "bf16": USE_BF16,
    "fp16": not USE_BF16,
    "logging_steps": 10,
    "save_strategy": "epoch",
    "save_total_limit": 3,
    _eval_key: "epoch" if eval_dataset else "no",
    "load_best_model_at_end": bool(eval_dataset and args.early_stop_patience > 0),
    "metric_for_best_model": "eval_loss",
    "greater_is_better": False,
    "output_dir": str(output_subdir / "checkpoints"),
    "optim": _optim,
    "weight_decay": args.weight_decay,
    "lr_scheduler_type": "cosine",
    "seed": args.seed,
    "report_to": "none",
    "gradient_checkpointing": True,
    "max_seq_length": args.max_seq_length,
    "dataset_text_field": "text",
}
# Keep only kwargs this trl/transformers version actually accepts.
cfg_kwargs = {k: v for k, v in cfg_kwargs.items() if k in _cfg_params}
sft_config = SFTConfig(**cfg_kwargs)

# tokenizer -> processing_class rename across trl versions.
_sft_params = inspect.signature(SFTTrainer.__init__).parameters
trainer_kwargs = dict(model=model, args=sft_config,
                      train_dataset=dataset, eval_dataset=eval_dataset)
if "processing_class" in _sft_params:
    trainer_kwargs["processing_class"] = tokenizer
elif "tokenizer" in _sft_params:
    trainer_kwargs["tokenizer"] = tokenizer
trainer = SFTTrainer(**trainer_kwargs)

if args.early_stop_patience > 0 and eval_dataset is not None:
    from transformers import EarlyStoppingCallback
    trainer.add_callback(EarlyStoppingCallback(
        early_stopping_patience=args.early_stop_patience))

trainer.train()

# ---------------------------------------------------------------------------
# Save LoRA adapter
# ---------------------------------------------------------------------------
print(f"[4/5] Saving LoRA adapter to {output_subdir / 'lora'} ...")
model.save_pretrained(str(output_subdir / "lora"))
tokenizer.save_pretrained(str(output_subdir / "lora"))

# ---------------------------------------------------------------------------
# Merge LoRA into base (skip for 4-bit — merge needs full-precision weights)
# ---------------------------------------------------------------------------
if not args.skip_merge and not args.load_4bit:
    print("[5/5] Merging LoRA into base model ...")
    try:
        merged = model.merge_and_unload()
        merged.save_pretrained(str(output_subdir / "merged"))
        tokenizer.save_pretrained(str(output_subdir / "merged"))
        print(f"   merged model: {output_subdir / 'merged'}")
    except Exception as e:
        print(f"   merge skipped: {e}")
else:
    print("[5/5] Skipping merge (4-bit or --skip-merge); use the LoRA adapter directly.")

print()
print("=" * 60)
print("Training complete.")
print(f"  Output: {output_subdir}")
print(f"  LoRA:   {output_subdir / 'lora'}")
print(f"  Eval:   python evaluate.py --backend hf --model {output_subdir / 'lora'}")
print("=" * 60)
