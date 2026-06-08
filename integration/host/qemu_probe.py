#!/usr/bin/env python3
"""qemu_probe — deterministic, scriptable driver for the xv6 console.

Boots xv6 under QEMU directly (no `make qemu`, so terminate() kills QEMU
itself, not a surviving child), feeds a list of shell commands, and returns
everything the console printed. Progression is idle-based: after each command
we wait until the console has been quiet for `idle` seconds (so fast commands
like `ps` and slow ones like `usertests` both work) up to a hard `cap`.

Usage as a library:
    from qemu_probe import probe
    out = probe(["ps", "uptime"], idle=1.5, cap=60)

CLI:
    python3 qemu_probe.py ps uptime sysinfo
    python3 qemu_probe.py --idle 6 --cap 600 usertests
"""
from __future__ import annotations
import os
import sys
import time
import threading
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OS_DIR = os.path.join(REPO, "os")

QEMU = [
    "qemu-system-riscv64", "-machine", "virt", "-bios", "none",
    "-kernel", "kernel/kernel", "-m", "128M", "-smp", "3", "-nographic",
    "-global", "virtio-mmio.force-legacy=false",
    "-drive", "file=fs.img,if=none,format=raw,id=x0",
    "-device", "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
]


def probe(commands, idle: float = 1.5, cap: float = 60.0,
          boot_idle: float = 2.5, boot_cap: float = 30.0,
          until=None, until_cap: float = 1200.0) -> str:
    """Boot xv6, run commands, return the full console transcript."""
    # Make sure nothing else holds the fs.img write lock.
    subprocess.run(["pkill", "-9", "-f", "qemu-system-riscv64"],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)

    proc = subprocess.Popen(QEMU, cwd=OS_DIR, stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            bufsize=0)
    out = bytearray()
    last = [time.time()]
    lock = threading.Lock()

    def reader():
        while True:
            b = proc.stdout.read(1)
            if not b:
                break
            with lock:
                out.append(b[0])
                last[0] = time.time()

    threading.Thread(target=reader, daemon=True).start()

    def wait_idle(quiet: float, hard: float):
        deadline = time.time() + hard
        while time.time() < deadline:
            with lock:
                since = time.time() - last[0]
            if since >= quiet:
                return
            time.sleep(0.1)

    def wait_for(substr: str, hard: float, start: int = 0) -> bool:
        """Wait until substr appears in out[start:] (robust string detect)."""
        target = substr.encode()
        deadline = time.time() + hard
        while time.time() < deadline:
            with lock:
                if target in bytes(out[start:]):
                    return True
            time.sleep(0.1)
        return False

    def send(c: str, tries: int = 5):
        """Write a command, retrying ONLY if the console dropped it entirely.

        At an idle shell prompt the only thing that produces output after we
        write is our own keystrokes echoing, so "any new output appeared" is a
        reliable signal the command landed. We resend solely when NOTHING comes
        back (a real dropped-byte window just after boot). Crucially we never
        resend a command that already produced output — that would run side-
        effecting commands (ln, mkdir, rm) twice."""
        for _ in range(tries):
            with lock:
                marker = len(out)
            proc.stdin.write((c + "\n").encode())
            proc.stdin.flush()
            deadline = time.time() + 2.5
            while time.time() < deadline:
                with lock:
                    if len(out) > marker:
                        return True          # command landed (echoed)
                time.sleep(0.05)
            time.sleep(0.3)                  # truly dropped — retry
        return False

    try:
        # Boot is done when the shell prints its first prompt. Idle-from-start
        # is unreliable (QEMU has startup latency before any output).
        wait_for("init: starting sh", boot_cap)
        wait_idle(boot_idle, boot_cap)            # let the first $ settle
        for i, c in enumerate(commands):
            send(c)
            # For a long final command (e.g. usertests), wait for a completion
            # marker instead of idle — slow tests go silent for minutes.
            if until is not None and i == len(commands) - 1:
                hit = False
                for marker in (until if isinstance(until, (list, tuple)) else [until]):
                    if wait_for(marker, until_cap):
                        hit = True
                        break
                if not hit:
                    wait_idle(idle, cap)
            else:
                wait_idle(idle, cap)
    finally:
        try:
            proc.terminate()
            proc.wait(timeout=5)
        except Exception:
            proc.kill()
        subprocess.run(["pkill", "-9", "-f", "qemu-system-riscv64"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    return bytes(out).decode(errors="replace")


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("commands", nargs="+")
    ap.add_argument("--idle", type=float, default=1.5)
    ap.add_argument("--cap", type=float, default=60.0)
    ap.add_argument("--until", default=None,
                    help="comma-separated completion markers for the last command")
    ap.add_argument("--until-cap", type=float, default=1200.0)
    args = ap.parse_args()
    until = args.until.split(",") if args.until else None
    print(probe(args.commands, idle=args.idle, cap=args.cap,
                until=until, until_cap=args.until_cap))


if __name__ == "__main__":
    main()
