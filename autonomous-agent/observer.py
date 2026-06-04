"""Observer — periodically read xv6 process snapshot via `getstats` syscall.

Communicates with QEMU stdin/stdout. Parses the `getstats` output emitted
by the kernel and returns a Snapshot object.

Snapshot format (from os kernel):
    GETSTATS pid=2 prio=1 run=180 io=0 wait_age=140 name=cpu_burner
    GETSTATS pid=3 prio=0 run=20  io=15 wait_age=2  name=io_burner
    ...
    GETSTATS END n=N
"""
from __future__ import annotations
import re
import time
from dataclasses import dataclass, field
from typing import List, Optional

# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------
@dataclass
class ProcStat:
    pid: int
    priority: int       # 0=HIGH, 1=MID, 2=LOW
    run_ticks: int      # accumulated CPU time
    io_blocks: int      # I/O sleep count
    wait_age: int       # ticks since last scheduled (high = starving)
    name: str

    @property
    def is_starving(self) -> bool:
        return self.wait_age > 100 and self.priority >= 1

    @property
    def is_runaway(self) -> bool:
        return self.run_ticks > 500 and self.io_blocks < 5


@dataclass
class Snapshot:
    ts: float                           # wall clock at capture
    procs: List[ProcStat] = field(default_factory=list)
    raw: str = ""                       # raw output for debug

    def by_pid(self, pid: int) -> Optional[ProcStat]:
        for p in self.procs:
            if p.pid == pid:
                return p
        return None


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------
_PROC_RE = re.compile(
    r"GETSTATS pid=(\d+)\s+prio=(\d+)\s+run=(\d+)\s+io=(\d+)\s+"
    r"wait_age=(\d+)\s+name=(\S+)"
)
_END_RE = re.compile(r"GETSTATS END n=(\d+)")


def parse_snapshot(raw_text: str) -> Snapshot:
    snap = Snapshot(ts=time.time(), raw=raw_text)
    for line in raw_text.splitlines():
        m = _PROC_RE.search(line)
        if m:
            snap.procs.append(ProcStat(
                pid=int(m.group(1)),
                priority=int(m.group(2)),
                run_ticks=int(m.group(3)),
                io_blocks=int(m.group(4)),
                wait_age=int(m.group(5)),
                name=m.group(6),
            ))
    return snap


# ---------------------------------------------------------------------------
# QEMU driver (reused from host integration)
# ---------------------------------------------------------------------------
import subprocess, threading, queue, os

class QemuDriver:
    """Simple QEMU stdio pipe driver. Reuses integration host's approach."""

    def __init__(self, xv6_dir: str, boot_wait: float = 6.0):
        self.xv6_dir = xv6_dir
        self.proc: Optional[subprocess.Popen] = None
        self.out_q: queue.Queue = queue.Queue()
        self.boot_wait = boot_wait

    def start(self):
        self.proc = subprocess.Popen(
            ["make", "qemu"],
            cwd=self.xv6_dir,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
            preexec_fn=os.setsid,
        )
        threading.Thread(target=self._reader_loop, daemon=True).start()
        time.sleep(self.boot_wait)

    def _reader_loop(self):
        assert self.proc and self.proc.stdout
        buf = []
        while True:
            ch = self.proc.stdout.read(1)
            if not ch:
                break
            buf.append(ch.decode("utf-8", errors="replace"))
            if ch == b"\n":
                self.out_q.put("".join(buf))
                buf = []

    def send(self, cmd: str):
        assert self.proc and self.proc.stdin
        self.proc.stdin.write((cmd + "\n").encode())
        self.proc.stdin.flush()

    def drain(self, settle: float = 0.3) -> str:
        time.sleep(settle)
        lines = []
        while True:
            try:
                lines.append(self.out_q.get_nowait())
            except queue.Empty:
                break
        return "".join(lines)

    def shutdown(self):
        if self.proc and self.proc.poll() is None:
            try:
                import signal
                os.killpg(os.getpgid(self.proc.pid), signal.SIGTERM)
                self.proc.wait(timeout=3)
            except Exception:
                pass


# ---------------------------------------------------------------------------
# Top-level observation function
# ---------------------------------------------------------------------------
def take_snapshot(driver: QemuDriver, settle: float = 0.5) -> Snapshot:
    """Send `getstats` and parse the response."""
    driver.drain(settle=0.1)        # clear any pending noise
    driver.send("getstats")
    out = driver.drain(settle=settle)
    return parse_snapshot(out)
