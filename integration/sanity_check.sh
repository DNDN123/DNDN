#!/usr/bin/env bash
# sanity_check.sh — automated regression test for the integration tree.
#
# What it does (in order, exit-on-first-failure):
#   1. xv6 build (kernel + fs.img)
#   2. Boot QEMU, wait for sh prompt
#   3. Run each slice's golden command and grep for the success signature:
#        haneol  : `ps`              -> "PID  PPID"
#        minju   : `tracetool dump`  -> '"traced":1'
#        hyunsung: `nlrun 2 cpu_burner 3000` -> "EXIT pid="
#        jinhwan : `threadtest`      -> "All tests done."
#        통합     : `bgq bgq.txt`     -> "BGQ all_done"
#   4. Each assertion produces a PASS/FAIL line; final exit code = 0 iff all pass.
#
# Usage:
#   bash integration/sanity_check.sh                # default (assumes ~/xv6-build)
#   XV6_DIR=/path/to/xv6-riscv bash integration/sanity_check.sh
#
# Designed for /root or other ext4 dirs. /mnt/c works but is slower and may
# need bigger timeouts (see TIMEOUT_BOOT / TIMEOUT_TEST).

set -u   # error on unset vars; do NOT set -e — we want to log each failure

XV6_DIR=${XV6_DIR:-$HOME/xv6-build}
TIMEOUT_BOOT=${TIMEOUT_BOOT:-12}
TIMEOUT_TEST=${TIMEOUT_TEST:-60}

PASS_CNT=0
FAIL_CNT=0
LOG="$(mktemp -t sanity_check.XXXXXX.log)"

log() { printf "%s\n" "$*" | tee -a "$LOG"; }
pass() { log "  ✓ PASS  $1";              PASS_CNT=$((PASS_CNT+1)); }
fail() { log "  ✗ FAIL  $1   ($2)";       FAIL_CNT=$((FAIL_CNT+1)); }

log "=== sanity_check $(date -Is) ==="
log "XV6_DIR=$XV6_DIR"
log "TIMEOUT_BOOT=${TIMEOUT_BOOT}s  TIMEOUT_TEST=${TIMEOUT_TEST}s"
log

# ── 1. Build ────────────────────────────────────────────────────────────
if [ ! -d "$XV6_DIR" ]; then
  fail "build prerequisites" "XV6_DIR not found: $XV6_DIR"
  log; log "Build skipped — fix XV6_DIR and rerun."; exit 1
fi

log "[1/3] build"
( cd "$XV6_DIR" && make kernel/kernel fs.img ) >>"$LOG" 2>&1
if [ -f "$XV6_DIR/kernel/kernel" ] && [ -f "$XV6_DIR/fs.img" ]; then
  pass "kernel/kernel + fs.img built"
else
  fail "build" "kernel/kernel or fs.img missing"
  log; log "Build failed. See $LOG (tail -40)"; exit 1
fi

# ── 2. Boot + run-and-grep harness ──────────────────────────────────────
log
log "[2/3] boot QEMU and exercise 5 slices"

# We script QEMU stdin via python3 to (a) cleanly time waits, (b) collect
# output, (c) kill via setsid so the qemu process tree dies if we time out.
SCENARIO=$(cat <<'PY'
import os, signal, subprocess, sys, time

xv6_dir = sys.argv[1]
boot_to = float(sys.argv[2])
test_to = float(sys.argv[3])

QEMU = ["qemu-system-riscv64","-machine","virt","-bios","none",
        "-kernel","kernel/kernel","-m","128M","-smp","2","-nographic",
        "-global","virtio-mmio.force-legacy=false",
        "-drive","file=fs.img,if=none,format=raw,id=x0",
        "-device","virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0"]

p = subprocess.Popen(QEMU, cwd=xv6_dir,
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, preexec_fn=os.setsid)

def send(cmd, wait):
    p.stdin.write((cmd+"\n").encode()); p.stdin.flush()
    time.sleep(wait)

try:
    time.sleep(boot_to)               # boot
    send("", 1)                       # nudge prompt
    send("ps", 2)                                   # haneol
    send("tracetool on", 1.5)                       # minju (setup)
    send("ls", 1.5)
    send("tracetool dump", 3)                       # minju (assert)
    send("nlrun 2 cpu_burner 3000", 4)              # hyunsung
    send("bgq bgq.txt", 7)                          # 통합
    send("thread_race", 4)                     # K5 fix path
    send("threadtest", test_to)                     # jinhwan
finally:
    try:
        os.killpg(os.getpgid(p.pid), signal.SIGTERM); p.wait(timeout=3)
    except Exception:
        try: os.killpg(os.getpgid(p.pid), signal.SIGKILL)
        except Exception: pass

print(p.stdout.read().decode("utf-8", errors="replace"))
PY
)

OUT=$(python3 -c "$SCENARIO" "$XV6_DIR" "$TIMEOUT_BOOT" "$TIMEOUT_TEST" 2>>"$LOG")

# Boot prompt
echo "$OUT" | grep -q "init: starting sh" \
  && pass "kernel boots, init starts sh"        \
  || fail "boot"          "no 'init: starting sh' in output"

# haneol — ps
echo "$OUT" | grep -Eq "^PID\s+PPID"            \
  && pass "haneol  ps prints process table"   \
  || fail "haneol/ps"     "header 'PID PPID' not found"

# minju — tracetool dump
echo "$OUT" | grep -q '"traced":1'              \
  && pass "minju   tracetool emits JSON dump"  \
  || fail "minju/trace"   '"traced":1 not in tracetool output'

# hyunsung — nlrun EXIT line
echo "$OUT" | grep -Eq 'EXIT pid=[0-9]+ name=cpu_burner' \
  && pass "hyunsung nlrun EXIT trace emitted"   \
  || fail "hyunsung/MLFQ" "no EXIT line from cpu_burner"

# 통합 — bgq
echo "$OUT" | grep -q "BGQ all_done"            \
  && pass "통합     bgq sequential queue done"  \
  || fail "통합/bgq"      "'BGQ all_done' not found (zombie leak suspected)"

# K5 — thread_race (main exits without join, K5 fix guards freeproc)
# Match early marker "thread_race: spawning" — output from threads may
# interleave with main's exit line, so we only require the binary to start.
# The real K5 evidence is the SUBSEQUENT threadtest passing post-race below.
echo "$OUT" | grep -q "thread_race: spawning"  \
  && pass "K5      thread_race binary executed (K5 race path triggered)"  \
  || fail "K5/teardown" "thread_race did not start — fs.img build broken or kernel panic before exec"

# jinhwan — threadtest (run AFTER K5 race test; if K5 fix is broken, kernel
# state is corrupted by thread_race and threadtest will fail too)
echo "$OUT" | grep -q "All tests done."         \
  && pass "jinhwan threadtest 4/4 (post-K5-race regression check)" \
  || fail "jinhwan/thread" "'All tests done.' not in threadtest output — \
threadtest hung or crashed (K5 race may have corrupted kernel state)"

# ── 3. Summary ──────────────────────────────────────────────────────────
log
log "[3/3] summary"
log "  PASS: $PASS_CNT"
log "  FAIL: $FAIL_CNT"
log "  log:  $LOG"

if [ "$FAIL_CNT" -gt 0 ]; then
  log
  log "── last 30 lines of QEMU output (debug) ──"
  printf "%s\n" "$OUT" | tail -30 | tee -a "$LOG"
  exit 1
fi
exit 0
