#!/usr/bin/env python3
"""dev_chat.py — Free-form chat with Solar Pro 3.

⚠️  NOT part of the evaluation path.
⚠️  NOT part of the integration demo.
⚠️  NOT cited in the report or slides.

This is a developer convenience: a plain REPL over the same Solar Pro 3 key
the project already uses, with no system prompt and no protocol. Use it for
quick debugging, prompt-tuning experiments, sanity-checking Solar's behavior
on a phrasing before wiring it into nl_shell.py / supervisor.py — same role
as `curl` or `psql` in a typical project: a dev tool, not the product.

The project's core principles (LLM never enters kernel, output reduced to
2-bit integer at syscall boundary, OS auto-corrects bad hints) are
*architectural* — they constrain `nl_shell.py` and the kernel. They do NOT
forbid using the same key for an unrelated developer REPL. The "no thin
wrapper" rule applies to what we *present* as the project, not to what we
allow on our own workstation.

Usage:
  python3 dev_chat.py                       # REPL
  python3 dev_chat.py --once "your message"
  python3 dev_chat.py --reset               # ignore session history file
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

try:
    from dotenv import load_dotenv
    load_dotenv(Path(__file__).resolve().parent / ".env")
except ImportError:
    pass

try:
    from openai import OpenAI
except ImportError:
    print("ERROR: openai SDK not installed. Run: pip install openai python-dotenv",
          file=sys.stderr)
    sys.exit(1)


API_KEY = os.getenv("UPSTAGE_API_KEY") or os.getenv("SOLAR_API_KEY")
BASE_URL = os.getenv("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
MODEL   = os.getenv("UPSTAGE_MODEL", "solar-pro3")
EFFORT  = os.getenv("UPSTAGE_REASONING_EFFORT", "low")


MAX_HISTORY_TURNS = 20   # cap rolling history (user+assistant counted as 2 entries)


def chat_once(client: OpenAI, history: list[dict], user_msg: str) -> str:
    history.append({"role": "user", "content": user_msg})
    # Trim oldest turns to keep token budget bounded. Each turn is 2 entries.
    max_entries = MAX_HISTORY_TURNS * 2
    if len(history) > max_entries:
        del history[: len(history) - max_entries]
    resp = client.chat.completions.create(
        model=MODEL,
        messages=history,
        temperature=0.7,
        max_tokens=1200,
        extra_body={"reasoning_effort": EFFORT},
    )
    reply = resp.choices[0].message.content or ""
    history.append({"role": "assistant", "content": reply})
    return reply


def main():
    ap = argparse.ArgumentParser(description="dev-only Solar chat REPL")
    ap.add_argument("--once", "-1", help="send one message and exit")
    args = ap.parse_args()

    if not API_KEY:
        print("ERROR: UPSTAGE_API_KEY not set (check .env)", file=sys.stderr)
        return 1

    client = OpenAI(api_key=API_KEY, base_url=BASE_URL)
    history: list[dict] = []

    if args.once:
        print(chat_once(client, history, args.once))
        return 0

    print(f"dev_chat — Solar {MODEL}.  /reset to clear history, exit to quit.")
    while True:
        try:
            msg = input("you> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not msg:
            continue
        if msg.lower() in {"exit", "quit", ":q"}:
            break
        if msg == "/reset":
            history.clear()
            print("(history cleared)")
            continue
        try:
            reply = chat_once(client, history, msg)
        except Exception as e:
            print(f"[error] {type(e).__name__}: {e}", file=sys.stderr)
            continue
        print(f"solar> {reply}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
