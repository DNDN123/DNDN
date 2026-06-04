"""Generate seed examples for the EXPANDED intent set (process + file + system
commands), so the model can handle the full xv6 surface — not just workload RUN.

Schema is unchanged: {"user": ..., "spec": {"cmd", "args", "queue_hint", "reason"}}.
We only EXTEND the cmd vocabulary, so the existing 3500 examples stay valid.

New cmds: ps, kill, setpri, rm, mkdir, ln, uptime, sysinfo, trace, explain.
(cpu_burner/io_burner/mixed_burner/echo/cat/ls/reject already covered.)

Safety: kill targets only pid 2..40 here; killing pid 0/1 (init) is already
trained as `reject` in 06_adversarial_reject — we do NOT add kill-0/1 here.

Output: ../data/seeds/07_intents.jsonl   (deterministic; safe to re-run)
"""
from __future__ import annotations
import json
import random
from pathlib import Path

rng = random.Random(7)
OUT = Path("../data/seeds/07_intents.jsonl")
rows: list[dict] = []

def add(user: str, cmd: str, args, reason: str, q: int = 0):
    rows.append({"user": user, "spec": {"cmd": cmd, "args": [str(a) for a in args],
                                        "queue_hint": q, "reason": reason}})

# ---------------------------------------------------------------------------
# ps — list processes
# ---------------------------------------------------------------------------
PS = ["프로세스 목록", "프로세스 목록 보여줘", "지금 뭐 돌고 있어?", "실행 중인 프로세스 알려줘",
      "프로세스 상태 보여줘", "ps", "ps 보여줘", "돌아가는 거 다 보여줘", "프로세스 리스트",
      "list processes", "show running processes", "what's running", "show the process table",
      "list all processes", "ps please", "which processes are active"]
for u in PS:
    add(u, "ps", [], "list active processes")

# ---------------------------------------------------------------------------
# kill — terminate a process (pid 2..40 only)
# ---------------------------------------------------------------------------
KILL_KO = ["{p}번 프로세스 꺼줘", "{p}번 죽여", "pid {p} 종료해줘", "{p}번 종료", "프로세스 {p} kill",
           "{p}번 멈춰줘", "{p}번 끝내", "{p}번 강제 종료"]
KILL_EN = ["kill pid {p}", "terminate process {p}", "kill {p}", "stop process {p}",
           "end process {p}", "kill the process with pid {p}"]
for tmpl in KILL_KO + KILL_EN:
    for p in rng.sample(range(2, 41), 6):
        add(tmpl.format(p=p), "kill", [p], f"kill pid {p}")

# ---------------------------------------------------------------------------
# setpri — change MLFQ priority (prio 0=HIGH,1=MID,2=LOW)
# ---------------------------------------------------------------------------
# word -> prio
HIGH = ["우선순위 높여", "우선순위 올려", "높은 큐로", "HIGH로 보내", "raise priority", "set to high", "prioritize"]
MID  = ["우선순위 보통으로", "중간 큐로", "MID로", "set to mid", "normal priority"]
LOW  = ["우선순위 낮춰", "우선순위 내려", "낮은 큐로", "LOW로 보내", "뒤로 보내", "lower priority", "deprioritize", "set to low"]
for words, prio, label in [(HIGH, 0, "HIGH"), (MID, 1, "MID"), (LOW, 2, "LOW")]:
    for w in words:
        for p in rng.sample(range(2, 41), 3):
            ko = any('가' <= ch <= '힣' for ch in w)
            user = f"{p}번 {w}" if ko else f"{w} of pid {p}"
            add(user, "setpri", [p, prio], f"set pid {p} priority to {label}")
# explicit numeric priority
for p in rng.sample(range(2, 41), 8):
    prio = rng.choice([0, 1, 2])
    add(f"{p}번 우선순위 {prio}로", "setpri", [p, prio], f"set pid {p} to prio {prio}")
    add(f"set pid {p} priority {prio}", "setpri", [p, prio], f"set pid {p} to prio {prio}")

# ---------------------------------------------------------------------------
# rm — delete a file (destructive; guarded at execution, not here)
# ---------------------------------------------------------------------------
FILES = ["foo.txt", "data.log", "tmp.txt", "old.txt", "notes.md", "out.csv", "test.bin", "a.txt", "scratch", "cache.dat"]
RM_KO = ["{f} 지워줘", "{f} 삭제해줘", "{f} 제거", "{f} 지워", "파일 {f} 삭제"]
RM_EN = ["delete {f}", "remove {f}", "rm {f}", "delete the file {f}"]
for tmpl in RM_KO + RM_EN:
    for f in rng.sample(FILES, 4):
        add(tmpl.format(f=f), "rm", [f], f"delete file {f}")

# ---------------------------------------------------------------------------
# mkdir — create a directory
# ---------------------------------------------------------------------------
DIRS = ["logs", "build", "test", "data", "tmp", "out", "src", "bin"]
MK_KO = ["{d} 폴더 만들어", "디렉토리 {d} 생성", "{d} 디렉토리 만들어줘", "폴더 {d} 생성해줘"]
MK_EN = ["make a directory called {d}", "create folder {d}", "mkdir {d}", "new directory {d}"]
for tmpl in MK_KO + MK_EN:
    for d in rng.sample(DIRS, 4):
        add(tmpl.format(d=d), "mkdir", [d], f"create directory {d}")

# ---------------------------------------------------------------------------
# ln — link target -> linkname
# ---------------------------------------------------------------------------
for _ in range(20):
    a = rng.choice(FILES); b = rng.choice(DIRS) + ".lnk"
    u = rng.choice([f"{a} 를 {b} 로 링크", f"{a} 링크 {b} 만들어", f"link {a} to {b}", f"ln {a} {b}"])
    add(u, "ln", [a, b], f"hard-link {a} to {b}")

# ---------------------------------------------------------------------------
# cat / ls (file-oriented; cmd already exists, add explicit file reads)
# ---------------------------------------------------------------------------
for f in FILES:
    u = rng.choice([f"{f} 열어줘", f"{f} 내용 보여줘", f"cat {f}", f"show {f}", f"read {f}"])
    add(u, "cat", [f], f"read file {f}")
for u in ["파일 목록", "ls", "ls 보여줘", "여기 뭐 있어?", "list files", "show files", "디렉토리 내용"]:
    add(u, "ls", [], "list files in cwd")

# ---------------------------------------------------------------------------
# uptime / sysinfo
# ---------------------------------------------------------------------------
for u in ["업타임", "업타임 알려줘", "얼마나 켜져 있었어?", "부팅한 지 얼마나 됐어?",
          "시스템 켜진 지 얼마나 됐어", "가동 시간", "가동 시간 보여줘", "틱 얼마나 지났어?",
          "uptime", "uptime 보여줘", "how long has the system been up", "system uptime",
          "show uptime", "how long since boot", "ticks since boot", "what's the uptime"]:
    add(u, "uptime", [], "report system uptime")
for u in ["시스템 정보", "시스템 정보 보여줘", "메모리 얼마 남았어?", "남은 메모리 알려줘",
          "여유 메모리 보여줘", "시스템 상태", "시스템 상태 보여줘", "프로세스 몇 개 떠 있어?",
          "메모리 상태 보여줘", "메모리랑 프로세스 수 알려줘", "sysinfo", "sysinfo 보여줘",
          "system info", "show system stats", "free memory", "how much memory is free",
          "memory usage", "how many processes are running", "system status"]:
    add(u, "sysinfo", [], "report system info (free mem, nproc)")

# ---------------------------------------------------------------------------
# trace — toggle syscall tracing for a pid
# ---------------------------------------------------------------------------
for p in rng.sample(range(2, 41), 8):
    add(f"{p}번 추적 켜줘", "trace", [p, "on"], f"enable syscall trace for pid {p}")
    add(f"turn on tracing for pid {p}", "trace", [p, "on"], f"enable trace pid {p}")
    add(f"{p}번 추적 꺼", "trace", [p, "off"], f"disable trace for pid {p}")
    add(f"stop tracing pid {p}", "trace", [p, "off"], f"disable trace pid {p}")

# ---------------------------------------------------------------------------
# explain — informational; model answers, no OS action
# ---------------------------------------------------------------------------
# explain — informational questions. Use explicit question forms so the model
# learns to route "what is X / X가 뭐야 / 설명해줘" to explain (was the weakest
# intent at 52%). Topic goes into args; cmd must classify as explain.
_TOPICS_KO = ["MLFQ", "스케줄러", "좀비 프로세스", "라운드 로빈", "컨텍스트 스위치", "syscall",
              "프로세스", "우선순위 큐", "setpri", "fork와 exec 차이", "데드락", "세마포어",
              "페이지 테이블", "인터럽트", "타임 슬라이스"]
_TOPICS_EN = ["MLFQ", "the scheduler", "a zombie process", "round robin", "context switching",
              "a syscall", "a process", "priority queues", "what setpri does", "deadlock",
              "semaphores", "the page table", "interrupts", "time slicing"]
_Q_KO = ["{t}가 뭐야", "{t}가 뭔지 설명해줘", "{t} 설명해줘", "{t}에 대해 알려줘", "{t} 개념 알려줘"]
_Q_EN = ["what is {t}", "explain {t}", "tell me about {t}", "how does {t} work", "what does {t} mean"]
for t in _TOPICS_KO:
    for q in _Q_KO:
        add(q.format(t=t), "explain", [t], "informational question; answer without acting")
for t in _TOPICS_EN:
    for q in _Q_EN:
        add(q.format(t=t), "explain", [t], "informational question; answer without acting")

# ---------------------------------------------------------------------------
# killall {name} — terminate all processes by name (Tier-1 cleanup)
# ---------------------------------------------------------------------------
PROGNAMES = ["cpu_burner", "io_burner", "mixed_burner", "echo", "grep", "wc", "cat"]
KA_KO = ["{n} 다 꺼줘", "{n} 전부 종료", "{n} 모두 죽여", "{n} 전부 꺼", "모든 {n} 종료해줘", "{n} 다 종료해"]
KA_EN = ["kill all {n}", "killall {n}", "terminate all {n}", "kill every {n} process"]
for tmpl in KA_KO + KA_EN:
    for nm in PROGNAMES:
        add(tmpl.format(n=nm), "killall", [nm], f"kill all processes named {nm}")

# ---------------------------------------------------------------------------
# killheavy {count?} — kill the heaviest CPU process(es) (Tier-1 optimize)
# ---------------------------------------------------------------------------
KH_KO = ["무거운 거 꺼줘", "제일 무거운 프로세스 종료", "가장 무거운 거 죽여", "느린 거 정리해줘",
         "CPU 많이 먹는 거 꺼줘", "제일 무거운 거 정리", "시스템 무거운 거 정리해줘"]
KH_EN = ["kill the heaviest process", "killheavy", "kill the most cpu-heavy process",
         "terminate the heaviest job", "clean up the heaviest process"]
for u in KH_KO + KH_EN:
    add(u, "killheavy", [], "kill the single heaviest process")
for k in (2, 3, 5):
    add(f"무거운 거 {k}개 정리해줘", "killheavy", [k], f"kill the {k} heaviest processes")
    add(f"kill the {k} heaviest processes", "killheavy", [k], f"kill the {k} heaviest processes")

# ---------------------------------------------------------------------------
# reap — clean up abandoned zombie processes
# ---------------------------------------------------------------------------
REAP = ["좀비 없애줘", "좀비 프로세스 정리", "좀비 청소", "좀비 다 치워", "좀비 정리해줘",
        "reap zombies", "clean up zombies", "remove zombie processes", "reap", "clear zombies"]
for u in REAP:
    add(u, "reap", [], "reap abandoned zombie processes")

# ---------------------------------------------------------------------------
# Write (dedup by user)
# ---------------------------------------------------------------------------
seen, uniq = set(), []
for r in rows:
    if r["user"] not in seen:
        seen.add(r["user"]); uniq.append(r)

OUT.parent.mkdir(parents=True, exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    for r in uniq:
        f.write(json.dumps(r, ensure_ascii=False) + "\n")

import collections
print(f"Wrote {len(uniq)} intent seeds to {OUT}")
print("cmd distribution:", dict(collections.Counter(r['spec']['cmd'] for r in uniq)))
