#!/usr/bin/env python3
"""make_chat_gif.py — render a real `--mode process` session transcript into an
animated terminal GIF (typed prompt + revealed [xv6 응답]).

Input: a transcript file captured by piping commands into
    python nl_shell.py --mode process
plus the ordered list of natural-language inputs that produced it (the piped
stdin is NOT echoed, so we re-supply it here to "type" each command).

Output: an animated GIF. Korean lines render with Malgun Gothic, ASCII lines
(the ps table) with Consolas for column alignment.

Usage:
    python make_chat_gif.py <transcript.txt> <out.gif>
"""
import sys
import re
from PIL import Image, ImageDraw, ImageFont

# Ordered NL inputs that produced the transcript (kept in sync with the capture).
INPUTS = [
    "show processes",
    "run cpu_burner 50000",
    "2번 프로세스 우선순위 가장 낮게 내려줘",
    "show processes",
]

# ── look ────────────────────────────────────────────────────────────────
W, H = 940, 560
BG = (13, 17, 23)          # #0d1117
FG = (201, 209, 217)       # default text
PROMPT = (88, 166, 255)    # '>' prompt, blue
USER = (224, 224, 224)     # typed command
RESP = (63, 185, 80)       # [xv6 응답] green
DIM = (139, 148, 158)      # header / meta
PAD = 24
LINE_H = 28
FONT_SZ = 19

MONO = "/mnt/c/Windows/Fonts/consola.ttf"
KR = "/mnt/c/Windows/Fonts/malgun.ttf"
f_mono = ImageFont.truetype(MONO, FONT_SZ)
f_kr = ImageFont.truetype(KR, FONT_SZ)

_CJK = re.compile(r"[가-힣㄰-㆏]")


def font_for(s):
    return f_kr if _CJK.search(s) else f_mono


# ── parse transcript into [(input, [response lines]), ...] ───────────────
def parse(path):
    raw = open(path, encoding="utf-8", errors="replace").read().splitlines()
    blocks, cur = [], None
    for ln in raw:
        if ln.startswith("> "):
            if cur is not None:
                blocks.append(cur)
            rest = ln[2:]
            cur = [rest] if rest.strip() else []
        elif cur is not None:
            cur.append(ln)
    if cur is not None:
        blocks.append(cur)
    # blocks[i] is the response (incl. the "[xv6 응답] ..." first line) for INPUTS[i]
    steps = []
    for i, inp in enumerate(INPUTS):
        resp = blocks[i] if i < len(blocks) else ["(no response captured)"]
        resp = [r for r in resp if r != "$"]            # drop bare xv6 prompt echoes
        steps.append((inp, resp))
    return steps


# ── frame model ──────────────────────────────────────────────────────────
# Each rendered line is (text, color, kind) where kind picks the font.
HEADER = [
    ("DNDN Project — 자연어로 움직이는 xv6  (Solar Pro 3)", DIM),
    ("$ python nl_shell.py --mode process", DIM),
    ("[nl_shell] 준비 완료. 자연어를 입력하세요 (Ctrl-D 종료).", DIM),
    ("", FG),
]


def draw(lines):
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    # window chrome dots
    for i, c in enumerate([(255, 95, 86), (255, 189, 46), (39, 201, 63)]):
        d.ellipse([PAD + i * 22, 14, PAD + i * 22 + 13, 27], fill=c)
    visible = lines[-15:]
    y = 44
    for text, color in visible:
        d.text((PAD, y), text, font=font_for(text), fill=color)
        y += LINE_H
    return img


def main():
    transcript, out = sys.argv[1], sys.argv[2]
    steps = parse(transcript)

    frames, durations = [], []
    committed = [(t, c) for t, c in HEADER]

    def snap(extra, ms):
        frames.append(draw(committed + extra))
        durations.append(ms)

    snap([], 700)
    for inp, resp in steps:
        # type the command after the prompt, a few chars per frame
        typed = ""
        step = max(1, len(inp) // 12)
        i = 0
        while i < len(inp):
            i = min(len(inp), i + step)
            typed = inp[:i]
            snap([("> " + typed, USER)], 55)
        snap([("> " + inp, USER)], 450)          # pause on full command
        committed.append(("> " + inp, PROMPT))
        # reveal response line by line
        shown = []
        for r in resp:
            color = RESP if "[xv6 응답]" in r else FG
            shown.append((r, color))
            snap(shown, 230)
        for r, color in shown:
            committed.append((r, color))
        committed.append(("", FG))
        snap([], 500)
    # hold final frame
    snap([], 1500)

    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, optimize=True, disposal=2)
    print(f"wrote {out}  ({len(frames)} frames)")


if __name__ == "__main__":
    main()
