"""Supervisor tool functions — OS-grounded data access.

Every tool returns a structured dict the LLM can reason over. None of these
touch network or hot-path code; they only READ artifacts produced by the
integration tree:

  - docs/charts/integration/{workload}/{baseline,heuristic,solar,metrics}.json
  - docs/charts/integration/combined_report.txt
  - integration/MERGE_NOTES.md
  - integration/xv6-riscv/kernel/syscall.h
  - integration/xv6-riscv/user/usys.pl
  - integration/xv6-riscv/user/user.h
  - integration/xv6-riscv/Makefile
  - K1~K4 fix label locations in source

Adding new tools is the right way to give the supervisor new capabilities —
do NOT give it free-form code execution or shell access.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
from typing import Any

# Paths anchored relative to this file so the supervisor works regardless of
# where it's invoked from.
_OPS_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_INTEGRATION = os.path.normpath(os.path.join(_OPS_DIR, "..", ".."))
ROOT_PROJECT     = os.path.normpath(os.path.join(ROOT_INTEGRATION, ".."))
EVAL_ROOT        = os.path.join(ROOT_PROJECT, "docs", "charts", "integration")
XV6_DIR          = os.path.join(ROOT_INTEGRATION, "xv6-riscv")
KNOWN_WORKLOADS  = ("cpu_heavy", "io_heavy", "mixed", "three_way",
                   "realprog", "stress", "bgq", "hints")


# ── Tool 1: workload list ────────────────────────────────────────────────

def list_workloads() -> dict:
    """Workload spec files packaged in fs.img + which ones have eval data."""
    spec_dir = os.path.join(XV6_DIR, "workloads")
    specs = sorted(f[:-4] for f in os.listdir(spec_dir) if f.endswith(".txt"))
    evaluated = []
    if os.path.isdir(EVAL_ROOT):
        evaluated = sorted(d for d in os.listdir(EVAL_ROOT)
                           if os.path.isdir(os.path.join(EVAL_ROOT, d)))
    return {"specs_in_fsimg": specs, "evaluated": evaluated}


# ── Tool 2: eval report ─────────────────────────────────────────────────

def read_eval_report(workload: str | None = None) -> dict:
    """Read one workload's metrics.json (3-way: baseline/heuristic/llm + comparison).
    workload=None returns the combined cross-workload report."""
    if workload is None:
        path = os.path.join(EVAL_ROOT, "combined_report.txt")
        if not os.path.exists(path):
            return {"error": f"combined report not found at {path}"}
        with open(path, encoding="utf-8") as f:
            return {"path": path, "text": f.read()}

    path = os.path.join(EVAL_ROOT, workload, "metrics.json")
    if not os.path.exists(path):
        return {"error": f"no eval data for workload '{workload}' at {path}",
                "available": list_workloads()["evaluated"]}
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    # Strip per_pid noise to keep token usage reasonable
    for mode in ("baseline", "heuristic", "llm"):
        if mode in data and isinstance(data[mode], dict):
            data[mode].pop("per_pid_tt", None)
    return {"path": path, "workload": workload, "data": data}


# ── Tool 3: trace dump JSON parse ───────────────────────────────────────

def read_trace_dump(json_text: str) -> dict:
    """Parse a tracetool dump JSON (`tracetool dump` output) and summarize."""
    try:
        d = json.loads(json_text.strip())
    except json.JSONDecodeError as e:
        return {"error": f"invalid JSON: {e}"}
    calls = d.get("calls", {})
    total = d.get("total", sum(calls.values()) if calls else 0)
    errors = d.get("errors", 0)
    # Classify dominant syscall category
    spawn = calls.get("fork", 0) + calls.get("exec", 0) + calls.get("wait", 0)
    fileops = calls.get("open", 0) + calls.get("read", 0) + calls.get("write", 0) + calls.get("close", 0)
    return {
        "pid": d.get("pid"),
        "total": total,
        "errors": errors,
        "calls": calls,
        "categories": {"spawn": spawn, "fileops": fileops,
                       "other": max(0, total - spawn - fileops)},
    }


# ── Tool 4: MERGE_NOTES section read ────────────────────────────────────

def read_merge_notes(section: str | None = None) -> dict:
    """Return the full MERGE_NOTES.md, or a single section like '1', '2.3', '8'."""
    path = os.path.join(ROOT_INTEGRATION, "MERGE_NOTES.md")
    if not os.path.exists(path):
        return {"error": f"MERGE_NOTES.md not found at {path}"}
    with open(path, encoding="utf-8") as f:
        text = f.read()
    if section is None:
        return {"path": path, "text": text}
    # Extract section. Headings look like "## 1. ..." or "### 2.3 ..."
    sec_re = re.compile(
        rf"^#{{2,4}}\s+{re.escape(section)}(?:[.\s].*?)?$",
        re.MULTILINE,
    )
    m = sec_re.search(text)
    if not m:
        return {"error": f"section '{section}' not found",
                "hint": "valid sections: 0, 1, 1.1, 2, 2.1, 2.2, 2.3, 3, 3.1..3.5, "
                       "4, 5, 5.1..5.4, 6, 6.1, 6.2, 7, 8"}
    start = m.start()
    # End = next heading at the same or higher level
    level = m.group(0).count("#", 0, m.group(0).index(" "))
    rest = text[m.end():]
    next_hdr = re.search(rf"^#{{2,{level}}}\s", rest, re.MULTILINE)
    end = m.end() + (next_hdr.start() if next_hdr else len(rest))
    return {"path": path, "section": section, "text": text[start:end].strip()}


# ── Tool 5: integration tree grep ───────────────────────────────────────

def grep_integration(pattern: str, file_glob: str = "**/*") -> dict:
    """Recursive grep across integration/. Returns up to 60 matches."""
    if not pattern.strip():
        return {"error": "pattern is empty"}
    # Pattern length guard — pathological ReDoS patterns (e.g., "(a+)+b") tend
    # to be short, but extreme inputs (>500 chars) are almost certainly junk.
    if len(pattern) > 500:
        return {"error": f"pattern too long ({len(pattern)} chars, max 500)"}
    # Use ripgrep if available, else fall back to python-side scan
    try:
        proc = subprocess.run(
            ["rg", "--no-heading", "-n", "--max-count", "5",
             "-g", file_glob, pattern, ROOT_INTEGRATION],
            capture_output=True, text=True, timeout=15,
        )
        lines = proc.stdout.splitlines()[:60]
        return {"pattern": pattern, "matches": lines, "truncated": len(lines) == 60}
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass
    # Fallback: python walk. We can't enforce a wall-clock timeout on re.search
    # without signals (which don't work in non-main threads). Bound the work
    # instead: a max file count + a max-bytes-scanned budget. Adequate for
    # patterns the supervisor actually generates.
    try:
        rx = re.compile(pattern)
    except re.error as e:
        return {"error": f"invalid regex: {e}"}
    matches: list[str] = []
    bytes_scanned = 0
    files_scanned = 0
    BYTES_BUDGET = 5_000_000   # ~5 MB scan cap
    FILES_BUDGET = 500
    for root, _, files in os.walk(ROOT_INTEGRATION):
        # Skip build artifacts / hidden dirs
        if any(seg in root for seg in (".git", "__pycache__", "_sources")):
            continue
        for fn in files:
            if not fn.endswith((".c", ".h", ".py", ".md", ".pl", ".S", ".txt", "Makefile")):
                continue
            if files_scanned >= FILES_BUDGET or bytes_scanned >= BYTES_BUDGET:
                return {"pattern": pattern, "matches": matches,
                        "truncated": True, "limit_hit": "budget"}
            path = os.path.join(root, fn)
            try:
                with open(path, encoding="utf-8", errors="replace") as f:
                    files_scanned += 1
                    for i, line in enumerate(f, 1):
                        bytes_scanned += len(line)
                        if rx.search(line):
                            matches.append(f"{path}:{i}: {line.rstrip()}")
                            if len(matches) >= 60:
                                return {"pattern": pattern, "matches": matches,
                                        "truncated": True}
                        if bytes_scanned >= BYTES_BUDGET:
                            break
            except OSError:
                continue
    return {"pattern": pattern, "matches": matches, "truncated": False}


# ── Tool 6: K1~K4 fix verification ──────────────────────────────────────

# Source-of-truth: MERGE_NOTES §8 (line ~270+). If you add K5 etc., extend here.
_K_FIX_SPECS = {
    "K1": {
        "purpose": "allocproc zeros traced/syscall_count/is_thread/tgroup",
        "files": ["xv6-riscv/kernel/proc.c"],
        "search": [r"K1\s*fix", r"p->traced\s*=\s*0", r"is_thread\s*=\s*0"],
    },
    "K2": {
        "purpose": "syscall_count[] / syscall_errors[] arrays 32 → 64",
        "files": ["xv6-riscv/kernel/proc.h", "xv6-riscv/kernel/syscall.c",
                  "xv6-riscv/kernel/sysproc.c", "xv6-riscv/user/user.h",
                  "xv6-riscv/user/tracetool.c"],
        "search": [r"\[64\]", r"K2\s*fix"],
    },
    "K3": {
        "purpose": "fork/forkpri propagate traced flag",
        "files": ["xv6-riscv/kernel/proc.c"],
        "search": [r"K3\s*fix", r"np->traced\s*=\s*p->traced"],
    },
    "K4": {
        "purpose": "host adapter priority range 0..20 → 0..2",
        "files": ["host/adapters/process_bridge.py"],
        "search": [r"K4\s*fix", r"0\.\.2\s*MLFQ"],
    },
    "K5": {
        "purpose": "thread/main teardown order — page-use-after-free guard in freeproc",
        "files": ["xv6-riscv/kernel/proc.c"],
        "search": [r"K5\s*fix", r"has_live_thread", r"thread_freepagetable"],
    },
}


def verify_k_fix(label: str | None = None) -> dict:
    """Confirm K1~K4 fix labels still live in the integrated code at expected
    locations. label=None checks ALL of K1..K4 and returns a summary."""
    if label is None or label == "":
        # Batch mode — verify every known label.
        summary = {}
        for lbl in _K_FIX_SPECS:
            summary[lbl] = verify_k_fix(lbl)
        return {"mode": "batch",
                "verdict": "ALL_PRESENT" if all(s.get("verdict") == "PRESENT"
                                                for s in summary.values()) else "INCOMPLETE",
                "by_label": summary}
    label = label.upper()
    if label not in _K_FIX_SPECS:
        return {"error": f"unknown label '{label}'",
                "known": list(_K_FIX_SPECS.keys())}
    spec = _K_FIX_SPECS[label]
    out: dict[str, Any] = {"label": label, "purpose": spec["purpose"], "hits": []}
    for rel in spec["files"]:
        path = os.path.join(ROOT_INTEGRATION, rel)
        if not os.path.exists(path):
            out["hits"].append({"file": rel, "error": "file not found"})
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            content = f.read()
        per_pat = {}
        for pat in spec["search"]:
            ms = list(re.finditer(pat, content))
            per_pat[pat] = len(ms)
        out["hits"].append({"file": rel,
                            "patterns": per_pat,
                            "total_matches": sum(per_pat.values())})
    out["verdict"] = (
        "PRESENT" if any(h.get("total_matches", 0) > 0 for h in out["hits"])
        else "MISSING"
    )
    return out


# ── Tool 7: syscall table consistency ───────────────────────────────────

def read_syscall_table() -> dict:
    """Cross-check syscall numbers across kernel/syscall.h, user/usys.pl,
    user/user.h. Detect drift like "SYS_X defined in syscall.h but no entry
    in usys.pl"."""
    sh   = os.path.join(XV6_DIR, "kernel", "syscall.h")
    pl   = os.path.join(XV6_DIR, "user", "usys.pl")
    uh   = os.path.join(XV6_DIR, "user", "user.h")
    out: dict[str, Any] = {"sources": [sh, pl, uh]}

    # Parse syscall.h: #define SYS_x  N
    nums: dict[str, int] = {}
    if os.path.exists(sh):
        with open(sh, encoding="utf-8") as f:
            for line in f:
                m = re.match(r"\s*#define\s+SYS_(\w+)\s+(\d+)", line)
                if m:
                    nums[m.group(1)] = int(m.group(2))
    out["syscall_h_count"] = len(nums)

    # Parse usys.pl: entry("name");
    usys: set[str] = set()
    if os.path.exists(pl):
        with open(pl, encoding="utf-8") as f:
            for line in f:
                m = re.search(r'entry\("(\w+)"\)', line)
                if m:
                    usys.add(m.group(1))
    out["usys_pl_count"] = len(usys)

    # Parse user.h for syscall prototypes (heuristic — any line ending with `;`
    # that follows the "system calls" comment block). We just grep for known.
    uh_decls: set[str] = set()
    if os.path.exists(uh):
        with open(uh, encoding="utf-8") as f:
            text = f.read()
        for name in nums:
            if re.search(rf"\b{name}\b\s*\(", text):
                uh_decls.add(name)
    out["user_h_count"] = len(uh_decls)

    # Drift detection
    in_sh   = set(nums)
    missing_in_pl = sorted(in_sh - usys - {"exit", "fork", "wait", "read", "write",
                                            "kill", "exec", "fstat", "chdir", "dup",
                                            "getpid", "sbrk", "sleep", "uptime",
                                            "open", "pipe", "close", "mkdir", "unlink",
                                            "link", "mknod"})
    missing_in_uh = sorted(in_sh - uh_decls - {"exit", "fork", "wait", "read", "write",
                                                "kill", "exec", "fstat", "chdir", "dup",
                                                "getpid", "sbrk", "sleep", "uptime",
                                                "open", "pipe", "close", "mkdir", "unlink",
                                                "link", "mknod"})
    out["drift"] = {
        "in_syscall_h_but_not_usys_pl": missing_in_pl,
        "in_syscall_h_but_not_user_h":  missing_in_uh,
    }
    out["new_syscalls_22_to_35"] = {n: name for name, n in nums.items() if 22 <= n <= 35}
    return out


# ── Registry exposed to the supervisor ──────────────────────────────────

TOOLS = {
    "list_workloads":    {"fn": list_workloads,    "args": {}},
    "read_eval_report":  {"fn": read_eval_report,  "args": {"workload": "str|null"}},
    "read_trace_dump":   {"fn": read_trace_dump,   "args": {"json_text": "str"}},
    "read_merge_notes":  {"fn": read_merge_notes,  "args": {"section": "str|null"}},
    "grep_integration":  {"fn": grep_integration,  "args": {"pattern": "str",
                                                              "file_glob": "str (optional)"}},
    "verify_k_fix":      {"fn": verify_k_fix,      "args": {"label": "K1|K2|K3|K4"}},
    "read_syscall_table":{"fn": read_syscall_table,"args": {}},
}


def describe_tools() -> str:
    """Human-readable tool catalog injected into the system prompt."""
    lines = []
    for name, spec in TOOLS.items():
        args_desc = ", ".join(f"{k}: {v}" for k, v in spec["args"].items()) or "(none)"
        doc = (spec["fn"].__doc__ or "").strip().split("\n")[0]
        lines.append(f"  - {name}({args_desc}) — {doc}")
    return "\n".join(lines)
