"""Safety guard — last line of defense before LLM-recommended actions
hit the kernel. Triple-checks every action.
"""
from __future__ import annotations
import time
from dataclasses import dataclass, field
from typing import List, Tuple
from collections import deque
from llm_advisor import Action


# ---------------------------------------------------------------------------
# Limits (conservative)
# ---------------------------------------------------------------------------
PROTECTED_PIDS = {0, 1}              # init protection
VALID_PRIORITIES = {0, 1, 2}
MAX_ACTIONS_PER_SECOND = 5            # rate limit
MAX_ACTIONS_PER_CALL = 3              # batch limit


@dataclass
class GuardResult:
    accepted: List[Action] = field(default_factory=list)
    rejected: List[Tuple[Action, str]] = field(default_factory=list)


class SafetyGuard:
    def __init__(self):
        # sliding window of recent action timestamps
        self._recent_ts: deque = deque(maxlen=MAX_ACTIONS_PER_SECOND * 5)

    def check(self, actions: List[Action]) -> GuardResult:
        result = GuardResult()
        if len(actions) > MAX_ACTIONS_PER_CALL:
            for a in actions[MAX_ACTIONS_PER_CALL:]:
                result.rejected.append((a, "too many actions per call"))
            actions = actions[:MAX_ACTIONS_PER_CALL]

        for a in actions:
            # 1. op must be setpri (no kill, no exec)
            if a.op != "setpri":
                result.rejected.append((a, f"op {a.op!r} not allowed"))
                continue

            # 2. pid must not be protected
            if a.pid in PROTECTED_PIDS:
                result.rejected.append((a, f"pid {a.pid} protected"))
                continue

            # 3. priority must be valid
            if a.new_pri not in VALID_PRIORITIES:
                result.rejected.append((a, f"prio {a.new_pri} invalid"))
                continue

            # 4. rate limit
            now = time.time()
            self._recent_ts.append(now)
            recent_1s = sum(1 for t in self._recent_ts if now - t < 1.0)
            if recent_1s > MAX_ACTIONS_PER_SECOND:
                result.rejected.append((a, "rate limit exceeded"))
                continue

            result.accepted.append(a)

        return result
