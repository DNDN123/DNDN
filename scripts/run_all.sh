#!/usr/bin/env bash
# SmartShell pipeline — train + evaluate.
# Modes:
#   bash run_all.sh                 — accuracy-first single run (Qwen 7B)
#   MODE=quick bash run_all.sh      — fast baseline (Qwen 1.5B, 3 epochs)
#   MODE=ablation bash run_all.sh   — train all 4 model sizes for comparison
#   MODE=ensemble bash run_all.sh   — train 3-seed ensemble on Qwen 7B

set -euo pipefail
cd "$(dirname "$0")"

MODE="${MODE:-accuracy}"
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
# Helper: train + evaluate one config
# ----------------------------------------------------------------------
run_one() {
    local tag="$1"
    shift
    echo "------------------------------------------------------------"
    echo "[$tag] python train.py $*"
    echo "------------------------------------------------------------"
    python3 train.py "$@"

    # Find latest output dir
    local out_dir
    out_dir=$(ls -td ../models/smartmlfq-* 2>/dev/null | head -1)
    [ -n "$out_dir" ] || { echo "  no output found"; return 1; }
    echo "  output: $out_dir"

    # Evaluate via HF backend (always works, no Ollama dependency)
    local lora_dir="$out_dir/lora"
    if [ -d "$lora_dir" ]; then
        python3 evaluate.py --backend hf --model "$lora_dir" \
            --output "../eval_${tag}.md"
        echo "  report: ../eval_${tag}.md"
    fi
}

case "$MODE" in
    quick)
        run_one "qwen-1.5b-quick" --model qwen-1.5b --quick
        ;;

    accuracy)
        # Single high-accuracy run: Qwen 7B, LoRA r=64, 10 epochs
        run_one "qwen-7b-accuracy" --model qwen-7b --epochs 10 --lora-r 64
        ;;

    ablation)
        # Compare 4 model sizes at same hyperparams
        echo "=== ablation study: 4 model sizes ==="
        for m in qwen-0.5b qwen-1.5b qwen-3b qwen-7b; do
            run_one "$m-ablation" --model "$m" --epochs 8 --lora-r 32 --skip-gguf
        done
        echo
        echo "=== ablation summary ==="
        for f in ../eval_qwen-*-ablation.md; do
            [ -f "$f" ] || continue
            echo
            echo "## $(basename $f)"
            head -20 "$f"
        done
        ;;

    ensemble)
        # 3-seed ensemble on Qwen 7B
        echo "=== 3-seed ensemble: Qwen 7B ==="
        for s in 42 123 7777; do
            run_one "qwen-7b-seed$s" --model qwen-7b --epochs 10 --lora-r 64 \
                --seed "$s" --skip-gguf
        done
        echo
        echo "=== ensemble: evaluate voting (next step manually) ==="
        echo "use evaluate.py with --ensemble flag (see docs)"
        ;;

    *)
        echo "Unknown MODE=$MODE. Choices: quick | accuracy | ablation | ensemble"
        exit 1
        ;;
esac

# ----------------------------------------------------------------------
# Final: register best in Ollama (only for accuracy mode)
# ----------------------------------------------------------------------
if [ "$MODE" = "accuracy" ]; then
    best_dir=$(ls -td ../models/smartmlfq-qwen-7b-* 2>/dev/null | head -1)
    if [ -n "$best_dir" ] && [ -d "$best_dir/gguf" ] && command -v ollama >/dev/null; then
        echo
        echo "=== registering with Ollama ==="
        cat > "$best_dir/gguf/Modelfile" <<EOF
FROM ./$(ls "$best_dir/gguf" | grep -i '\.gguf$' | head -1)
SYSTEM """You are an xv6 NL-to-spec classifier. Output JSON: cmd/args/queue_hint/reason."""
PARAMETER temperature 0
PARAMETER stop "<|im_end|>"
EOF
        (cd "$best_dir/gguf" && ollama create smartmlfq -f Modelfile)
        echo "  Ollama model: smartmlfq"
    fi
fi

echo
echo "============================================================"
echo "Done."
echo "============================================================"
