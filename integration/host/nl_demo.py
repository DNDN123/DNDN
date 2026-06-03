#!/usr/bin/env python3
"""nl_demo.py — terminal showcase: natural language -> queue level -> live MLFQ.

The terminal twin of integration/DEMO.html. It reuses the REAL translation
pipeline (nl_shell.translate: cache -> Solar Pro 3 -> heuristic fallback) and
animates the 3-level MLFQ with the REAL kernel constants
(slice {2,4,8} ticks, boost every 100 ticks — see kernel/proc.c).

What it shows, in one screen:
  1. you type plain language
  2. the LLM (or offline heuristic) reduces it to a 2-bit queue level {0,1,2}
  3. that job is injected into a 3-level MLFQ alongside a CPU hog and an I/O task
  4. the CPU hog auto-demotes HIGH->LOW (self-correction); the I/O task keeps
     its level on block; every 100 ticks a boost resets everyone to HIGH.

The LLM never enters the kernel — it only nudges the *initial* queue; standard
MLFQ rules take over and fix any bad hint within a few ticks.

Usage:
    python3 nl_demo.py                      # interactive REPL
    python3 nl_demo.py --once "밤새 돌려도 되는 통계 집계"
    python3 nl_demo.py --once "make it snappy" --ticks 120 --speed 90
    python3 nl_demo.py --no-cache --once "heavy batch job"   # force live Solar
"""
import argparse
import os
import sys
import time

# Reuse the real translation pipeline from the host entry point.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nl_shell import translate  # noqa: E402

# ── Real kernel constants (kernel/proc.c) ──────────────────────────────
SLICE = [2, 4, 8]          # mlfq_slice_limit — ticks per level
BOOST = 100                # MLFQ_BOOST_INTERVAL — full reset to HIGH
IO_BLOCK = 3               # ticks an I/O task sleeps after a run (yields CPU)
QNAME = ["HIGH", "MID", "LOW"]

# ── ANSI ───────────────────────────────────────────────────────────────
RESET = "\033[0m"
DIM = "\033[2m"
BOLD = "\033[1m"
INV = "\033[7m"
CYAN = "\033[38;5;39m"
PURPLE = "\033[38;5;141m"
GREEN = "\033[38;5;42m"
YELLOW = "\033[38;5;220m"
RED = "\033[38;5;203m"
GREY = "\033[38;5;245m"
QCOLOR = [GREEN, YELLOW, RED]
CLEAR = "\033[H\033[J"


def kind_of(cmd):
    # echo/cat/io_burner behave like interactive/IO procs (keep level on block);
    # everything else (cpu_burner, ...) is CPU-bound and demotes.
    return "io" if cmd in ("echo", "cat", "io_burner") else "cpu"


def base_procs():
    return [
        {"id": "cpu_hog", "name": "cpu_hog", "kind": "cpu", "lvl": 0, "used": 0, "blk": 0, "user": False},
        {"id": "io_task", "name": "io_task", "kind": "io",  "lvl": 0, "used": 0, "blk": 0, "user": False},
    ]


def render(procs, tick, running):
    out = [CLEAR]
    out.append(f"{BOLD}{CYAN}  DNDN Project  —  말로 움직이는 OS 스케줄러{RESET}"
               f"{DIM}   (LLM은 큐 힌트만, 결정은 MLFQ){RESET}\n")
    run_name = next((p["name"] for p in procs if p["id"] == running), "—")
    out.append(f"  tick {BOLD}{tick:>3}{RESET}   "
               f"next boost in {BOLD}{BOOST - (tick % BOOST):>3}{RESET}   "
               f"running {BOLD}{run_name}{RESET}\n")
    out.append(f"  {GREY}{'─' * 56}{RESET}\n")
    for lvl in range(3):
        head = f"  {QCOLOR[lvl]}●{RESET} {QCOLOR[lvl]}{QNAME[lvl]:<4}{RESET} {DIM}slice {SLICE[lvl]}{RESET} {GREY}│{RESET} "
        cells = []
        for p in procs:
            if p["lvl"] != lvl:
                continue
            if p["blk"] > 0:
                cells.append(f"{DIM}{p['name']}(sleep {p['blk']}){RESET}")
                continue
            tag = (PURPLE if p["user"] else "") + p["name"] + RESET
            meta = f"{DIM}[{p['kind']} {p['used']}/{SLICE[p['lvl']]}]{RESET}"
            cell = f"{tag}{meta}"
            if p["id"] == running:
                cell = f"{INV} {p['name']} {RESET}{meta}"
            cells.append(cell)
        out.append(head + "  ".join(cells) + "\n")
    out.append(f"  {GREY}{'─' * 56}{RESET}\n")
    out.append(f"  {DIM}보라=내가 투입한 작업 · cpu_hog는 HIGH→LOW 자동강등(self-correction)"
               f" · io_task는 레벨 유지 · 100tick boost{RESET}\n")
    sys.stdout.write("".join(out))
    sys.stdout.flush()


def simulate(spec, ticks, speed_ms):
    procs = [{"id": "USER", "name": spec["cmd"], "kind": kind_of(spec["cmd"]),
              "lvl": spec["queue_hint"], "used": 0, "blk": 0, "user": True}]
    procs += base_procs()
    rr = [0, 0, 0]
    delay = max(0.0, speed_ms / 1000.0)
    try:
        for tick in range(1, ticks + 1):
            # I/O sleepers wake up over time (blocked procs aren't schedulable).
            for q in procs:
                if q["blk"] > 0:
                    q["blk"] -= 1
            # Pick highest non-empty level among RUNNABLE (non-blocked) procs.
            runnable = [p for p in procs if p["blk"] == 0]
            lvl = next((l for l in range(3) if any(p["lvl"] == l for p in runnable)), -1)
            running_id = None
            if lvl >= 0:
                in_lvl = [p for p in runnable if p["lvl"] == lvl]
                rr[lvl] %= len(in_lvl)
                p = in_lvl[rr[lvl]]
                running_id = p["id"]
                p["used"] += 1
                if p["kind"] == "io":
                    # I/O block: keep level (no demote), reset slice, go to sleep.
                    p["used"] = 0
                    p["blk"] = IO_BLOCK
                    rr[lvl] = (rr[lvl] + 1) % len(in_lvl)
                else:
                    if p["used"] >= SLICE[p["lvl"]]:   # slice spent -> demote
                        p["used"] = 0
                        if p["lvl"] < 2:
                            p["lvl"] += 1
                        rr[lvl] = 0
            if tick % BOOST == 0:                 # periodic boost -> all HIGH
                for q in procs:
                    q["lvl"] = 0
                    q["used"] = 0
                rr = [0, 0, 0]
            render(procs, tick, running_id)
            time.sleep(delay)
    except KeyboardInterrupt:
        pass
    print(f"\n  {GREEN}✓ {ticks} ticks 완료{RESET} — "
          f"{DIM}cpu_hog는 LOW로 가라앉고, io_task는 위에 남고, boost가 전부 끌어올렸습니다.{RESET}\n")


def show_translation(text, spec):
    src = spec.get("_source", "?")
    src_label = {"solar": f"{CYAN}Solar Pro 3 (LLM){RESET}",
                 "cache": f"{CYAN}Solar (cache){RESET}",
                 "heuristic": f"{GREY}휴리스틱 (오프라인){RESET}"}.get(src, src)
    q = spec["queue_hint"]
    badge = f"{QCOLOR[q]}{BOLD}{q} · {QNAME[q]}{RESET}"
    args = " ".join(str(a) for a in spec["args"])
    print()
    print(f"  {DIM}입력{RESET}      {text}")
    print(f"  {DIM}번역 출처{RESET}  {src_label}")
    print(f"  {DIM}큐 레벨{RESET}    {badge}")
    print(f"  {DIM}프로그램{RESET}   {spec['cmd']} {args}")
    print(f"  {DIM}이유{RESET}      {spec.get('reason', '')}")
    print(f"  {DIM}xv6 명령{RESET}   {CYAN}$ nlrun {q} {spec['cmd']} {args}{RESET}")
    print()
    try:
        input(f"  {DIM}[Enter] 스케줄러에 투입 ▶{RESET} ")
    except (EOFError, KeyboardInterrupt):
        print()


def run_one(text, args):
    spec = translate(text, use_cache=not args.no_cache)
    show_translation(text, spec)
    simulate(spec, args.ticks, args.speed)


def main():
    ap = argparse.ArgumentParser(description="Terminal NL -> MLFQ showcase")
    ap.add_argument("--once", help="single request, animate, then exit")
    ap.add_argument("--ticks", type=int, default=110, help="ticks to simulate (default 110)")
    ap.add_argument("--speed", type=int, default=110, help="ms per tick (default 110)")
    ap.add_argument("--no-cache", action="store_true", help="force live Solar (skip cache)")
    args = ap.parse_args()

    if args.once:
        run_one(args.once, args)
        return

    print(f"{BOLD}{CYAN}DNDN Project 터미널 데모{RESET} — 자연어를 입력하면 큐 레벨로 번역 후 MLFQ 시뮬레이션.")
    print(f"{DIM}예: '밤새 돌려도 되는 통계 집계'  /  '사용자가 기다리는 작업 빠르게'   ('exit' 종료){RESET}\n")
    while True:
        try:
            text = input("nl> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if text in ("exit", "quit", ""):
            if text == "":
                continue
            break
        run_one(text, args)


if __name__ == "__main__":
    main()
