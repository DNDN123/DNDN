"""Fine-tune Qwen/Llama on SmartShell NL-to-spec dataset.

Configured for **accuracy-first** mode (slow but maximum quality).
For faster iteration, override with CLI flags.

Run on the RTX 5070 Ti machine. Reads ../data/train.jsonl and saves
LoRA adapter + GGUF quantization.

Usage:
    python train.py                       # default Qwen 2.5 7B accuracy mode
    python train.py --model qwen-1.5b     # faster baseline
    python train.py --quick               # quick mode (epochs=3, no GGUF)
    python train.py --seed 1 --seed 2     # multi-seed ensemble (not yet)
"""
import argparse
import json
import os
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument("--model", default="qwen-7b",
                    choices=["qwen-0.5b", "qwen-1.5b", "qwen-3b", "qwen-7b",
                             "qwen-14b", "llama-3b", "llama-8b"])
parser.add_argument("--epochs", type=int, default=10,
                    help="accuracy mode default 10")
parser.add_argument("--lora-r", type=int, default=64,
                    help="accuracy mode default 64 (was 32)")
parser.add_argument("--lora-alpha", type=int, default=64)
parser.add_argument("--batch-size", type=int, default=8,
                    help="reduced for larger model")
parser.add_argument("--grad-accum", type=int, default=4,
                    help="effective batch = batch-size * grad-accum")
parser.add_argument("--lr", type=float, default=1e-4,
                    help="accuracy mode lower lr for stability")
parser.add_argument("--warmup-ratio", type=float, default=0.05,
                    help="proportional warmup, more stable than fixed steps")
parser.add_argument("--weight-decay", type=float, default=0.05,
                    help="higher to combat overfitting")
parser.add_argument("--seed", type=int, default=42)
parser.add_argument("--data-dir", default="../data")
parser.add_argument("--output-dir", default="../models")
parser.add_argument("--skip-gguf", action="store_true",
                    help="skip GGUF conversion (faster iteration)")
parser.add_argument("--quick", action="store_true",
                    help="quick mode: epochs=3, r=16, no GGUF")
parser.add_argument("--early-stop-patience", type=int, default=0,
                    help="if >0, stop when eval loss doesn't improve")
args = parser.parse_args()

if args.quick:
    args.epochs = 3
    args.lora_r = 16
    args.lora_alpha = 16
    args.skip_gguf = True

MODEL_MAP = {
    "qwen-0.5b": "unsloth/Qwen2.5-0.5B-Instruct-bnb-4bit",
    "qwen-1.5b": "unsloth/Qwen2.5-1.5B-Instruct-bnb-4bit",
    "qwen-3b":   "unsloth/Qwen2.5-3B-Instruct-bnb-4bit",
    "qwen-7b":   "unsloth/Qwen2.5-7B-Instruct-bnb-4bit",
    "qwen-14b":  "unsloth/Qwen2.5-14B-Instruct-bnb-4bit",
    "llama-3b":  "unsloth/Llama-3.2-3B-Instruct-bnb-4bit",
    "llama-8b":  "unsloth/Llama-3.1-8B-Instruct-bnb-4bit",
}
model_name = MODEL_MAP[args.model]
output_subdir = Path(args.output_dir) / f"smartmlfq-{args.model}-r{args.lora_r}-e{args.epochs}"
output_subdir.mkdir(parents=True, exist_ok=True)

# Save config for reproducibility
(output_subdir / "config.json").write_text(json.dumps(vars(args), indent=2))

# ---------------------------------------------------------------------------
# Imports (heavy; deferred until after arg parsing)
# ---------------------------------------------------------------------------
import torch
import random
random.seed(args.seed)
torch.manual_seed(args.seed)
if torch.cuda.is_available():
    torch.cuda.manual_seed_all(args.seed)

from unsloth import FastLanguageModel
from datasets import Dataset
from trl import SFTTrainer
from transformers import TrainingArguments

# ---------------------------------------------------------------------------
# Load model + LoRA
# ---------------------------------------------------------------------------
print(f"[1/6] Loading model {model_name} ...")
model, tokenizer = FastLanguageModel.from_pretrained(
    model_name=model_name,
    max_seq_length=2048,
    load_in_4bit=True,
)
model = FastLanguageModel.get_peft_model(
    model,
    r=args.lora_r,
    lora_alpha=args.lora_alpha,
    target_modules=["q_proj", "k_proj", "v_proj", "o_proj",
                    "gate_proj", "up_proj", "down_proj"],
    lora_dropout=0.05,                # small dropout helps generalization
    bias="none",
    use_gradient_checkpointing="unsloth",
    random_state=args.seed,
)

# ---------------------------------------------------------------------------
# Dataset
# ---------------------------------------------------------------------------
print(f"[2/6] Loading dataset from {args.data_dir} ...")
SYSTEM_PROMPT = (
    "You are an xv6 NL-to-spec classifier. Given a user request, output a "
    "single JSON object with keys cmd, args, queue_hint, reason. "
    "cmd in {cpu_burner, io_burner, mixed_burner, echo, cat, ls, reject}. "
    "queue_hint in {0=HIGH, 1=MID, 2=LOW}. "
    "args is a list of strings. reason is a short English sentence. "
    "Output JSON only, no markdown fences."
)

def format_example(ex):
    user = ex["user"]
    assistant = json.dumps(ex["spec"], ensure_ascii=False)
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": user},
        {"role": "assistant", "content": assistant},
    ]
    return {"text": tokenizer.apply_chat_template(messages, tokenize=False)}

raw = [json.loads(line) for line in open(Path(args.data_dir) / "train.jsonl")]
random.shuffle(raw)
dataset = Dataset.from_list(raw).map(format_example, remove_columns=["user", "spec"])
print(f"   loaded {len(dataset)} training examples")

# Load eval set for monitoring (subset of test)
eval_path = Path(args.data_dir) / "test.jsonl"
eval_dataset = None
if eval_path.exists():
    eval_raw = [json.loads(line) for line in open(eval_path)][:100]   # subset
    eval_dataset = Dataset.from_list(eval_raw).map(
        format_example, remove_columns=["user", "spec"]
    )
    print(f"   eval subset for loss monitoring: {len(eval_dataset)} examples")

# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------
print("[3/6] Starting fine-tune (accuracy mode) ...")
print(f"   epochs={args.epochs}, lora_r={args.lora_r}, "
      f"effective batch={args.batch_size * args.grad_accum}, "
      f"lr={args.lr}, warmup={args.warmup_ratio*100:.0f}%, "
      f"wd={args.weight_decay}")

trainer = SFTTrainer(
    model=model,
    tokenizer=tokenizer,
    train_dataset=dataset,
    eval_dataset=eval_dataset,
    dataset_text_field="text",
    max_seq_length=2048,
    args=TrainingArguments(
        per_device_train_batch_size=args.batch_size,
        gradient_accumulation_steps=args.grad_accum,
        warmup_ratio=args.warmup_ratio,
        num_train_epochs=args.epochs,
        learning_rate=args.lr,
        fp16=not torch.cuda.is_bf16_supported(),
        bf16=torch.cuda.is_bf16_supported(),
        logging_steps=10,
        save_strategy="epoch",
        save_total_limit=3,
        evaluation_strategy="epoch" if eval_dataset else "no",
        load_best_model_at_end=bool(eval_dataset and args.early_stop_patience > 0),
        metric_for_best_model="eval_loss",
        greater_is_better=False,
        output_dir=str(output_subdir / "checkpoints"),
        optim="adamw_8bit",
        weight_decay=args.weight_decay,
        lr_scheduler_type="cosine",     # cosine for accuracy mode
        seed=args.seed,
        report_to="none",
        gradient_checkpointing=True,
    ),
)

# Early stopping callback (optional)
if args.early_stop_patience > 0 and eval_dataset is not None:
    from transformers import EarlyStoppingCallback
    trainer.add_callback(EarlyStoppingCallback(
        early_stopping_patience=args.early_stop_patience
    ))

trainer.train()

# ---------------------------------------------------------------------------
# Save LoRA adapter
# ---------------------------------------------------------------------------
print(f"[4/6] Saving LoRA adapter to {output_subdir} ...")
model.save_pretrained(str(output_subdir / "lora"))
tokenizer.save_pretrained(str(output_subdir / "lora"))

# ---------------------------------------------------------------------------
# Merge LoRA into base for easier serving (optional but recommended)
# ---------------------------------------------------------------------------
print("[5/6] Merging LoRA into base model ...")
try:
    merged = model.merge_and_unload()
    merged.save_pretrained(str(output_subdir / "merged"))
    tokenizer.save_pretrained(str(output_subdir / "merged"))
    print(f"   merged model: {output_subdir / 'merged'}")
except Exception as e:
    print(f"   merge skipped: {e}")

# ---------------------------------------------------------------------------
# GGUF + Q4_K_M quantization (for Ollama serving)
# ---------------------------------------------------------------------------
if not args.skip_gguf:
    print("[6/6] Converting to GGUF (Q4_K_M) ...")
    try:
        model.save_pretrained_gguf(
            str(output_subdir / "gguf"),
            tokenizer,
            quantization_method="q4_k_m",
        )
        print(f"   GGUF saved to {output_subdir / 'gguf'}")
    except Exception as e:
        print(f"   GGUF conversion failed: {e}")
        print("   You can still use the LoRA adapter directly.")
else:
    print("[6/6] Skipping GGUF (--skip-gguf)")

print()
print("=" * 60)
print("Training complete.")
print(f"  Output:  {output_subdir}")
print(f"  Config:  {output_subdir / 'config.json'}")
print(f"  LoRA:    {output_subdir / 'lora'}")
print(f"  Merged:  {output_subdir / 'merged'} (if successful)")
print(f"  GGUF:    {output_subdir / 'gguf'} (if successful)")
print("=" * 60)
