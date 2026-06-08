#!/usr/bin/env python3
"""nlbridge — make xv6 understand plain language from *inside* its own console.

This is the live-execution glue (the "QEMU driver" item in CLAUDE.md). It wraps
`make qemu`, mirrors the xv6 console to your terminal, and turns plain language
typed at the xv6 prompt into real commands. There are two ways in:

  (A) auto-fallback — just type plain language, no prefix:
        $ 무거운 프로세스 정리해줘
        exec 무거운 failed            <- xv6 sh: not a real command
        [bridge] (자연어로 해석) ...   <- bridge re-interprets the typed line
        [bridge] -> killheavy         <- translated + injected, xv6 runs it
      Real commands (ps, ls, kill 7) are NOT intercepted — they run normally and
      fast. Only a line xv6 fails to exec (its first word isn't a program) is
      treated as natural language.

  (B) explicit `ask` — the in-guest `ask` program emits a sentinel the bridge
      catches directly (handy when the request starts with a real command word):
        $ ask 7번 프로세스 꺼줘   ->   @@NL 7번 프로세스 꺼줘   ->   kill 7

Both paths run: translate (executor.classify, Solar/local) -> guard
(executor.SafetyGuard) -> inject the command into the SAME console.

The LLM never enters the kernel — only the tiny translated command crosses the
console (and thus the syscall boundary). The host<->guest console round-trip is
itself an IPC channel between two processes (qemu/guest and this bridge).

Run:
    cd host && python3 nlos.py            (recommended launcher; picks backend)
    cd host && python3 nlbridge.py        (bridge only; backend via env)
Env:
    UPSTAGE_API_KEY / UPSTAGE_BASE_URL / UPSTAGE_MODEL  -> pick the LLM backend
    NLBRIDGE_OFFLINE=1   -> skip the LLM, use the built-in rule fallback
    NLBRIDGE_NOAUTO=1    -> disable auto-fallback; require the `ask` prefix
    NLBRIDGE_CONFIRM=1   -> never auto-run destructive ops; print them and let
                            the user confirm by typing the command themselves
"""
from __future__ import annotations
import os
import re
import sys
import shutil
import threading
import subprocess
import collections

import executor  # build_command / SafetyGuard / classify  (same dir)

try:                                  # POSIX-only; absent on Windows native
    import termios
    import tty
    _HAVE_TERMIOS = True
except ImportError:
    _HAVE_TERMIOS = False

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OS_DIR = os.path.join(REPO, "os")
SENTINEL = "@@NL "


def load_env() -> None:
    """Load KEY=VALUE pairs from host/.env and repo .env without overriding
    variables already set in the environment. No python-dotenv dependency."""
    for path in (os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env"),
                 os.path.join(REPO, ".env")):
        if not os.path.isfile(path):
            continue
        try:
            with open(path, encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#") or "=" not in line:
                        continue
                    k, v = line.split("=", 1)
                    os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))
        except Exception:
            pass


def extract_request(line: str):
    """Pull the NL request out of a console line, or None if it is not a
    sentinel line. Reacts only to the `ask` program's own output, never to the
    echoed command the user typed (which has no @@NL before the sentinel)."""
    idx = line.find(SENTINEL)
    if idx != -1 and "ask " not in line[:idx]:
        text = line[idx + len(SENTINEL):].strip()
        return text or None
    return None


def preflight() -> list[str]:
    """Return a list of fatal problems with the xv6 build environment."""
    issues = []
    if shutil.which("make") is None:
        issues.append("`make` not found")
    if shutil.which("qemu-system-riscv64") is None:
        issues.append("`qemu-system-riscv64` not found")
    if not any(shutil.which(g) for g in
               ("riscv64-unknown-elf-gcc", "riscv64-elf-gcc", "riscv64-linux-gnu-gcc")):
        issues.append("RISC-V gcc (riscv64-*-elf-gcc) not found")
    return issues


def ensure_built() -> bool:
    """Build kernel + fs.img, letting make decide what is stale.

    We always invoke make (naming both targets explicitly — plain `make` only
    builds the kernel, and fs.img pulls in all UPROGS incl. `ask`/`nlrun`). make
    is idempotent: a fast no-op when nothing changed, a rebuild when a source or
    user program changed — so editing the kernel and just re-running `nlos.py`
    always picks the change up (no stale-image trap). Building here, before we
    launch `make qemu`, also keeps the xv6 console clean of compiler output."""
    print("[bridge] building xv6 (make) ...", flush=True)
    return subprocess.run(["make", "kernel/kernel", "fs.img"], cwd=OS_DIR).returncode == 0


# --------------------------------------------------------------------------
# Offline fallback: a handful of rules so the live demo works with no API key.
# The real translator is executor.classify() (the fine-tuned / Solar model);
# this only covers the obvious phrasings for when no backend is reachable.
# --------------------------------------------------------------------------
def rule_fallback(text: str) -> dict:
    t = text.lower().strip()
    # kill by pid — handle both word orders: "7번 꺼줘" and "kill 7".
    kill_word = r"(?:꺼|죽|종료|kill|terminate|stop)"
    m = (re.search(rf"(\d+)\s*(?:번|pid)?\s*(?:프로세스)?\D*{kill_word}", t)
         or re.search(rf"{kill_word}\D*?(\d+)", t))
    if m:
        return {"cmd": "kill", "args": [m.group(1)], "queue_hint": 0, "reason": "kill by pid"}
    # zombie/reap is more specific than the generic "정리" (cleanup), so check
    # it first — otherwise "좀비 정리해줘" wrongly matches killheavy.
    if any(k in t for k in ("좀비", "zombie", "reap", "수확", "거둬")):
        return {"cmd": "reap", "args": [], "queue_hint": 0, "reason": "reap zombies"}
    if any(k in t for k in ("정리", "무거운", "heavy", "killheavy")):
        return {"cmd": "killheavy", "args": [], "queue_hint": 0, "reason": "kill heavy procs"}
    if any(k in t for k in ("프로세스 목록", "프로세스 보여", "ps", "process list", "목록")):
        return {"cmd": "ps", "args": [], "queue_hint": 0, "reason": "list procs"}
    if any(k in t for k in ("업타임", "uptime", "얼마나")):
        return {"cmd": "uptime", "args": [], "queue_hint": 0, "reason": "uptime"}
    if any(k in t for k in ("시스템 정보", "sysinfo", "메모리")):
        return {"cmd": "sysinfo", "args": [], "queue_hint": 0, "reason": "sysinfo"}
    m = re.search(r"(killall|모두|전부).*?([a-z_]+_burner|[a-z_]+)", t)
    if "killall" in t or "모두" in t or "전부" in t:
        name = m.group(2) if m else "cpu_burner"
        return {"cmd": "killall", "args": [name], "queue_hint": 0, "reason": "kill all by name"}
    if any(k in t for k in ("cpu", "계산", "무거운 작업")):
        return {"cmd": "cpu_burner", "args": ["2000000"], "queue_hint": 2, "reason": "cpu workload, low prio"}
    return {"cmd": "reject", "args": [], "queue_hint": 0, "reason": f"no rule for: {text!r}"}


def translate(text: str) -> dict:
    """NL -> spec. Prefer the LLM backend; fall back to rules if unavailable."""
    if os.environ.get("NLBRIDGE_OFFLINE") == "1":
        return rule_fallback(text)
    try:
        return executor.classify(text, timeout=20.0)
    except Exception as e:                       # no key / no network / bad json
        print(f"\r\n[bridge] LLM 호출 실패 ({type(e).__name__}); 규칙 폴백 사용", flush=True)
        return rule_fallback(text)


def chat_reply(text: str):
    """Conversational answer for input that is NOT a runnable command — the
    model *talks* (general NL). Returns None when no model backend is reachable
    (offline rules), so the caller can fall back to a plain message."""
    if os.environ.get("NLBRIDGE_OFFLINE") == "1":
        return None
    try:
        return executor.chat(text, timeout=30.0)
    except Exception:
        return None


def main() -> int:
    load_env()
    guard = executor.SafetyGuard()
    need_confirm = os.environ.get("NLBRIDGE_CONFIRM") == "1"
    auto_fallback = os.environ.get("NLBRIDGE_NOAUTO") != "1"

    issues = preflight()
    if issues:
        print("[bridge] cannot launch xv6 — missing tools:")
        for i in issues:
            print(f"  - {i}")
        print("\n  macOS:  brew install riscv64-elf-gcc qemu")
        print("  Linux:  apt install gcc-riscv64-unknown-elf qemu-system-misc")
        print("  Windows native cannot build xv6 — use WSL2 (Ubuntu) and run there.")
        print("\n  (Host translation/guard still work without xv6:"
              " python3 executor.py --selftest)")
        return 2
    if not ensure_built():
        print("[bridge] build failed — fix the errors above and retry.")
        return 2

    print(f"[bridge] launching xv6 (make qemu) in {OS_DIR} ...")
    print("[bridge] just type plain language — e.g.  무거운 프로세스 정리해줘  /  7번 꺼줘")
    print("[bridge] real commands (ps, ls, kill 7) still run normally; `ask ...` also works.")
    if not auto_fallback:
        print("[bridge] (auto-fallback disabled via NLBRIDGE_NOAUTO; use the `ask` prefix)")
    print("[bridge] (Ctrl-A X to quit qemu)\n")

    proc = subprocess.Popen(
        ["make", "qemu"], cwd=OS_DIR,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, bufsize=0,
    )
    write_lock = threading.Lock()
    last_injected = [None]     # last command WE injected (never re-translate it)

    def inject(line: str):
        last_injected[0] = line
        with write_lock:
            # Ctrl-U (0x15) first — xv6's console "kill line" (console.c). It
            # clears any half-typed line the user started while our async
            # translation was in flight, so the injected command lands on a
            # clean prompt instead of merging with their partial input.
            proc.stdin.write(b"\x15" + (line + "\n").encode())
            proc.stdin.flush()

    def handle(text: str, src: str = "ask"):
        if src == "auto":
            # The user typed bare language; xv6 printed "exec ... failed" just
            # above. Make clear that was expected and we're interpreting it.
            print(f"\r\n[bridge] (자연어로 해석) “{text}”", flush=True)
        spec = translate(text)
        d = guard.evaluate(spec)
        if d.verdict == "reject":
            if spec.get("cmd") == "reject":      # MODEL judged it non-actionable
                ans = chat_reply(text)           # -> let the model talk
                print(f"\r\n[assistant] {ans}" if ans
                      else f"\r\n[bridge] REJECT: {d.reason}", flush=True)
            else:                                # SAFETY reject (guard) — keep clear
                print(f"\r\n[bridge] REJECT: {d.reason}", flush=True)
            return
        if d.command is None:                    # explain — answer conversationally
            ans = chat_reply(text)
            print(f"\r\n[assistant] {ans}" if ans
                  else f"\r\n[bridge] (no OS action) {d.reason}", flush=True)
            return
        if d.verdict == "confirm":
            if need_confirm:
                # Race-free gate: don't auto-run. The user confirms by typing the
                # command themselves (no contention with feed_stdin over stdin).
                print(f"\r\n[bridge] ⚠ 확인 필요 — 직접 `{d.command}` 입력 시 실행 "
                      f"(가드: {d.reason})", flush=True)
                return
            print(f"\r\n[bridge] ⚠ destructive(auto): {d.command}  ({d.reason})", flush=True)
        print(f"\r\n[bridge] -> {d.command}", flush=True)
        inject(d.command)

    # Thread: forward your keystrokes straight into qemu. os.read gives us
    # per-keystroke bytes in cbreak mode, per-line in cooked mode, and chunks
    # from a pipe (automated tests) — all forwarded verbatim.
    stdin_fd = sys.stdin.fileno()

    def feed_stdin():
        # Forward keystrokes verbatim. We do NOT track input here — the command
        # to (maybe) re-interpret is read back from xv6's own echo in the main
        # loop, which reflects exactly what the shell received (immune to async
        # injection, dropped bytes, and line-editing).
        try:
            while True:
                data = os.read(stdin_fd, 1024)
                if not data:
                    break
                with write_lock:
                    proc.stdin.write(data)
                    proc.stdin.flush()
        except Exception:
            pass

    # Put the real terminal in cbreak so xv6 sees keys immediately (backspace,
    # Ctrl-P, etc.). Skipped for pipes/Windows. Restored on exit no matter what.
    use_raw = _HAVE_TERMIOS and sys.stdin.isatty()
    old_attr = termios.tcgetattr(stdin_fd) if use_raw else None
    if use_raw:
        tty.setcbreak(stdin_fd)

    threading.Thread(target=feed_stdin, daemon=True).start()

    # Main loop: mirror qemu output, watch for the @@NL sentinel and for the
    # shell's "exec X failed" (a typed line that wasn't a real command).
    buf = bytearray()
    # Recent completed console lines. We read the failed command back from
    # xv6's OWN echo (not tracked keystrokes), which reflects exactly what the
    # shell received. A small buffer (not just the previous line) is needed
    # because xv6 echoes pasted/rapid input lines before it processes them, so
    # the matching "$ <cmd>" echo may be a few lines above the "exec X failed".
    recent = collections.deque(maxlen=16)
    out = sys.stdout.buffer
    try:
        while True:
            b = proc.stdout.read(1)
            if not b:
                break
            out.write(b)
            out.flush()
            if b == b"\n":
                line = buf.decode(errors="replace")
                buf.clear()
                stripped = line.strip()
                text = extract_request(line)              # explicit `ask` path
                if text:
                    threading.Thread(target=handle, args=(text,), daemon=True).start()
                elif auto_fallback:
                    # Allow leading "$ " prompts: rapid input can interleave so
                    # the failure prints as "$ exec X failed". The precise guard
                    # below still rejects "echo exec…"/"nlrun: exec…" (the part
                    # before "exec" must be only prompts).
                    m = re.match(r"(?:\$\s*)*exec (.+) failed$", stripped)
                    if m:
                        prog = m.group(1)
                        # Find the echoed command whose FIRST token is the failed
                        # program name (most-recent first). Ignores "exec…failed"
                        # emitted by a running program and our own injections.
                        cmd = None
                        for cand in reversed(recent):
                            c = cand.lstrip("$ ").strip()
                            toks = c.split()
                            if toks and toks[0] == prog and c != last_injected[0]:
                                cmd = c
                                break
                        if cmd:
                            threading.Thread(target=handle, args=(cmd, "auto"),
                                             daemon=True).start()
                recent.append(stripped)
            else:
                buf += b
    except KeyboardInterrupt:
        pass
    finally:
        if old_attr is not None:
            termios.tcsetattr(stdin_fd, termios.TCSADRAIN, old_attr)
        if proc.poll() is None:
            proc.terminate()

    return proc.wait()


if __name__ == "__main__":
    raise SystemExit(main())
