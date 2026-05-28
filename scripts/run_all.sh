#!/usr/bin/env bash
# Full pipeline runner — train + evaluate.
# Run on the RTX 5070 Ti machine after `git pull`.
#
# Usage:
#   cd scripts
#   bash run_all.sh                  # train Qwen 1.5B (default)
#   MODEL=qwen-7b bash run_all.sh    # train 7B instead

set -euo pipefail
cd "$(dirname "$0")"

MODEL="${MODEL:-qwen-1.5b}"
EPOCHS="${EPOCHS:-5}"

echo "============================================================"
echo "SmartShell pipeline — model=$MODEL epochs=$EPOCHS"
echo "============================================================"

# ----------------------------------------------------------------------
# 1. Environment check
# ----------------------------------------------------------------------
echo "[step 1] checking environment..."
python3 -c "import torch; print(f'  torch {torch.__version__}, CUDA={torch.cuda.is_available()}, device={torch.cuda.get_device_name(0) if torch.cuda.is_available() else \"none\"}')"
[ -f ../data/train.jsonl ] || { echo "ERROR: ../data/train.jsonl not found"; exit 1; }
[ -f ../data/test.jsonl  ] || { echo "ERROR: ../data/test.jsonl not found";  exit 1; }
echo "  train: $(wc -l < ../data/train.jsonl) examples"
echo "  test:  $(wc -l < ../data/test.jsonl) examples"

# ----------------------------------------------------------------------
# 2. Training
# ----------------------------------------------------------------------
echo "[step 2] training..."
python3 train.py --model "$MODEL" --epochs "$EPOCHS"

# ----------------------------------------------------------------------
# 3. Ollama registration (only if GGUF was produced)
# ----------------------------------------------------------------------
GGUF_DIR="../models/smartmlfq-$MODEL/gguf"
if [ -d "$GGUF_DIR" ]; then
    echo "[step 3] registering with Ollama..."
    if command -v ollama >/dev/null 2>&1; then
        cat > "$GGUF_DIR/Modelfile" <<EOF
FROM ./$(ls "$GGUF_DIR" | grep -i '\.gguf$' | head -1)
SYSTEM """You are an xv6 NL-to-spec classifier. Output JSON: cmd/args/queue_hint/reason."""
PARAMETER temperature 0
PARAMETER stop "<|im_end|>"
EOF
        (cd "$GGUF_DIR" && ollama create "smartmlfq-$MODEL" -f Modelfile)
        echo "  Ollama model: smartmlfq-$MODEL"
        BACKEND_ARGS="--backend ollama --model smartmlfq-$MODEL"
    else
        echo "  ollama not installed; falling back to HF backend"
        BACKEND_ARGS="--backend hf --model ../models/smartmlfq-$MODEL/lora"
    fi
else
    echo "[step 3] GGUF not produced; using HF backend"
    BACKEND_ARGS="--backend hf --model ../models/smartmlfq-$MODEL/lora"
fi

# ----------------------------------------------------------------------
# 4. Evaluation
# ----------------------------------------------------------------------
echo "[step 4] evaluating..."
python3 evaluate.py $BACKEND_ARGS --output "../eval_report_$MODEL.md"

echo
echo "============================================================"
echo "Done. See ../eval_report_$MODEL.md"
echo "============================================================"
