#!/usr/bin/env python3
"""Offline tests for the NL-OS host pipeline — no model, no QEMU, no network.

Covers the parts that must be correct regardless of which machine runs them:
  - SafetyGuard verdicts (protect init, gate destructive, validate args)
  - build_command mapping (all intents -> exact xv6 command strings)
  - the @@NL sentinel parser (reacts to program output, not echoed input)
  - the offline rule fallback (Korean + English, both word orders)
  - preflight tool detection

Run anywhere:   python3 host/test_nlos.py
"""
import sys
import executor
import nlbridge

_fail = 0


def check(name, got, want):
    global _fail
    ok = got == want
    if not ok:
        _fail += 1
    print(f"  [{'ok ' if ok else 'FAIL'}] {name}: got={got!r} want={want!r}")


print("SafetyGuard:")
g = executor.SafetyGuard()
check("kill init rejected", g.evaluate({"cmd": "kill", "args": ["1"]}).verdict, "reject")
check("kill pid 0 rejected", g.evaluate({"cmd": "kill", "args": ["0"]}).verdict, "reject")
check("kill negative rejected", g.evaluate({"cmd": "kill", "args": ["-1"]}).verdict, "reject")
check("kill huge pid rejected", g.evaluate({"cmd": "kill", "args": ["99999999999"]}).verdict, "reject")
check("kill injection rejected", g.evaluate({"cmd": "kill", "args": ["1; rm -rf /"]}).verdict, "reject")
check("kill 7 -> confirm", g.evaluate({"cmd": "kill", "args": ["7"]}).command, "kill 7")
check("kill 7 verdict", g.evaluate({"cmd": "kill", "args": ["7"]}).verdict, "confirm")
check("rm gated", g.evaluate({"cmd": "rm", "args": ["a.txt"]}).verdict, "confirm")
check("ps allowed", g.evaluate({"cmd": "ps", "args": []}).verdict, "allow")
check("bad prio rejected", g.evaluate({"cmd": "setpri", "args": ["5", "9"]}).verdict, "reject")
check("setpri bad pid rejected", g.evaluate({"cmd": "setpri", "args": ["-1", "2"]}).verdict, "reject")
check("setpri init pid rejected", g.evaluate({"cmd": "setpri", "args": ["1", "2"]}).verdict, "reject")
check("setpri valid", g.evaluate({"cmd": "setpri", "args": ["5", "2"]}).command, "setprio 5 2")
check("reject passthrough", g.evaluate({"cmd": "reject", "args": []}).verdict, "reject")
check("explain no command", g.evaluate({"cmd": "explain", "args": ["MLFQ"]}).command, None)

print("build_command:")
check("cpu_burner q2 heavy+bg", executor.build_command(
    {"cmd": "cpu_burner", "args": ["1000"], "queue_hint": 2})[0],
    "nlrun 2 cpu_burner 1000000000 &")
check("setpri -> setprio", executor.build_command(
    {"cmd": "setpri", "args": ["5", "2"]})[0], "setprio 5 2")
check("trace -> tracepid", executor.build_command(
    {"cmd": "trace", "args": ["6", "on"]})[0], "tracepid 6 1")
check("killheavy bare", executor.build_command({"cmd": "killheavy", "args": []})[0], "killheavy")

print("build_command — all 20 intents:")
_CASES = [
    ({"cmd": "cpu_burner", "args": ["5000"], "queue_hint": 2}, "nlrun 2 cpu_burner 1000000000 &"),
    ({"cmd": "io_burner", "args": ["100"], "queue_hint": 0}, "nlrun 0 io_burner 1000000000 &"),
    ({"cmd": "mixed_burner", "args": ["7"], "queue_hint": 1}, "nlrun 1 mixed_burner 1000000000 &"),
    ({"cmd": "echo", "args": ["hello", "world"]}, "echo hello world"),
    ({"cmd": "ls", "args": []}, "ls"),
    ({"cmd": "ls", "args": ["/dir"]}, "ls /dir"),
    ({"cmd": "cat", "args": ["README"]}, "cat README"),
    ({"cmd": "rm", "args": ["a.txt"]}, "rm a.txt"),
    ({"cmd": "mkdir", "args": ["d"]}, "mkdir d"),
    ({"cmd": "ln", "args": ["a", "b"]}, "ln a b"),
    ({"cmd": "ps", "args": []}, "ps"),
    ({"cmd": "kill", "args": ["7"]}, "kill 7"),
    ({"cmd": "setpri", "args": ["5", "1"]}, "setprio 5 1"),
    ({"cmd": "trace", "args": ["6", "off"]}, "tracepid 6 0"),
    ({"cmd": "killall", "args": ["cpu_burner"]}, "killall cpu_burner"),
    ({"cmd": "killheavy", "args": ["2"]}, "killheavy 2"),
    ({"cmd": "reap", "args": []}, "reap"),
    ({"cmd": "uptime", "args": []}, "uptime"),
    ({"cmd": "sysinfo", "args": []}, "sysinfo"),
    ({"cmd": "explain", "args": ["MLFQ"]}, None),
    ({"cmd": "reject", "args": []}, None),
]
for spec, want in _CASES:
    check(f"map {spec['cmd']}", executor.build_command(spec)[0], want)

print("sentinel parser:")
check("program output parsed",
      nlbridge.extract_request("@@NL 7번 프로세스 꺼줘"), "7번 프로세스 꺼줘")
check("echoed input ignored",
      nlbridge.extract_request("$ ask 7번 프로세스 꺼줘"), None)
check("plain line ignored",
      nlbridge.extract_request("$ ps"), None)
check("empty request ignored",
      nlbridge.extract_request("@@NL   "), None)

print("offline rule fallback:")
check("kor pid kill", nlbridge.rule_fallback("4번 프로세스 꺼줘")["cmd"], "kill")
check("kor pid value", nlbridge.rule_fallback("4번 프로세스 꺼줘")["args"], ["4"])
check("eng kill order", nlbridge.rule_fallback("kill process 7")["args"], ["7"])
check("eng kill 7", nlbridge.rule_fallback("kill 7")["args"], ["7"])
check("cleanup heavy", nlbridge.rule_fallback("무거운 프로세스 정리해줘")["cmd"], "killheavy")
check("zombies", nlbridge.rule_fallback("clean up zombies")["cmd"], "reap")
check("zombie+정리 ordering", nlbridge.rule_fallback("좀비 정리해줘")["cmd"], "reap")
check("ps", nlbridge.rule_fallback("프로세스 목록 보여줘")["cmd"], "ps")
check("uptime", nlbridge.rule_fallback("업타임 얼마나 됐어")["cmd"], "uptime")

print("preflight:")
issues = nlbridge.preflight()
check("returns list", isinstance(issues, list), True)
# missing-tool branch: pretend nothing is on PATH
import shutil
_orig = shutil.which
shutil.which = lambda *_a, **_k: None
try:
    check("missing tools detected", len(nlbridge.preflight()) >= 3, True)
finally:
    shutil.which = _orig

print()
if _fail:
    print(f"FAILED: {_fail} check(s)")
    sys.exit(1)
print("ALL CHECKS PASSED")
