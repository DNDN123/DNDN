#!/usr/bin/env bash
# run_eval_matrix.sh — Smart-MLFQ 3-way evaluation matrix runner.
#
# For each workload W in {cpu_heavy, io_heavy, mixed} and each mode M in
# {baseline, heuristic, solar}, this script:
#   1. Prepares the appropriate hints.txt
#   2. Boots xv6 in QEMU, runs `wrunner W.txt [hints.txt]`, captures log
#   3. Parses the log to JSON
# At the end it runs `evaluator.py --baseline --heuristic --llm` for each
# workload and dumps a metrics.json + Gantt/turnaround charts.
#
# Requires:
#   - venv activated  (matplotlib + openai available)
#   - .env with UPSTAGE_API_KEY for the Solar pass
#   - $XV6_DIR pointing at a built xv6-riscv tree (default: ~/xv6-riscv)
#
# Usage:
#   ./run_eval_matrix.sh                         # full matrix
#   ./run_eval_matrix.sh --skip-solar            # baseline + heuristic only
#   XV6_DIR=/other/xv6 ./run_eval_matrix.sh
#
# Total runtime: ~6 minutes on a typical machine (9 QEMU runs × ~30s each).

set -uo pipefail

# ---- Config -----------------------------------------------------------------
XV6_DIR="${XV6_DIR:-$HOME/xv6-riscv}"
HOST_DIR="${HOST_DIR:-$(cd "$(dirname "$0")" && pwd)}"
OUT_ROOT="${OUT_ROOT:-$HOME/mlfq-experiments/eval_$(date +%Y%m%d-%H%M%S)}"
QEMU_TIMEOUT="${QEMU_TIMEOUT:-45}"     # seconds, generous to let workload finish
SLEEP_BEFORE_CMD=4
SLEEP_AFTER_CMD=25

# Default = full 5-workload matrix. Override with WORKLOADS env var, e.g.
#   WORKLOADS="three_way realprog" ./run_eval_matrix.sh
WORKLOADS=(${WORKLOADS:-cpu_heavy io_heavy mixed three_way realprog})
MODES=(baseline heuristic solar)

SKIP_SOLAR=0
for arg in "$@"; do
  case "$arg" in
    --skip-solar) SKIP_SOLAR=1 ;;
    *) echo "unknown arg: $arg" >&2; exit 2 ;;
  esac
done

mkdir -p "$OUT_ROOT"
echo "[matrix] output root: $OUT_ROOT"
echo "[matrix] xv6 tree:    $XV6_DIR"

if [ ! -f "$XV6_DIR/kernel/kernel" ]; then
  echo "ERROR: $XV6_DIR has no built kernel — run 'make' there first." >&2
  exit 1
fi

# ---- Helpers ----------------------------------------------------------------
run_xv6() {
  # $1 = workload .txt name, $2 = hints .txt name (or "" for none)
  # $3 = output log path
  local wl="$1" hints="$2" log="$3"
  local cmd="wrunner $wl"
  if [ -n "$hints" ]; then
    cp -f "$hints" "$XV6_DIR/hints.txt"
    cmd="wrunner $wl hints.txt"
  else
    echo "# empty (baseline)" > "$XV6_DIR/hints.txt"
  fi

  (
    cd "$XV6_DIR" || exit 1
    ( sleep "$SLEEP_BEFORE_CMD"; printf '%s\n' "$cmd"; sleep "$SLEEP_AFTER_CMD"; printf '\x01x' ) \
      | timeout "$QEMU_TIMEOUT" make CPUS=1 qemu 2>&1
  ) > "$log"

  local got_done
  got_done=$(grep -c "ALL_DONE wrunner" "$log" || true)
  if [ "$got_done" = "0" ]; then
    echo "  [warn] no ALL_DONE in $log — workload may have hung or timed out"
  fi
}

parse_log() {
  local log="$1" json="$2"
  python "$HOST_DIR/parse_trace.py" "$log" --output "$json" >/dev/null 2>&1
}

# ---- Matrix run -------------------------------------------------------------
for wl in "${WORKLOADS[@]}"; do
  echo
  echo "=============================================================="
  echo "[matrix] WORKLOAD: $wl"
  echo "=============================================================="
  wl_dir="$OUT_ROOT/$wl"
  mkdir -p "$wl_dir"

  # 1) Baseline ---------------------------------------------------------------
  echo "[matrix]   → mode: baseline"
  run_xv6 "$wl.txt" "" "$wl_dir/baseline.log"
  parse_log "$wl_dir/baseline.log" "$wl_dir/baseline.json"

  # 2) Heuristic --------------------------------------------------------------
  echo "[matrix]   → mode: heuristic (generating hints.txt without API key)"
  UPSTAGE_API_KEY="" python "$HOST_DIR/llm_hint.py" \
      "$wl_dir/baseline.json" \
      --output "$wl_dir/heuristic_hints.txt" \
      >/dev/null 2>&1 || echo "    [warn] heuristic hint generation failed"
  run_xv6 "$wl.txt" "$wl_dir/heuristic_hints.txt" "$wl_dir/heuristic.log"
  parse_log "$wl_dir/heuristic.log" "$wl_dir/heuristic.json"

  # 3) Solar ------------------------------------------------------------------
  if [ "$SKIP_SOLAR" = "0" ]; then
    echo "[matrix]   → mode: solar (querying Solar Pro 3)"
    python "$HOST_DIR/llm_hint.py" \
        "$wl_dir/baseline.json" \
        --output "$wl_dir/solar_hints.txt" \
        >/dev/null 2>&1 || echo "    [warn] solar hint generation failed"
    run_xv6 "$wl.txt" "$wl_dir/solar_hints.txt" "$wl_dir/solar.log"
    parse_log "$wl_dir/solar.log" "$wl_dir/solar.json"
  else
    echo "[matrix]   → mode: solar SKIPPED (--skip-solar)"
  fi

  # 4) Evaluate this workload -------------------------------------------------
  echo "[matrix]   → evaluating $wl"
  if [ "$SKIP_SOLAR" = "0" ]; then
    python "$HOST_DIR/evaluator.py" \
        --baseline  "$wl_dir/baseline.json" \
        --heuristic "$wl_dir/heuristic.json" \
        --llm       "$wl_dir/solar.json" \
        --output    "$wl_dir/metrics.json" \
        > "$wl_dir/report.txt"
  else
    python "$HOST_DIR/evaluator.py" \
        --baseline  "$wl_dir/baseline.json" \
        --heuristic "$wl_dir/heuristic.json" \
        --output    "$wl_dir/metrics.json" \
        > "$wl_dir/report.txt"
  fi

  # 5) Charts -----------------------------------------------------------------
  echo "[matrix]   → charts for $wl"
  if [ "$SKIP_SOLAR" = "0" ]; then
    python "$HOST_DIR/viz.py" \
        --baseline "$wl_dir/baseline.json" \
        --llm      "$wl_dir/solar.json" \
        --out-dir  "$wl_dir/charts" >/dev/null 2>&1
  else
    python "$HOST_DIR/viz.py" \
        --baseline "$wl_dir/baseline.json" \
        --llm      "$wl_dir/heuristic.json" \
        --out-dir  "$wl_dir/charts" >/dev/null 2>&1
  fi

  echo "[matrix]   ✓ $wl done — see $wl_dir/report.txt"
done

# ---- Summary ----------------------------------------------------------------
echo
echo "=============================================================="
echo "[matrix] MATRIX COMPLETE — combined report"
echo "=============================================================="
{
  for wl in "${WORKLOADS[@]}"; do
    echo
    echo "### Workload: $wl ###"
    cat "$OUT_ROOT/$wl/report.txt"
  done
} > "$OUT_ROOT/combined_report.txt"
cat "$OUT_ROOT/combined_report.txt"

echo
echo "[matrix] All artefacts in: $OUT_ROOT"
echo "[matrix] Combined report : $OUT_ROOT/combined_report.txt"
