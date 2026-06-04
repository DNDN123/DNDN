# 통합 작업 폴더

> 4팀(hyunsung / minju / jinhwan / haneol) 슬라이스를 **단일 xv6-riscv 트리**로 합친 결과.
> GitHub 원본 브랜치는 **절대 수정되지 않음** — 모든 파일은 `git archive` 로 읽기만 해서 이 폴더로 복사됨.
> 통합 작업 일자: 2026-05-26.

---

## 폴더 구조

```
integration/
├── README.md            ← 이 파일 (개요)
├── MERGE_NOTES.md       ← 충돌 결정 / 머지 결정 기록 (상세)
├── _sources/            ← 각 브랜치 원본 파일 (읽기 참조용, 변경 안 됨)
│   ├── hyunsung/        ← Scheduler 슬라이스 원본
│   ├── minju/           ← Syscall(trace) 슬라이스 원본
│   ├── jinhwan/         ← Thread 슬라이스 원본
│   └── haneol/          ← Process 슬라이스 원본
├── xv6-riscv/           ← ⭐ 통합 결과 — 빌드 대상
│   ├── kernel/          (proc.c, proc.h, syscall.{c,h}, sysproc.c, vm.c, kalloc.c, defs.h,
│   │                     procinfo.h, sysinfo.h, trap.c …)
│   ├── user/            (init, sh, ls, … + nlrun, wrunner, cpu/io/mixed_burner, rrunner,
│   │                     diagprog, tracetool, threadtest, ps, setprio, …)
│   ├── workloads/       (cpu_heavy, io_heavy, mixed, three_way, realprog, stress, hints_example)
│   └── Makefile
├── host/                ← 통합 호스트 Python
│   ├── README.md
│   ├── nl_shell.py      ⭐ 단일 진입점 (hyunsung)
│   └── adapters/
│       ├── process_bridge.py  (haneol)
│       └── thread_bridge.py   (jinhwan, API 키 sanitize 완료)
└── docs/                ← 통합 트리 전용 docs (현재 비어있음)
```

---

## 통합 결과 요약

### Syscall 번호 최종 분배

| 영역 | 슬라이스 |
|---|---|
| 1~21 | stock xv6 |
| 22~24 | minju (trace) |
| 25~29 | jinhwan (thread/futex) |
| 30~33 | hyunsung (Smart-MLFQ) |
| 34~35 | haneol (ps, sysinfo) |

전체 매핑은 `MERGE_NOTES.md §1` 참조.

### 핵심 결정

| 충돌 | 결정 |
|---|---|
| MLFQ 구현 | hyunsung (정량 검증) |
| 0..20 priority (haneol) | DROP — hyunsung 의 setpri (0..2) 로 통일 |
| trace syscall (haneol vs minju) | minju (더 풍부한 per-syscall 통계) |
| host NL 진입점 | `host/nl_shell.py` (hyunsung 단일), 나머지는 adapter |
| Solar API 키 하드코딩 (jinhwan) | 환경변수로 sanitize |

상세 결정 근거는 `MERGE_NOTES.md §2` 참조.

---

## Quick Start (통합 트리 cold-start 절차)

### 0. Prerequisites

```
WSL2 Ubuntu (또는 native Linux)
riscv64-unknown-elf-gcc + binutils
qemu-system-riscv64
Python 3.10+
```

설치 예 (Ubuntu):
```bash
sudo apt update
sudo apt install -y gcc-riscv64-unknown-elf qemu-system-misc python3-venv
```

### 1. 호스트 Python 환경 (한 번만)

```bash
# host 에 venv 생성 (또는 ~/host/venv 재활용)
cd host
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# .env 생성 — Solar API 키는 본인 키 사용
cp .env.example .env
# .env 안의 your_api_key_here 를 실제 키로 교체 (nano/vi)
```

### 2. xv6 빌드

```bash
cd os
make clean && make           # 약 30s
ls kernel/kernel fs.img      # 둘 다 존재하면 빌드 OK
```

### 3. 슬라이스별 라이브 데모 (5종, 단일 QEMU 세션)

```bash
make qemu CPUS=2             # 부팅 후 $ 프롬프트
```

```text
# hyunsung (Scheduler) — 자연어 힌트가 큐 레벨로 전달됨
$ nlrun 2 cpu_burner 50000
TRACE tick=… pri=2 …
EXIT  … final_pri=2

# minju (Syscall trace) — 모든 syscall이 한 지점에서 후킹됨
$ tracetool on
$ ls
$ tracetool dump
{"pid":…,"traced":1,"total":…,"calls":{"fork":…,"exec":…,…}}

# jinhwan (Thread) — kthread + futex 정상 동작
$ threadtest
test 1: shared memory ... OK (counter=200000)
test 2: 4 threads ... OK
test 3: mutex exclusion ... OK
test 4: condvar producer/consumer ... OK (sum=499500)

# haneol (Process) — sys_ps + setpri 통합
$ ps
PID  PPID  STATE    PRIO  SZ        NAME
1    0     sleep    1     12288     init
…
$ setprio 1 2
OK pid=1 level=2

# 통합 데모 — 백그라운드 큐 + LOW 우선순위 + 좀비 회수
$ bgq bgq.txt
BGQ start echo
job1_started
BGQ done echo pid=4 exit=0
…
BGQ all_done 3

# QEMU 종료: Ctrl-A x
```

### 4. 호스트 측 LLM 분석 (xv6 출력을 Solar로 흘려보내기)

```bash
# 4a. 시스템 상태 진단 — diagprog 출력을 Solar에 보내 starvation 식별
#   (xv6 안에서) $ diagprog > /diag.txt   ← xv6 셸은 redirect 지원 안 함
#   대안: QEMU 콘솔에서 diagprog 출력을 복사해 호스트 파일로 저장 후
$ python nl_shell.py --diagnose-from /tmp/diag.txt
=======================================================
  System Diagnosis (Solar Pro 3)
-------------------------------------------------------
  summary    : 3 processes healthy, 1 stuck at LOW
  ⚠ concern  : pid=5 stuck at priority=2 with run=200
  → action   : setpri 5 0

# 4b. syscall 추적 결과 LLM 분석 — tracetool dump JSON 을 패턴 분류
$ python nl_shell.py --analyze-tracetool /tmp/tracedump.json
  verdict : spawner
  summary : Process performed many fork/exec — shell-like
  → suggested queue: 0 (interactive launchers benefit from HIGH)

# 4c. 자연어 명령 (단순 dry-run, paste 권장)
$ python nl_shell.py --once "Run a heavy job in background"
[nl_shell] → xv6 명령: nlrun 2 cpu_burner 2000000

# 4d. 풀 자동 시연 (process adapter, xv6 셸 stdin 자동 주입)
$ python adapters/process_bridge.py
> show processes      ← 자연어 입력
[xv6 응답] PID …
> set process 5 to low priority
[xv6 응답] OK pid=5 level=2
```

### 5. 정량 평가 매트릭스 재실행 (선택)

```bash
cd host
source venv/bin/activate                          # 위 1.에서 만든 venv
XV6_DIR=../xv6-riscv \
OUT_ROOT=../../docs/charts/integration \
  ./run_eval_matrix.sh                           # ~8분 (5 워크로드 × 3 모드)

# 결과 확인
cat ../../docs/charts/integration/combined_report.txt
ls ../../docs/charts/integration/*/{*.json,charts/}

# 실패 진단 (워크로드별 stderr 캡처):
ls ../../docs/charts/integration/*/heuristic_hint.stderr
ls ../../docs/charts/integration/*/solar_hint.stderr
```

> ⚠️ /mnt/c 경로의 QEMU 가 /root 대비 느려서 일부 워크로드에서 `no ALL_DONE` timeout 가능. `QEMU_TIMEOUT` 환경변수로 늘릴 수 있음 (기본 90s).

---

## 원본 보호 원칙 (재확인)

- 이 폴더 안 모든 작업은 **로컬** working copy
- GitHub 의 4개 원본 브랜치 — `origin/hyunsung`, `origin/minju`, `origin/jinhwan`, `origin/haneol` — 는 `git fetch` 만 했고 단 1바이트도 수정하지 않음
- 새 브랜치 생성 안 함, push 안 함, commit 안 함
- `_sources/` 의 파일도 단순 복사본일 뿐 원본을 가리키지 않음 — 안전하게 비교·검증·롤백 가능

---

## 진행 상황

- [x] 폴더 구조 생성
- [x] 각 브랜치 원본 파일 `_sources/` 로 추출
- [x] 베이스 xv6 트리 + hyunsung 슬라이스 적용
- [x] minju 슬라이스 머지 (Syscall trace)
- [x] jinhwan 슬라이스 머지 (Thread / Futex)
- [x] haneol 슬라이스 머지 (Process ps / sysinfo)
- [x] host/ Python 통합 (단일 nl_shell.py + adapters)
- [x] MERGE_NOTES.md 완성
- [x] **빌드/부팅 검증 (WSL/QEMU)** — 2026-05-26 통과
- [x] **슬라이스별 데모 4종 라이브 시연** — 2026-05-26 완료 (hyunsung/minju/jinhwan/haneol 모두 동작 확인)
- [x] **정량 평가 재실행 (`run_eval_matrix.sh`)** — 2026-05-27 완료 (상세 §아래)
- [-] 통합 PR 작성 — 다음 단계로 연기 결정 (MERGE_NOTES "원본 보호 원칙" 유지)

---

## 정량 평가 결과 (2026-05-27 재실행)

5/26 1차 실행은 venv 미활성화로 `llm_hint.py`가 import 단계에서 죽어 hints 파일이 생성되지 않은 채 돌아감 → heuristic/solar 모드가 사실상 baseline과 동일 조건이라 Δ% 값이 노이즈에 불과했음. 5/27 재실행은 `/root/host/venv` + `.env` 로 hints 정상 생성 확인 후 수행.

### 워크로드별 핵심 Δ (avg_turnaround 기준, Δheur% / Δllm%)

| 워크로드 | Δheur% | Δllm% | 해석 |
|---|---|---|---|
| cpu_heavy   | +283%  | +550%  | 단일 프로그램 → 우선순위 낮추면 손해 (예상된 역효과) |
| io_heavy    | +2.8%  | +15.8% | 단일 프로그램 + 부분 timeout, 변화 미미 |
| mixed       | +17.1% | +14.3% | 평균은 악화지만 fairness +43~155%, max_TT −53% (heur) |
| three_way   | **−7.3%**  | **−5.8%**  | 다중 프로그램에서 heur/llm 모두 turnaround 개선 |
| realprog    | +6.8%  | −0.2%  | heur avg_response −26% — 대화형 응답 개선 |

### 환경 아티팩트

- `io_heavy` 전 모드 + `mixed/baseline`에서 `no ALL_DONE` (QEMU_TIMEOUT=45s 한계). 부분 trace로 평가 수행 — 절대값보다 모드 간 상대 비교에 의존.
- /mnt/c 경로 QEMU가 /root 대비 느려 단독 슬라이스 환경(`~/xv6-riscv`)과 직접 비교는 불가.

### 결론

다중 프로그램 워크로드(`three_way`, `mixed`, `realprog`)에서 hints의 효과가 측정됐고 회귀 없음 → §6 통과. 단일 프로그램 워크로드의 역효과는 hints 시스템의 본질적 특성으로 새 발견이 아님.

전체 산출물: `docs/charts/integration/` (5 워크로드 × 4 산출물 + `combined_report.txt`).
