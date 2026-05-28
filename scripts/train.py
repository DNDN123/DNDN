"""Fine-tune Qwen 2.5 1.5B (or 7B) on SmartShell NL-to-spec dataset.

Run on the RTX 5070 Ti machine. Reads ../data/train.jsonl and saves
LoRA adapter + GGUF quantization.

Usage:
    python train.py                       # default Qwen 2.5 1.5B
    python train.py --model qwen-7b       # use 7B model
    python train.py --epochs 5            # override epochs
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
parser.add_argument("--model", default="qwen-1.5b",
                    choices=["qwen-0.5b", "qwen-1.5b", "qwen-3b", "qwen-7b",
                             "llama-3b"])
parser.add_argument("--epochs", type=int, default=5)
parser.add_argument("--lora-r", type=int, default=32)
parser.add_argument("--batch-size", type=int, default=16)
parser.add_argument("--lr", type=float, default=2e-4)
parser.add_argument("--data-dir", default="../data")
parser.add_argument("--output-dir", default="../models")
parser.add_argument("--skip-gguf", action="store_true",
                    help="skip GGUF conversion (faster iteration)")
args = parser.parse_args()

MODEL_MAP = {
    "qwen-0.5b": "unsloth/Qwen2.5-0.5B-Instruct-bnb-4bit",
    "qwen-1.5b": "unsloth/Qwen2.5-1.5B-Instruct-bnb-4bit",
    "qwen-3b":   "unsloth/Qwen2.5-3B-Instruct-bnb-4bit",
    "qwen-7b":   "unsloth/Qwen2.5-7B-Instruct-bnb-4bit",
    "llama-3b":  "unsloth/Llama-3.2-3B-Instruct-bnb-4bit",
}
model_name = MODEL_MAP[args.model]
output_subdir = Path(args.output_dir) / f"smartmlfq-{args.model}"
output_subdir.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Imports (heavy; deferred until after arg parsing)
# ---------------------------------------------------------------------------
import torch
from unsloth import FastLanguageModel
from datasets import Dataset
from trl import SFTTrainer
from transformers import TrainingArguments

# ---------------------------------------------------------------------------
# Load model + LoRA
# ---------------------------------------------------------------------------
print(f"[1/5] Loading model {model_name} ...")
model, tokenizer = FastLanguageModel.from_pretrained(
    model_name=model_name,
    max_seq_length=2048,
    load_in_4bit=True,
)
model = FastLanguageModel.get_peft_model(
    model,
    r=args.lora_r,
    lora_alpha=args.lora_r,
    target_modules=["q_proj", "k_proj", "v_proj", "o_proj",
                    "gate_proj", "up_proj", "down_proj"],
    lora_dropout=0,
    bias="none",
)

# ---------------------------------------------------------------------------
# Dataset
# ---------------------------------------------------------------------------
print(f"[2/5] Loading dataset from {args.data_dir} ...")
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
dataset = Dataset.from_list(raw).map(format_example, remove_columns=["user", "spec"])
print(f"   loaded {len(dataset)} training examples")

# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------
print("[3/5] Starting fine-tune ...")
trainer = SFTTrainer(
    model=model,
    tokenizer=tokenizer,
    train_dataset=dataset,
    dataset_text_field="text",
    max_seq_length=2048,
    args=TrainingArguments(
        per_device_train_batch_size=args.batch_size,
        gradient_accumulation_steps=2,
        warmup_steps=10,
        num_train_epochs=args.epochs,
        learning_rate=args.lr,
        fp16=not torch.cuda.is_bf16_supported(),
        bf16=torch.cuda.is_bf16_supported(),
        logging_steps=10,
        save_strategy="epoch",
        save_total_limit=2,
        output_dir=str(output_subdir / "checkpoints"),
        optim="adamw_8bit",
        weight_decay=0.01,
        lr_scheduler_type="linear",
        seed=42,
        report_to="none",
    ),
)
trainer.train()

# ---------------------------------------------------------------------------
# Save LoRA adapter
# ---------------------------------------------------------------------------
print(f"[4/5] Saving LoRA adapter to {output_subdir} ...")
model.save_pretrained(str(output_subdir / "lora"))
tokenizer.save_pretrained(str(output_subdir / "lora"))

# ---------------------------------------------------------------------------
# GGUF + Q4_K_M quantization
# ---------------------------------------------------------------------------
if not args.skip_gguf:
    print("[5/5] Converting to GGUF (Q4_K_M) ...")
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
    print("[5/5] Skipping GGUF (--skip-gguf)")

print()
print("=" * 60)
print("Training complete.")
print(f"  LoRA: {output_subdir / 'lora'}")
print(f"  GGUF: {output_subdir / 'gguf'} (if conversion succeeded)")
print("=" * 60)
