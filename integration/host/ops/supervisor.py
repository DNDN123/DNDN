"""Supervisor LLM — OS-grounded audit/diagnosis layer over the integration tree.

This is NOT a thin LLM wrapper. The supervisor can only answer questions
grounded in xv6/integration artifacts via the tools in `ops/tools.py`. Free
chat ("hi", "tell me a joke", "what's the weather") is rejected by design,
keeping the project's "LLM for OS" framing intact.

Architecture:

    user question
        │
        ▼
    Solar Pro 3 ───[chosen tool + args as JSON]───┐
        ▲                                          │
        │                                          ▼
        │                              ops/tools.py reads
        │                              MERGE_NOTES, eval JSONs,
        │                              syscall.h, etc.
        │                                          │
        └────────[tool result fed back]────────────┘
        │
        ▼
    Solar produces final answer (cites evidence)
        │
        ▼
    user

The loop runs up to MAX_TURNS rounds (default 6). If the model never calls a
tool it gets one warning, then the request is rejected — answers without
evidence are not allowed.

Not callable from nl_shell.py. Live in `host/ops/` so the supervisor is
visibly separated from the hot path.
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import re
import sys
from pathlib import Path
from typing import Any

# Persistent log — same file as nl_shell so a single tail catches everything.
_LOG_PATH = Path(__file__).resolve().parent.parent / "run.log"
logging.basicConfig(
    filename=str(_LOG_PATH),
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s supervisor] %(message)s",
)
log = logging.getLogger("supervisor")

# Make ops/tools.py importable when running this file as a script.
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE.parent))   # so `from ops.tools import ...` works
from ops import tools as T   # noqa: E402

try:
    from dotenv import load_dotenv
    load_dotenv(_HERE.parent / ".env")
except ImportError:
    pass

try:
    from openai import OpenAI
except ImportError:
    print("ERROR: openai SDK not installed. Run: pip install openai python-dotenv",
          file=sys.stderr)
    sys.exit(1)


# ── Configuration ───────────────────────────────────────────────────────

API_KEY  = os.getenv("UPSTAGE_API_KEY") or os.getenv("SOLAR_API_KEY")
BASE_URL = os.getenv("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
MODEL    = os.getenv("UPSTAGE_MODEL", "solar-pro3")
EFFORT   = os.getenv("UPSTAGE_REASONING_EFFORT", "low")
MAX_TURNS = 6


SYSTEM_PROMPT = f"""\
You are the Supervisor LLM for an xv6 OS integration project. Your job is to
audit, diagnose, and answer questions about the integrated 4-team xv6 tree
(scheduler / syscall / thread / process slices) and its quantitative
evaluation artifacts.

STRICT RULES:
1. Only answer questions grounded in the integration tree's data. Refuse free
   chat, jokes, general programming help, weather, news, language questions.
2. To gather evidence you MUST call tools. Do not invent file contents,
   metrics, or line numbers.
3. Every claim in your final answer must be supported by something a tool
   returned. Cite the tool name and key field.
4. If a question is off-topic, respond with the REJECT action immediately.
5. YOUR FIRST RESPONSE MUST BE EITHER a `call_tool` action OR a `reject`
   action. You may NOT emit `answer` on turn 0 — you have no evidence yet.

PROTOCOL — your every response must be a single JSON object, no prose
outside the JSON, no markdown fences. The `action` field is REQUIRED and
MUST be one of "call_tool", "answer", "reject":

  {{ "action": "call_tool", "tool": "<name>", "args": {{...}} }}
  {{ "action": "answer",    "summary": "...", "evidence": ["tool: ..."] }}
  {{ "action": "reject",    "reason": "off-topic / not grounded in xv6 data" }}

If you do not emit a valid action JSON, you will be re-prompted with an
error message and must try again.

Available tools:
{T.describe_tools()}

When a tool returns, you will see a system message with the result. Then
either call another tool, give the final answer, or reject. You have at most
{MAX_TURNS} tool-call turns. Be efficient — pick the most informative tool
first.

Example of a grounded answer:

  {{ "action": "answer",
     "summary": "io_heavy regressed +15.8% under Solar mode because the workload \
is single-program — applying a LOW hint to the only running process has no \
fairness benefit and only adds turnaround.",
     "evidence": ["read_eval_report(io_heavy): comparison.summary.avg_turnaround.delta_llm_pct=+15.8"]
  }}

Off-topic examples you MUST reject:
  - "hello" / "안녕하세요" / "how are you"
  - "explain pointers in C"
  - "write me a poem"
  - "what's the weather"

These get {{"action":"reject", "reason":"..."}} and nothing else.
"""


# ── Local off-topic guard (runs BEFORE LLM call, saves API quota) ───────

_OFFTOPIC_PATTERNS = [
    r"^\s*(안녕|하이|hi|hello|hey|good\s*morning)[\s.!?]*$",
    r"weather|날씨",
    r"tell\s+me\s+a\s+joke|joke|농담",
    r"write\s+(me\s+)?a\s+(poem|story|song)",
    r"explain.*(pointer|recursion|sort)\b(?!.*xv6)",
]


def is_obviously_offtopic(q: str) -> bool:
    qq = q.strip().lower()
    if len(qq) < 3:
        return True
    for pat in _OFFTOPIC_PATTERNS:
        if re.search(pat, qq, re.IGNORECASE):
            return True
    return False


# ── Tool execution + response truncation ────────────────────────────────

# Truncate huge tool outputs before feeding back to the model. Long
# MERGE_NOTES / combined_report dumps would blow the context window.
_MAX_TOOL_BYTES = 8000          # per individual string
_MAX_AGGREGATE_BYTES = 32000    # entire JSON payload per tool call


def execute_tool(name: str, args: dict) -> dict:
    if name not in T.TOOLS:
        return {"error": f"unknown tool '{name}'", "available": list(T.TOOLS)}
    fn = T.TOOLS[name]["fn"]
    try:
        result = fn(**(args or {}))
    except TypeError as e:
        return {"error": f"bad arguments to {name}: {e}"}
    except Exception as e:
        return {"error": f"{type(e).__name__}: {e}"}
    return result


def _truncate(obj: Any) -> Any:
    """Cap (a) any individual string at _MAX_TOOL_BYTES, AND (b) the entire
    JSON-serialized payload at _MAX_AGGREGATE_BYTES. Without (b) a tool that
    returns thousands of short strings (e.g. grep on a popular pattern) still
    blows the context window even when (a) passes."""
    def _per_string(x):
        if isinstance(x, str):
            if len(x) > _MAX_TOOL_BYTES:
                return x[:_MAX_TOOL_BYTES] + f"\n... [truncated, {len(x) - _MAX_TOOL_BYTES} bytes more]"
            return x
        if isinstance(x, dict):
            return {k: _per_string(v) for k, v in x.items()}
        if isinstance(x, list):
            return [_per_string(y) for y in x]
        return x
    capped = _per_string(obj)
    blob = json.dumps(capped, ensure_ascii=False)
    if len(blob) > _MAX_AGGREGATE_BYTES:
        return {"_aggregate_truncated": True,
                "_original_size_bytes": len(blob),
                "_preview": blob[:_MAX_AGGREGATE_BYTES] + f"\n... [truncated, {len(blob) - _MAX_AGGREGATE_BYTES} bytes more]"}
    return capped


# ── JSON parsing from model output ──────────────────────────────────────

def parse_action(text: str) -> dict:
    """Extract the JSON action from the model's reply.
    Robust to: leading/trailing whitespace, code fences, prose preamble.
    Returns dict WITHOUT an "action" key if parsing fails, so the caller
    can re-prompt with a protocol-error correction."""
    text = (text or "").strip()
    # Strip ```json ... ``` fences
    text = re.sub(r"^```(?:json)?\s*", "", text)
    text = re.sub(r"\s*```\s*$", "", text)
    try:
        obj = json.loads(text)
        if isinstance(obj, dict):
            return obj
    except json.JSONDecodeError:
        pass
    # Find first balanced { ... } that parses
    depth = 0
    start = -1
    for i, ch in enumerate(text):
        if ch == "{":
            if depth == 0:
                start = i
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0 and start >= 0:
                try:
                    obj = json.loads(text[start:i+1])
                    if isinstance(obj, dict):
                        return obj
                except json.JSONDecodeError:
                    continue
    # Parse failed entirely — return marker dict the loop will retry on.
    return {"_parse_error": True, "_raw": text[:400]}


# ── Main loop ───────────────────────────────────────────────────────────

def supervise(question: str, verbose: bool = False) -> dict:
    log.info("question: %s", question[:200])
    if is_obviously_offtopic(question):
        log.info("rejected by local guard")
        return {"action": "reject",
                "reason": "local guard: input looks like greeting / general chat",
                "input": question}

    if not API_KEY:
        return {"action": "reject",
                "reason": "UPSTAGE_API_KEY not set — supervisor needs Solar"}

    client = OpenAI(api_key=API_KEY, base_url=BASE_URL)
    messages: list[dict] = [
        {"role": "system",  "content": SYSTEM_PROMPT},
        {"role": "user",    "content": question},
    ]
    tool_calls: list[dict] = []

    for turn in range(MAX_TURNS):
        try:
            resp = client.chat.completions.create(
                model=MODEL,
                messages=messages,
                temperature=0.0,
                max_tokens=1500,
                # Force JSON-only output — prevents Solar from emitting prose
                # reasoning between/around the protocol JSON.
                response_format={"type": "json_object"},
                extra_body={"reasoning_effort": EFFORT},
            )
        except Exception as e:
            return {"action": "reject",
                    "reason": f"Solar API error: {type(e).__name__}: {e}"}

        raw = resp.choices[0].message.content or ""
        action = parse_action(raw)
        if verbose:
            print(f"[turn {turn}] model → {json.dumps(action, ensure_ascii=False)[:200]}",
                  file=sys.stderr)

        # Normalize common Solar variants:
        #   {"action":"verify_k_fix","args":{...}}      → call_tool
        #   {"action":"verify_k_fix","tool":"verify..."} → call_tool
        if action.get("action") in T.TOOLS and "tool" not in action:
            action = {"action": "call_tool", "tool": action["action"],
                      "args": action.get("args", {})}
        elif (action.get("action") not in {"call_tool", "answer", "reject", None}
              and action.get("tool") in T.TOOLS):
            action = {"action": "call_tool", "tool": action["tool"],
                      "args": action.get("args", {})}

        # Reject malformed responses (no action key) and re-prompt once.
        if "action" not in action or action["action"] not in {"call_tool", "answer", "reject"}:
            messages.append({"role": "assistant", "content": raw})
            messages.append({
                "role": "system",
                "content": ('[protocol error] your reply did not contain a valid "action" key. '
                            'Reply with EXACTLY one JSON object: '
                            '{"action":"call_tool","tool":"<name>","args":{...}} OR '
                            '{"action":"answer","summary":"...","evidence":[...]} OR '
                            '{"action":"reject","reason":"..."}.'),
            })
            continue

        if action.get("action") == "call_tool":
            tname = action.get("tool", "")
            targs = action.get("args", {}) or {}
            result = execute_tool(tname, targs)
            truncated = _truncate(result)
            tool_calls.append({"tool": tname, "args": targs, "result_keys": list(result) if isinstance(result, dict) else None})
            # Feed back to model
            messages.append({"role": "assistant", "content": raw})
            messages.append({
                "role": "system",
                "content": f"[tool_result for {tname}]\n{json.dumps(truncated, ensure_ascii=False, indent=2)}",
            })
            continue

        # answer or reject ends the loop
        action["_tool_calls"] = tool_calls
        # Enforce: an "answer" with zero tool calls is disallowed.
        if action.get("action") == "answer" and not tool_calls:
            return {"action": "reject",
                    "reason": "supervisor refused to answer without tool evidence",
                    "raw_attempt": action.get("summary", "")[:300]}
        return action

    return {"action": "reject",
            "reason": f"exceeded MAX_TURNS={MAX_TURNS} without a final answer",
            "_tool_calls": tool_calls}


# ── CLI ─────────────────────────────────────────────────────────────────

def _render(result: dict) -> str:
    act = result.get("action")
    if act == "answer":
        lines = ["✓ ANSWER", "─" * 60, result.get("summary", "(no summary)"), ""]
        ev = result.get("evidence") or []
        if ev:
            lines.append("Evidence:")
            for e in ev:
                lines.append(f"  • {e}")
        tc = result.get("_tool_calls") or []
        if tc:
            lines.append("")
            lines.append(f"Tool calls used ({len(tc)}):")
            for t in tc:
                lines.append(f"  → {t['tool']}({t.get('args')})")
        return "\n".join(lines)
    if act == "reject":
        return f"✗ REJECTED\n─────────────\n{result.get('reason', '(no reason)')}"
    return json.dumps(result, indent=2, ensure_ascii=False)


def main():
    ap = argparse.ArgumentParser(
        description="Supervisor LLM — OS-grounded audit/diagnosis over integration tree",
    )
    ap.add_argument("--once", "-1", metavar="QUESTION",
                    help="answer one question and exit")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="show tool-call trace on stderr")
    args = ap.parse_args()

    if args.once:
        result = supervise(args.once, verbose=args.verbose)
        print(_render(result))
        return 0 if result.get("action") == "answer" else 1

    # Interactive REPL
    print("Supervisor LLM (OS-grounded). Type a question. 'exit' or Ctrl-D to quit.")
    print(f"Available tools: {', '.join(T.TOOLS)}")
    print()
    # Exact termination words only — typos do NOT trigger exit; they get a
    # helpful "did you mean exit?" hint instead. Prevents accidental exits
    # when the user is starting to type a short query.
    _EXIT_EXACT = {"exit", "quit", ":q"}
    # Likely-typo set: if a short input matches one of these, suggest 'exit'
    # rather than sending it to the LLM (which would just REJECT it as
    # off-topic and waste an API call).
    _EXIT_TYPOS = {"eixt", "exi", "ext", "exti", "qiut", "qit", "quti",
                   "quitt", "q", "/exit", "/quit", "끝", "종료"}
    while True:
        try:
            q = input("supv> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not q:
            continue
        lower = q.lower()
        if lower in _EXIT_EXACT:
            break
        if lower in _EXIT_TYPOS:
            print("(did you mean 'exit'? type 'exit' or Ctrl-D to quit)\n")
            continue
        # Help-like inputs: redirect to docs rather than sending to LLM.
        if lower in {"help", "?", "/help", "도움말"}:
            print("Available tools:")
            for name, spec in T.TOOLS.items():
                doc = (spec["fn"].__doc__ or "").strip().split("\n")[0]
                print(f"  • {name}  — {doc}")
            print("\nAsk in plain Korean/English about MERGE_NOTES, eval JSONs, "
                  "K1~K4 fixes, syscall numbers, or specific workloads.\n")
            continue
        result = supervise(q, verbose=args.verbose)
        print(_render(result))
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
