"""Anomaly detector — cheap rule-based filter that decides whether the
LLM advisor should be invoked. Avoids expensive LLM calls on healthy snapshots.

Returns a list of `Concern` objects when anomalies are present, empty list
otherwise.
"""
from __future__ import annotations
from dataclasses import dataclass
from typing import List
from observer import Snapshot, ProcStat


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
STARVATION_WAIT_THRESHOLD = 100     # ticks not scheduled
RUNAWAY_RUN_THRESHOLD = 500          # ticks of cumulative CPU
IO_MISCLASSIFY_IO_THRESHOLD = 50     # I/O ops while in LOW queue
MIN_PROCS_FOR_REPORT = 2             # don't report anomalies with single proc


@dataclass
class Concern:
    pid: int
    name: str
    kind: str           # 'starving' | 'runaway' | 'io_misclassified'
    detail: str


# ---------------------------------------------------------------------------
# Detector
# ---------------------------------------------------------------------------
def detect(snap: Snapshot) -> List[Concern]:
    """Return concerns; empty list means healthy."""
    concerns: List[Concern] = []
    if len(snap.procs) < MIN_PROCS_FOR_REPORT:
        return concerns

    for p in snap.procs:
        # Skip init
        if p.pid == 1:
            continue

        # 1. Starvation: long wait_age, non-HIGH queue
        if p.wait_age > STARVATION_WAIT_THRESHOLD and p.priority >= 1:
            concerns.append(Concern(
                pid=p.pid, name=p.name, kind="starving",
                detail=f"wait_age={p.wait_age} prio={p.priority} "
                       f"(threshold={STARVATION_WAIT_THRESHOLD})",
            ))

        # 2. Runaway CPU: lots of run_ticks, very little I/O, still in HIGH
        if (p.run_ticks > RUNAWAY_RUN_THRESHOLD
                and p.io_blocks < 5
                and p.priority == 0):
            concerns.append(Concern(
                pid=p.pid, name=p.name, kind="runaway",
                detail=f"run_ticks={p.run_ticks} io={p.io_blocks} "
                       f"still HIGH",
            ))

        # 3. I/O misclassified: many I/O blocks but stuck in LOW
        if p.io_blocks > IO_MISCLASSIFY_IO_THRESHOLD and p.priority == 2:
            concerns.append(Concern(
                pid=p.pid, name=p.name, kind="io_misclassified",
                detail=f"io={p.io_blocks} prio=LOW (should be HIGH)",
            ))

    return concerns


# ---------------------------------------------------------------------------
# Convenience: human-readable summary
# ---------------------------------------------------------------------------
def summarize(concerns: List[Concern]) -> str:
    if not concerns:
        return "OK"
    lines = []
    for c in concerns:
        lines.append(f"  - pid={c.pid:3d} {c.name:12s} {c.kind:18s} {c.detail}")
    return "Concerns:\n" + "\n".join(lines)
