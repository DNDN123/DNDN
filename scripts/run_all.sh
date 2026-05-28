#!/usr/bin/env bash
# SmartShell pipeline.
# Modes:
#   bash run_all.sh                — accuracy mode + Solar baseline + comparison
#   MODE=quick bash run_all.sh     — fast baseline (Qwen 1.5B, 3 epochs) + comparison
#   MODE=ablation bash run_all.sh  — 4 model sizes
#   MODE=ensemble bash run_all.sh  — Qwen 7B × 3 seeds
#   MODE=compare-only bash run_all.sh  — just compare existing model vs Solar
#
# Requires (for comparison): UPSTAGE_API_KEY (Solar baseline)

set -euo pipefail
cd "$(dirname "$0")"

MODE="${MODE:-accuracy}"
SOLAR_MODEL="${SOLAR_MODEL:-solar-pro2}"
echo "============================================================"
echo "SmartShell pipeline — MODE=$MODE"
echo "============================================================"

# ----------------------------------------------------------------------
# Environment check
# ----------------------------------------------------------------------
python3 -c "import torch; print(f'torch {torch.__version__}, CUDA={torch.cuda.is_available()}, dev={torch.cuda.get_device_name(0) if torch.cuda.is_available() else \"none\"}')"
[ -f ../data/train.jsonl ] || { echo "ERROR: ../data/train.jsonl not found"; exit 1; }
[ -f ../data/test.jsonl  ] || { echo "ERROR: ../data/test.jsonl not found";  exit 1; }
echo "  train: $(wc -l < ../data/train.jsonl) examples"
echo "  test:  $(wc -l < ../data/test.jsonl) examples"
echo

# ----------------------------------------------------------------------
# Helper: train one config
# ----------------------------------------------------------------------
train_one() {
    local tag="$1"
    shift
    echo "------------------------------------------------------------"
    echo "[train $tag] python train.py $*"
    echo "------------------------------------------------------------"
    python3 train.py "$@"
}

# Helper: find latest model dir
latest_model() {
    ls -td ../models/smartmlfq-* 2>/dev/null | head -1
}

# ----------------------------------------------------------------------
# Step 1. Train (depending on mode)
# ----------------------------------------------------------------------
case "$MODE" in
    quick)
        train_one "quick" --model qwen-1.5b --quick
        ;;
    accuracy)
        train_one "accuracy" --model qwen-7b --epochs 10 --lora-r 64
        ;;
    ablation)
        for m in qwen-0.5b qwen-1.5b qwen-3b qwen-7b; do
            train_one "$m" --model "$m" --epochs 8 --lora-r 32 --skip-gguf
        done
        ;;
    ensemble)
        for s in 42 123 7777; do
            train_one "seed$s" --model qwen-7b --epochs 10 --lora-r 64 \
                --seed "$s" --skip-gguf
        done
        ;;
    compare-only)
        echo "Skipping training (compare-only mode)."
        ;;
    *)
        echo "Unknown MODE=$MODE"; exit 1 ;;
esac

# ----------------------------------------------------------------------
# Step 2. Register the latest model in Ollama (if GGUF exists)
# ----------------------------------------------------------------------
MODEL_DIR=$(latest_model)
OURS_NAME="smartmlfq"
if [ -n "$MODEL_DIR" ] && [ -d "$MODEL_DIR/gguf" ] && command -v ollama >/dev/null; then
    echo
    echo "=== Registering Ollama: $OURS_NAME ==="
    GGUF_FILE=$(ls "$MODEL_DIR/gguf" | grep -i '\.gguf$' | head -1)
    cat > "$MODEL_DIR/gguf/Modelfile" <<EOF
FROM ./$GGUF_FILE
SYSTEM """You are an xv6 NL-to-spec classifier. Output JSON: cmd/args/queue_hint/reason."""
PARAMETER temperature 0
PARAMETER stop "<|im_end|>"
EOF
    (cd "$MODEL_DIR/gguf" && ollama create "$OURS_NAME" -f Modelfile) || true
fi

# ----------------------------------------------------------------------
# Step 3. Evaluate ours (using LoRA via HF backend, always works)
# ----------------------------------------------------------------------
if [ -n "$MODEL_DIR" ] && [ -d "$MODEL_DIR/lora" ]; then
    echo
    echo "=== Evaluating fine-tuned model ==="
    python3 evaluate.py --backend hf --model "$MODEL_DIR/lora" \
        --output "../eval_ours.md"
fi

# ----------------------------------------------------------------------
# Step 4. Head-to-head comparison vs Solar (the actual goal!)
# ----------------------------------------------------------------------
if [ -z "${UPSTAGE_API_KEY:-}" ]; then
    echo
    echo "=== UPSTAGE_API_KEY not set — skipping Solar comparison ==="
    echo "    To compare against Solar:"
    echo "    export UPSTAGE_API_KEY=up_..."
    echo "    bash run_all.sh"
else
    echo
    echo "=== Head-to-head: Solar Pro vs Ours ==="
    # Prefer ollama backend for ours if registered
    if command -v ollama >/dev/null && ollama list 2>/dev/null | grep -q "^$OURS_NAME"; then
        OURS_BACKEND=ollama
        OURS_MODEL=$OURS_NAME
    else
        OURS_BACKEND=hf
        OURS_MODEL="$MODEL_DIR/lora"
    fi

    python3 compare.py \
        --a "Solar Pro 3"   --a-backend solar   --a-model "$SOLAR_MODEL" \
        --b "Ours"          --b-backend "$OURS_BACKEND" --b-model "$OURS_MODEL" \
        --output "../compare_solar_vs_ours.md"
fi

echo
echo "============================================================"
echo "Done. Reports:"
echo "  ../eval_ours.md             — fine-tuned model accuracy"
echo "  ../compare_solar_vs_ours.md — head-to-head comparison"
echo "============================================================"
