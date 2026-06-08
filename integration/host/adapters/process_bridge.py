"""SmartShell NL Bridge — host-side natural-language interface to xv6-riscv.

Pipeline:
    user NL  ->  Solar Pro 3 (intent JSON)  ->  whitelist guard
              ->  xv6 shell command (sent over QEMU stdin)
              ->  parsed output  ->  NL summary back to user

Both the QEMU driver (subprocess.Popen + stdin pipe) and the Solar API call
are fully implemented. When `UPSTAGE_API_KEY` is unset a deterministic
keyword parser (`_offline_parse`) takes over so local dev / demos still work
without network access.
"""

from __future__ import annotations

import json
import os
import re
import signal
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from typing import Optional

# --- Configuration ---------------------------------------------------------

def _load_dotenv() -> None:
    """Load KEY=VALUE lines from a .env file into os.environ.
    Existing env vars take precedence so shell exports still win.

    The real key lives in host/.env (one level up from this adapters/ dir),
    so check there as well as a local adapters/.env. Without the parent path
    a standalone `python process_bridge.py` run would never see the key and
    silently fall back to the tiny offline parser.
    """
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, ".env"),            # adapters/.env (optional override)
        os.path.join(here, "..", ".env"),      # host/.env (where the key lives)
    ]
    for path in candidates:
        if not os.path.exists(path):
            continue
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                k, v = k.strip(), v.strip().strip('"').strip("'")
                os.environ.setdefault(k, v)


_load_dotenv()

XV6_DIR = os.environ.get(
    "XV6_DIR",
    # this file lives at integration/host/adapters/, xv6 is at integration/xv6-riscv
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "xv6-riscv"),
)
SOLAR_API_KEY = os.environ.get("UPSTAGE_API_KEY") or os.environ.get("SOLAR_API_KEY")
SOLAR_BASE_URL = os.environ.get("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
SOLAR_MODEL = os.environ.get("UPSTAGE_MODEL", "solar-pro3")

# Programs the LLM is allowed to spawn. PID-1 (init) is never killable.
# K4 fix: priority_test / trace_test / sysinfo_test were dropped at
# integration (they use removed APIs setpriority/trace). Replaced with the
# integrated tree's actual test programs.
SPAWN_WHITELIST = {"ls", "ps", "cat", "echo", "wc", "grep",
                   "setprio", "tracetool", "threadtest",
                   "nlrun", "wrunner", "rrunner",
                   "cpu_burner", "io_burner", "mixed_burner",
                   "diagprog"}


# --- Intent representation -------------------------------------------------

@dataclass
class Intent:
    type: str                       # PS | SETPRIO | SPAWN | KILL | EXPLAIN | REJECT
    args: dict = field(default_factory=dict)
    reason: str = ""


# --- Solar API ------------------------------------------------------------

SYSTEM_PROMPT = """\
You translate a user's natural-language request into ONE structured intent for
an xv6 RISC-V shell. Reply ONLY as a JSON object with keys:
  type:   one of PS, SETPRIO, SPAWN, KILL, EXPLAIN, REJECT
  args:   object (see below)
  reason: short string (required for REJECT, optional otherwise)

Allowed args by type:
  PS:       {}                                 -> list active processes
  SETPRIO:  {"pid": int, "prio": int 0..2}     -> change MLFQ queue (K4: was 0..20)
                                                  0 = HIGH (interactive)
                                                  1 = MID
                                                  2 = LOW  (CPU-bound)
  SPAWN:    {"cmd": "<program> [args...]"}     -> run a user program
  KILL:     {"pid": int}                       -> kill a process
  EXPLAIN:  {"about": "free text"}             -> answer w/o running anything
  REJECT:   {}                                 -> request is unsafe / out of scope

Runnable programs for SPAWN — use ONLY these names (pick the closest fit;
never invent a program name):
  cpu_burner <N>    CPU-bound loop of N iterations (heavy / compute / batch /
                    "background" jobs)
  io_burner <N>     I/O-bound, N short reads (file / read / disk / IO work)
  mixed_burner <N>  alternating CPU burst + sleep, N iterations (mixed load)
  ls  ps  cat  echo  wc  grep  setprio  tracetool  threadtest  diagprog
  nlrun  wrunner  rrunner

Normalising SPAWN requests (do this BEFORE deciding to REJECT):
  - A vague "run a heavy / long / background / compute job" with no program
    named -> SPAWN cpu_burner with a large N. "some I/O / file work" ->
    io_burner. "a mixed job" -> mixed_burner. Always emit a concrete name
    from the list above, never REJECT just because no program was named.
  - Convert spoken/written numbers to an integer count N:
      "5만" / "오만" / "50k"        -> 50000
      "백만" / "1 million" / "1m"   -> 1000000
      "천번" / "1k"                 -> 1000
    If no count is given, default cpu_burner->1000000, io_burner->50,
    mixed_burner->100000.

Refuse (REJECT) only genuinely unsafe / out-of-scope requests: anything that
targets pid 1, modifies the kernel, or asks for a program not in the list
above. Do NOT reject a normal "run a job" request — map it to cpu_burner/
io_burner/mixed_burner as described.
Output JSON only — no markdown, no commentary.
"""


def parse_nl(text: str) -> Intent:
    """Call Solar Pro 3 to convert NL to an Intent. Falls back to a tiny
    rule-based parser when no API key is configured OR when the Solar call
    fails for any reason (network, HTTP error, malformed JSON, missing keys).
    Without this fallback the whole adapter crashes on a transient 5xx."""
    if not SOLAR_API_KEY:
        return _offline_parse(text)

    try:
        # Lazy import so the file is importable without `requests` installed
        import requests  # type: ignore
        resp = requests.post(
            f"{SOLAR_BASE_URL}/chat/completions",
            headers={"Authorization": f"Bearer {SOLAR_API_KEY}"},
            json={
                "model": SOLAR_MODEL,
                "messages": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": text},
                ],
                "temperature": 0,
                "response_format": {"type": "json_object"},
            },
            timeout=15,
        )
        resp.raise_for_status()
        data = resp.json()["choices"][0]["message"]["content"]
        obj = json.loads(data)
        return Intent(type=obj.get("type", "REJECT"),
                      args=obj.get("args", {}),
                      reason=obj.get("reason", ""))
    except Exception as e:
        # Don't crash the REPL on a transient Solar issue — fall back to the
        # offline keyword parser. The REJECT case still surfaces if the
        # offline parser can't classify.
        print(f"  ⚠  Solar 호출 실패 ({type(e).__name__}: {e}) — 오프라인 파서 사용",
              file=sys.stderr)
        return _offline_parse(text)


def _offline_parse(text: str) -> Intent:
    """Tiny offline parser used when SOLAR_API_KEY is missing."""
    t = text.strip().lower()
    if t in {"ps", "list", "프로세스", "process", "show processes"}:
        return Intent("PS")
    m = re.match(r"(?:set\s*prio|priority|우선순위)\s+(\d+)\s+(\d+)", t)
    if m:
        return Intent("SETPRIO", {"pid": int(m.group(1)), "prio": int(m.group(2))})
    m = re.match(r"kill\s+(\d+)", t)
    if m:
        return Intent("KILL", {"pid": int(m.group(1))})
    m = re.match(r"run\s+(\S+)(.*)$", t)
    if m:
        return Intent("SPAWN", {"cmd": (m.group(1) + m.group(2)).strip()})
    return Intent("REJECT", reason="offline parser could not classify input")


# --- Safety guard ----------------------------------------------------------

def guard(intent: Intent) -> Optional[str]:
    """Return None if intent is allowed, else a rejection reason string."""
    if intent.type == "KILL":
        if intent.args.get("pid") in (0, 1):
            return "refusing to kill pid 1 (init)"
    if intent.type == "SETPRIO":
        # K4: integrated kernel uses hyunsung MLFQ queue levels 0..2,
        # not haneol's original 0..20 priority. setpri(pid, level) rejects
        # anything outside this range, so guard here too.
        prio = intent.args.get("prio")
        if not isinstance(prio, int) or not (0 <= prio <= 2):
            return f"priority must be 0..2 MLFQ queue level (got {prio!r})"
    if intent.type == "SPAWN":
        raw = (intent.args.get("cmd") or "").strip()
        # Block command chaining / injection — a Solar response of
        # "ls\nkill 1" would otherwise pass the whitelist check on parts[0]
        # and then send_to_xv6 would write both lines into xv6 stdin.
        for bad in ("\n", "\r", ";", "&", "|", "`", "$("):
            if bad in raw:
                return f"refusing command with {bad!r} (chain/injection blocked)"
        prog = raw.split()
        if not prog or prog[0] not in SPAWN_WHITELIST:
            return f"program not in whitelist: {prog[0] if prog else '<empty>'}"
    return None


# --- Intent -> xv6 shell command ------------------------------------------

def to_xv6_cmd(intent: Intent) -> str:
    if intent.type == "PS":
        return "ps"
    if intent.type == "SETPRIO":
        return f"setprio {intent.args['pid']} {intent.args['prio']}"
    if intent.type == "SPAWN":
        return intent.args["cmd"]
    if intent.type == "KILL":
        return f"kill {intent.args['pid']}"
    return ""


# --- QEMU driver ----------------------------------------------------------

# Lines we drop entirely when assembling the response — pure scheduler noise.
# `search` (not match) so we drop noise lines even when the kernel print
# was injected in the middle of a user-program output line.
_NOISE_RE = re.compile(r"\[(sched|sleep|wakeup)\]")


class QEMUDriver:
    """Spawn `make qemu` and talk to the xv6 shell over stdio.

    A background reader thread continuously drains QEMU's stdout into a
    rolling buffer. This is necessary because xv6's scheduler prints debug
    lines on every context switch — if nobody reads, the OS pipe buffer
    (~64KB on macOS) fills up in seconds and QEMU dies on its next write.
    """

    # Discard buffered output older than this many bytes (keep memory bounded
    # over long REPL sessions).
    _MAX_BUF = 1 << 20  # 1 MiB

    def __init__(self, xv6_dir: str = XV6_DIR, boot_wait: float = 4.0,
                 boot_timeout: float = 60.0):
        if not os.path.isdir(xv6_dir):
            raise RuntimeError(
                f"xv6 dir not found: {xv6_dir!r}. Set XV6_DIR or run from the "
                f"correct tree (integration/host)."
            )
        # preexec_fn=os.setsid is POSIX-only. On Windows there's no setsid
        # and Popen would AttributeError. Skip process-group setup there;
        # QEMU still terminates correctly via proc.terminate()/kill().
        popen_kwargs = {}
        if sys.platform != "win32" and hasattr(os, "setsid"):
            popen_kwargs["preexec_fn"] = os.setsid
        self.proc = subprocess.Popen(
            ["make", "qemu"],
            cwd=xv6_dir,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            **popen_kwargs,
        )
        self._buf = bytearray()
        self._buf_lock = threading.Lock()
        self._reader = threading.Thread(target=self._drain_forever, daemon=True)
        self._reader.start()
        self._wait_for_boot(boot_wait, boot_timeout)

    def _wait_for_boot(self, min_wait: float, timeout: float) -> None:
        """Block until xv6 has actually booted to a shell prompt, rather than
        sleeping a fixed interval. `make qemu` may first recompile the tree
        (tens of seconds) before QEMU even starts; with a fixed 4s wait the
        first commands get written into the *compiler's* stdin and silently
        lost, so the REPL looks dead ("아무것도 안 떠"). Poll the output buffer
        for the boot banner / shell prompt instead, up to `timeout`."""
        # xv6 prints "init: starting sh" then sh shows "$ ". Either marker
        # means the shell is ready to accept commands.
        markers = (b"init: starting sh", b"$ ")
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError(
                    f"QEMU/make exited during boot (returncode={self.proc.returncode}). "
                    f"Build error or qemu missing? Last output:\n"
                    f"{self._snapshot().decode(errors='replace')[-2000:]}"
                )
            buf = self._snapshot()
            if any(m in buf for m in markers):
                time.sleep(0.5)  # let the prompt settle
                return
            time.sleep(0.2)
        # Timed out waiting for the prompt — fall back to the old fixed wait
        # so a banner-format change doesn't hard-fail, but warn loudly.
        print("  ⚠  부팅 마커를 못 봤습니다 (셸 프롬프트 미검출) — 그래도 진행합니다. "
              "응답이 비면 빌드/부팅 로그를 확인하세요.", file=sys.stderr)
        time.sleep(min_wait)
        if self.proc.poll() is not None:
            raise RuntimeError(
                f"QEMU exited during boot (returncode={self.proc.returncode}). "
                f"Last output:\n{self._snapshot().decode(errors='replace')[-2000:]}"
            )

    # --- background reader -------------------------------------------------

    def _drain_forever(self) -> None:
        # read1() returns whatever bytes are immediately available (up to N)
        # without blocking for the full count. Plain read(N) would block
        # waiting for exactly N bytes and freeze the drain.
        while True:
            try:
                chunk = self.proc.stdout.read1(4096)
            except (ValueError, OSError):
                return
            if not chunk:
                return  # EOF — QEMU exited
            with self._buf_lock:
                self._buf.extend(chunk)
                if len(self._buf) > self._MAX_BUF:
                    # Drop oldest half to keep memory bounded
                    del self._buf[: len(self._buf) // 2]

    def _snapshot(self) -> bytes:
        with self._buf_lock:
            return bytes(self._buf)

    # --- public API --------------------------------------------------------

    def run(self, cmd: str, settle: float = 1.5) -> str:
        """Send a shell command, wait `settle` seconds, return clean stdout
        captured during that window (filtered of scheduler noise)."""
        if self.proc.poll() is not None:
            raise RuntimeError(
                f"QEMU is no longer running (returncode={self.proc.returncode})"
            )

        # Reset buffer so we only capture output produced by this command
        with self._buf_lock:
            self._buf.clear()

        try:
            self.proc.stdin.write((cmd + "\n").encode())
            self.proc.stdin.flush()
        except BrokenPipeError as e:
            raise RuntimeError(
                f"QEMU stdin closed unexpectedly: {e}. "
                f"Was the kernel/fs.img built? Try `make` in xv6-riscv/."
            )

        time.sleep(settle)
        text = self._snapshot().decode("utf-8", errors="replace")
        cleaned = "\n".join(
            line for line in text.splitlines() if not _NOISE_RE.search(line)
        )
        return cleaned

    def shutdown(self) -> None:
        if self.proc.poll() is None:
            posix_killpg = (sys.platform != "win32"
                            and hasattr(os, "killpg")
                            and hasattr(os, "getpgid"))
            try:
                if posix_killpg:
                    os.killpg(os.getpgid(self.proc.pid), signal.SIGTERM)
                else:
                    self.proc.terminate()
                self.proc.wait(timeout=3)
            except (ProcessLookupError, subprocess.TimeoutExpired, OSError):
                try:
                    if posix_killpg:
                        os.killpg(os.getpgid(self.proc.pid), signal.SIGKILL)
                    else:
                        self.proc.kill()
                except (ProcessLookupError, OSError):
                    pass
        # Reader thread exits naturally when stdout EOFs
        self._reader.join(timeout=2)


# --- ps output parser (used to summarize) ---------------------------------

_PS_LINE_RE = re.compile(
    r"^\s*(\d+)\s+(\d+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\S+)\s*$"
)


def parse_ps_output(text: str) -> list[dict]:
    rows = []
    for line in text.splitlines():
        m = _PS_LINE_RE.match(line)
        if not m:
            continue
        rows.append({
            "pid": int(m.group(1)),
            "ppid": int(m.group(2)),
            "state": m.group(3),
            "prio": int(m.group(4)),
            "sz": int(m.group(5)),
            "name": m.group(6),
        })
    return rows


def summarize(intent: Intent, xv6_out: str) -> str:
    if intent.type == "PS":
        rows = parse_ps_output(xv6_out)
        if not rows:
            return "(no processes parsed — raw output below)\n" + xv6_out
        parts = [f"{len(rows)} active process(es):"]
        for r in rows:
            parts.append(f"  pid={r['pid']:>3} {r['state']:<7} "
                         f"prio={r['prio']:>2} name={r['name']}")
        return "\n".join(parts)
    return xv6_out.strip() or "(no output)"


# --- REPL ------------------------------------------------------------------

def repl() -> None:
    print("SmartShell NL Bridge — booting xv6 in QEMU...")
    qemu = QEMUDriver()
    print("Ready. Type natural-language commands. Ctrl-D to exit.\n")
    try:
        while True:
            try:
                nl = input("smart> ").strip()
            except EOFError:
                print()
                break
            if not nl:
                continue

            intent = parse_nl(nl)
            err = guard(intent)
            if intent.type == "REJECT" or err:
                print(f"⚠️  rejected: {err or intent.reason}")
                continue
            if intent.type == "EXPLAIN":
                print(intent.args.get("about", "(nothing to explain)"))
                continue

            cmd = to_xv6_cmd(intent)
            if not cmd:
                print(f"⚠️  no xv6 mapping for intent {intent.type}")
                continue

            print(f"  -> intent: {intent.type} {intent.args}")
            print(f"  -> xv6  : {cmd}")
            out = qemu.run(cmd)
            print(summarize(intent, out))
    finally:
        qemu.shutdown()


if __name__ == "__main__":
    sys.exit(repl())
