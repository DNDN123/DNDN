#!/usr/bin/env python3
"""
Thread Intent Bridge for xv6 (from jinhwan's solar_bridge.py)
자연어 입력 → Solar API 번역 → xv6 thread/futex 명령 자동 전송

Integration notes:
  - 하드코딩된 API 키 제거됨 → 환경변수 UPSTAGE_API_KEY/SOLAR_API_KEY 로
  - 하드코딩된 XV6_DIR 제거됨 → 환경변수 XV6_DIR 로 (기본값은 통합 트리)
  - 보안 정책: docs/security-policy.md 참조
  - K5 fix: LLM 응답을 send_to_xv6 로 직접 주입하기 전 whitelist + guard 통과.
"""

import shlex
import subprocess
import threading
import sys
import time
import os
import requests

# ── 설정 ──────────────────────────────────────────────────────────────────────

SOLAR_API_KEY  = os.environ.get("UPSTAGE_API_KEY") or os.environ.get("SOLAR_API_KEY")
SOLAR_BASE_URL = os.environ.get("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
SOLAR_URL      = f"{SOLAR_BASE_URL}/chat/completions"
SOLAR_MODEL    = os.environ.get("UPSTAGE_MODEL", "solar-pro3")

# 통합 트리 기본 위치 (host/ 와 같은 레벨의 xv6-riscv/)
_DEFAULT_XV6 = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "..", "xv6-riscv"
)
XV6_DIR = os.environ.get("XV6_DIR", _DEFAULT_XV6)

QEMU_CMD = [
    "qemu-system-riscv64",
    "-machine", "virt",
    "-bios",    "none",
    "-kernel",  "kernel/kernel",
    "-m",       "128M",
    "-smp",     "3",
    "-nographic",
    "-global",  "virtio-mmio.force-legacy=false",
    "-drive",   "file=fs.img,if=none,format=raw,id=x0",
    "-device",  "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
]

# K5 fix: programs/builtins the LLM is allowed to invoke. Anything else from
# Solar (e.g. prompt-injected `kill 1`, `rm /` style request, or hallucinated
# binaries) is rejected before reaching QEMU stdin.
ALLOWED_CMDS = {
    "ls", "cat", "echo", "mkdir", "rm", "grep", "wc",
    "threadtest", "forktest",
    "ps", "tracetool", "setprio",
}

def guard_cmd(cmd: str) -> tuple[bool, str]:
    """Return (ok, reason). Validates the first token against ALLOWED_CMDS
    and rejects kill of init / negative or non-numeric pids.
    Also rejects shell metacharacters and any embedded newline / CR — Solar
    is allowed to translate prose into ONE xv6 command, not chain multiple."""
    cmd = (cmd or "").strip()
    if not cmd:
        return False, "empty command"
    if cmd.startswith("[오류]"):
        return False, cmd       # surface API error verbatim, don't run
    # Block command chaining / injection — shlex.split eats \n as whitespace
    # so a Solar response "ls\nkill 1" would silently pass the whitelist
    # check on parts[0]="ls" and then xv6 sh would execute both lines.
    for bad in ("\n", "\r", ";", "&", "|", "`", "$("):
        if bad in cmd:
            return False, f"refusing command with {bad!r} (chain/injection blocked)"
    try:
        parts = shlex.split(cmd)
    except ValueError as e:
        return False, f"unparseable: {e}"
    prog = parts[0]
    # `kill` is special-cased: only allowed for non-init pids.
    if prog == "kill":
        if len(parts) != 2 or not parts[1].lstrip("-").isdigit():
            return False, "kill requires one numeric pid"
        if int(parts[1]) <= 1:
            return False, "refusing to kill pid <= 1"
        return True, ""
    if prog not in ALLOWED_CMDS:
        return False, f"program not in whitelist: {prog}"
    return True, ""


SYSTEM_PROMPT = """You are an assistant for an xv6 RISC-V operating system shell.
The user will give you a natural language request in Korean or English.
Translate it into a SINGLE xv6 shell command.

Available xv6 commands:
  ls [dir]              - 파일 목록
  cat <file>            - 파일 내용 출력
  echo <text>           - 텍스트 출력
  mkdir <dir>           - 디렉토리 생성
  rm <file>             - 파일 삭제
  grep <pattern> <file> - 파일에서 패턴 검색
  wc <file>             - 단어/줄 수 세기
  kill <pid>            - 프로세스 종료
  threadtest            - 스레드/뮤텍스/조건변수 테스트 실행
  forktest              - 프로세스 fork 테스트 실행

Reply with ONLY the command. No explanation. No markdown. No quotes."""

# ── Solar API 호출 ────────────────────────────────────────────────────────────

def call_solar(user_input: str) -> str:
    if not SOLAR_API_KEY:
        # API 키 없으면 정적 폴백 (네트워크 장애 시연 안전망)
        return "echo (Solar API key missing — set UPSTAGE_API_KEY)"
    try:
        r = requests.post(
            SOLAR_URL,
            headers={
                "Authorization": f"Bearer {SOLAR_API_KEY}",
                "Content-Type":  "application/json",
            },
            json={
                "model": SOLAR_MODEL,
                "messages": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user",   "content": user_input},
                ],
                "max_tokens":  60,
                "temperature": 0,
            },
            timeout=15,
        )
        r.raise_for_status()
        return r.json()["choices"][0]["message"]["content"].strip()
    except requests.exceptions.Timeout:
        return "[오류] Solar API 응답 시간 초과"
    except requests.exceptions.HTTPError as e:
        return f"[오류] API HTTP {e.response.status_code}"
    except Exception as e:
        return f"[오류] {e}"

# ── xv6 출력 읽기 스레드 ──────────────────────────────────────────────────────

shell_ready = threading.Event()

def read_xv6(proc):
    buf = []
    while True:
        ch = proc.stdout.read(1)
        if not ch:
            break
        c = ch.decode("utf-8", errors="replace")
        sys.stdout.write(c)
        sys.stdout.flush()
        buf.append(c)
        # xv6 shell prompt: "$ "
        if len(buf) >= 2 and buf[-2] == "$" and buf[-1] == " ":
            shell_ready.set()

# ── 메인 ─────────────────────────────────────────────────────────────────────

def main():
    print("=" * 55)
    print("  Solar LLM Bridge for xv6")
    print("  자연어 → Solar API → xv6 명령 자동 실행")
    print("=" * 55)
    print()
    print("[1/3] xv6 QEMU 시작 중...")

    proc = subprocess.Popen(
        QEMU_CMD,
        cwd=XV6_DIR,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    reader = threading.Thread(target=read_xv6, args=(proc,), daemon=True)
    reader.start()

    print("[2/3] xv6 부팅 대기 중 (최대 20초)...")
    if shell_ready.wait(timeout=20):
        print("[3/3] xv6 셸 준비 완료!\n")
    else:
        print("[경고] 셸 프롬프트 미감지. 계속 진행합니다.\n")

    print("─" * 55)
    print("  자연어로 명령하세요.  종료: Ctrl+C")
    print("─" * 55)
    print()

    def send_to_xv6(cmd: str):
        proc.stdin.write((cmd + "\n").encode())
        proc.stdin.flush()

    try:
        while True:
            try:
                nl = input("자연어> ").strip()
            except EOFError:
                break

            if not nl:
                continue

            # LLM 번역
            print("  ⟳  LLM 처리 중...", end="", flush=True)
            cmd = call_solar(nl)
            ok, reason = guard_cmd(cmd)
            if not ok:
                print(f"\r  ⚠  거부됨: {reason:<40}")
                print()
                continue
            print(f"\r  →  [xv6 실행] $ {cmd:<35}")
            print()

            # xv6에 전송
            time.sleep(0.2)   # xv6 프롬프트 출력 대기
            send_to_xv6(cmd)
            time.sleep(0.8)   # 명령 실행 결과 출력 대기

    except KeyboardInterrupt:
        print("\n\n종료합니다.")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    main()
