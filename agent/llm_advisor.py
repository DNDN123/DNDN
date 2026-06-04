"""LLM advisor — given a snapshot + concerns, ask the LLM what to do.

Output: list of `Action` objects (setpri actions only — limited blast radius).

Uses the same Solar/Ollama API shape as the integration host bridge, so
the same model (Solar Pro 3 OR our fine-tuned Qwen via Ollama) works
without code changes.
"""
from __future__ import annotations
import json
import os
import re
from dataclasses import dataclass
from typing import List, Optional
import requests

from observer import Snapshot
from anomaly_detector import Concern


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
# Defaults match the project contract in integration/host/.env.example:
#   UPSTAGE_BASE_URL=https://api.upstage.ai/v1   UPSTAGE_MODEL=solar-pro3
# Override both via env to swap in a local model (e.g. Ollama):
#   UPSTAGE_BASE_URL=http://localhost:11434/v1   UPSTAGE_MODEL=smartmlfq
BASE_URL = os.environ.get("UPSTAGE_BASE_URL",
                          "https://api.upstage.ai/v1")
MODEL = os.environ.get("UPSTAGE_MODEL", "solar-pro3")
API_KEY = (os.environ.get("UPSTAGE_API_KEY")
           or os.environ.get("ANTHROPIC_API_KEY")
           or "not-needed")

MAX_ACTIONS_PER_CALL = 3


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------
@dataclass
class Action:
    op: str          # "setpri"
    pid: int
    new_pri: int     # 0/1/2
    reason: str

    def to_xv6_cmd(self) -> str:
        return f"setpri {self.pid} {self.new_pri}"


# ---------------------------------------------------------------------------
# Prompt
# ---------------------------------------------------------------------------
SYSTEM_PROMPT = """You are an OS scheduler advisor for a 3-level MLFQ on xv6.

You receive (a) a snapshot of all processes, (b) anomaly concerns from a
rule-based detector. Recommend AT MOST 3 setpri actions to improve fairness
or throughput.

Queue levels: 0=HIGH (interactive/IO), 1=MID (mixed), 2=LOW (CPU heavy).

Output JSON only, no markdown:
  {
    "actions": [
      {"op": "setpri", "pid": <int>, "new_pri": 0|1|2, "reason": "<short>"}
    ]
  }

Constraints:
  - NEVER touch pid=1 (init)
  - At most 3 actions per call
  - If no action needed, return {"actions": []}
"""


def build_user_prompt(snap: Snapshot, concerns: List[Concern]) -> str:
    proc_lines = []
    for p in snap.procs:
        proc_lines.append(
            f"  pid={p.pid:3d} prio={p.priority} run={p.run_ticks} "
            f"io={p.io_blocks} wait_age={p.wait_age} name={p.name}"
        )
    concern_lines = [f"  - pid={c.pid} {c.kind}: {c.detail}" for c in concerns]

    return (
        "Snapshot:\n" + "\n".join(proc_lines) +
        "\n\nConcerns:\n" + ("\n".join(concern_lines) if concern_lines
                              else "  (none — but use your judgment)") +
        "\n\nOutput JSON:"
    )


# ---------------------------------------------------------------------------
# Backend call
# ---------------------------------------------------------------------------
def call_llm(user_prompt: str, timeout: float = 30.0) -> str:
    r = requests.post(
        f"{BASE_URL}/chat/completions",
        headers={"Authorization": f"Bearer {API_KEY}"},
        json={
            "model": MODEL,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": user_prompt},
            ],
            "temperature": 0,
            "response_format": {"type": "json_object"},
        },
        timeout=timeout,
    )
    r.raise_for_status()
    return r.json()["choices"][0]["message"]["content"]


# ---------------------------------------------------------------------------
# Parse
# ---------------------------------------------------------------------------
def parse_actions(raw: str) -> List[Action]:
    raw = raw.strip()
    raw = re.sub(r"^```(?:json)?\s*", "", raw)
    raw = re.sub(r"\s*```$", "", raw)
    try:
        obj = json.loads(raw)
    except json.JSONDecodeError:
        m = re.search(r"\{.*\}", raw, re.DOTALL)
        if not m:
            return []
        try:
            obj = json.loads(m.group(0))
        except json.JSONDecodeError:
            return []

    actions: List[Action] = []
    for a in obj.get("actions", [])[:MAX_ACTIONS_PER_CALL]:
        try:
            actions.append(Action(
                op=a.get("op", "setpri"),
                pid=int(a["pid"]),
                new_pri=int(a["new_pri"]),
                reason=a.get("reason", ""),
            ))
        except (KeyError, ValueError, TypeError):
            continue
    return actions


# ---------------------------------------------------------------------------
# Top-level advice
# ---------------------------------------------------------------------------
def advise(snap: Snapshot, concerns: List[Concern]) -> List[Action]:
    if not concerns:
        return []
    user = build_user_prompt(snap, concerns)
    try:
        raw = call_llm(user)
    except Exception as e:
        print(f"  [advisor] LLM call failed: {e}")
        return []
    return parse_actions(raw)
