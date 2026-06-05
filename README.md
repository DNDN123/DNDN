# Smart-MLFQ on xv6 — LLM 가이드 스케줄러

> **xv6-riscv 커널 내부에 구현한 3-단계 MLFQ(Multi-Level Feedback Queue) 스케줄러** 와,
> Upstage **Solar Pro 3** 를 사용해 자연어 요청을 커널 스케줄링 힌트로 변환하는
> 호스트-사이드 Python 브리지.
>
> **Team Project · Direction B (LLM for OS) · 2026 Spring**

> 📌 **자연어 OS 셸(LLM 통합) 전체 설명은 [`docs/nl-os-agent.md`](docs/nl-os-agent.md)** 참조 —
> 로컬 파인튜닝 모델(20종 인텐트), 학습 파이프라인(`scripts/`), 인텐트 실행기·안전가드
> (`host/executor.py`), 추상요청 분해 에이전트(`host/agent.py`),
> 신규 커널/유저 명령(`killall`/`killheavy`/`reap`/`uptime`/`sysinfo`/`tracepid`)을 다룹니다.

---

## 0. 빠른 시작 — 라이브 자연어 OS 셸 (`ask`)

xv6 콘솔 **안에서** 자연어로 말하면, 호스트 브리지가 LLM으로 인텐트를 뽑아
안전가드를 통과시킨 뒤 그 명령을 같은 콘솔에 자동 입력해 커널이 실행합니다.
LLM은 절대 커널에 들어가지 않습니다 — `kill 7` 같은 작은 명령만 syscall 경계를 넘습니다.

```bash
# 한 줄 실행 (백엔드 자동 선택: Solar 키 있으면 Solar, 없고 모델 있으면 온디바이스, 둘 다 없으면 규칙)
cd host && python3 nlos.py

# xv6 콘솔 안에서 — 그냥 자연어로 입력하면 됨 (접두사 불필요):
$ 무거운 프로세스 정리해줘
[bridge] (자연어로 해석) “무거운 프로세스 정리해줘”
[bridge] -> killheavy
killed heaviest pid 4 (cpu_burner, run_ticks=68)

# 일반 명령(ps, ls, kill 7)은 그대로 즉시 실행. 명시적으로 `ask 무거운거 정리`도 가능.
```

> **동작 원리**: 입력한 줄을 xv6 셸이 명령으로 실행하려다 실패(`exec ... failed`)하면, 브리지가 그 줄을 자연어로 재해석해 번역·실행합니다. 진짜 명령은 가로채지 않아 빠릅니다. (실패한 프로그램명이 입력 첫 단어와 일치할 때만 발동 → 오작동 없음.) 자연어가 일반 명령어로 시작하면(예: `ls 정리해줘`) 셸이 그 명령으로 실행하니, 그럴 땐 `ask` 접두를 쓰세요.

### 백엔드 3종 (코드 변경 0 — `nlos.py`가 환경변수만 세팅)

| 백엔드 | 명령 | 필요 조건 |
|---|---|---|
| **Solar Pro 3** (클라우드, 과제 필수) | `python3 nlos.py --backend solar` | `UPSTAGE_API_KEY` |
| **온디바이스** (직접 학습한 SLM) | `python3 nlos.py --backend local` | `pip install -r host/requirements-local.txt` + `ml/models/.../lora` |
| **오프라인** (LLM 없이 규칙) | `python3 nlos.py --backend offline` | 없음 — 키·모델·네트워크 불필요 |

> Solar ↔ 온디바이스 전환은 `UPSTAGE_BASE_URL`/`UPSTAGE_MODEL` 한 줄 차이.
> 브리지·가드·커널·xv6 명령은 어느 백엔드든 **완전히 동일**합니다.

### 구성 (이 트랙의 산출물)

```
xv6 콘솔:  ask <자연어>            ← os/user/ask.c  (콘솔에 @@NL 마커 송출)
host:      nlos.py                ← 백엔드 선택 + 모델 서버 수명관리 + 브리지 기동
           nlbridge.py            ← QEMU 콘솔 감싸 @@NL 포착 → 번역 → 가드 → 명령 자동 주입
           model_server.py        ← 학습한 LoRA를 OpenAI 호환 /v1로 서빙 (MPS/CUDA/CPU)
           executor.py            ← 인텐트 → xv6 명령 + SafetyGuard (kill 0·1 보호, 위험 확인)
```

호스트↔게스트 콘솔 왕복은 그 자체로 **IPC 채널**입니다(프로세스/스케줄링/동기화/스레드/시스템호출에
더해 OS 개념 하나 추가). 자세한 동작은 [`docs/nl-os-agent.md`](docs/nl-os-agent.md).

### 셋업 / 검증

```bash
# 1) xv6 빌드 (RISC-V 툴체인 필요 — macOS/Linux/WSL2; 윈도우 네이티브 불가)
#    macOS:  brew install riscv64-elf-gcc qemu
cd os && make
# 2) 호스트 의존성
cd ../host && pip install -r requirements.txt          # Solar/오프라인용 (requests 등)
pip install -r requirements-local.txt                  # 온디바이스 백엔드 쓸 때만
# 3) 오프라인 검증 (모델·QEMU 불필요 — 어느 머신에서나)
python3 test_nlos.py        # 가드/명령매핑/센티넬/폴백/프리플라이트 52종
python3 executor.py --selftest
```

`nlos.py`는 실행 전 툴체인을 점검하고, 빌드가 없으면 자동 빌드하며, 누락 시 OS별 안내를 출력합니다.

---

## 1. 프로젝트 방향성

### 한 줄 요약
> "LLM은 절대 커널 안에 들어오지 않는다. LLM은 *힌트* 만 주고, 실제 스케줄링 결정은
> 우리가 직접 구현한 xv6 MLFQ가 내린다."

### 왜 이 방향인가
강의계획서 한 줄이 핵심이었습니다:

> *"Thin wrappers around an LLM API — or projects where the OS angle is only
> 'it runs on Linux' — do not qualify and will not be accepted."*

흔한 *"LLM에 dmesg 던지고 응답 받는 도구"* 류는 이 문장에 정확히 걸리는 위험이 있어서,
우리는 다음 두 가지 원칙을 잡았습니다:

1. **LLM 출력은 syscall 경계에서 2비트 정수(`{0, 1, 2}`)로 축약되어야 한다.**
   잘못된 값이 들어오면 syscall에서 거부됨. 커널은 LLM의 존재를 모름.
2. **OS 계층이 잘못된 힌트를 자가 교정한다.**
   CPU-bound 프로세스가 HIGH로 잘못 배치되어도, 표준 MLFQ demotion 규칙이
   몇 tick 안에 자동으로 LOW 큐로 강등시킴. LLM은 *초기* 큐만 슬쩍 건드림.
3. **LLM 없이도 모든 데모가 동작한다.**
   API 키가 없으면 결정론적 키워드 휴리스틱으로 폴백 → 네트워크 장애에 데모가 안 깨짐.

---

## 2. 팀 구성과 역할 분담

4인 팀 프로젝트. 각자 xv6의 다른 슬라이스를 책임지고 **Week 13에 통합**합니다.

| 팀원 (브랜치) | 담당 슬라이스 | 핵심 작업 |
|---|---|---|
| **조현성 (`hyunsung`)** ← 본인 | **Scheduler** | 3-단계 MLFQ + LLM 힌트 통합 + Solar Pro 3 브리지 |
| `jinhwan` | Thread | 유저레벨 스레드 + worker pool (clone/futex 기반) |
| `haneol` | Process | `sys_ps` + NL Intent 브리지 + 안전 가드 |
| `minju` | Syscall | 통합 syscall 후킹 + trace JSON |

> ⚠️ `jinhwan / haneol / minju` 의 구체 작업 항목은 통합 시점에 각 브랜치의
> README 를 참조하세요. 본 저장소(`hyunsung` 브랜치)는 **Scheduler 슬라이스**만 다룹니다.

각 슬라이스는 *"이 모듈이 망해도 본인 단독 데모가 가능"* 한 형태로 설계되어 있어서,
한 명이 막혀도 발표 자체는 가능합니다.

---

## 3. 본인이 맡은 임무 (Scheduler Track)

### 구현 완료
| 항목 | 위치 | 상태 |
|---|---|---|
| 3-단계 MLFQ (HIGH/MID/LOW) + demotion + periodic boost | `os/kernel/proc.c`, `trap.c` | ✅ |
| I/O-aware queue retention (sleep 시 큐 레벨 유지) | `kernel/proc.c::sleep` | ✅ |
| Multi-CPU safe priority changes (`boost_lock` + per-proc lock) | `kernel/proc.c` | ✅ |
| 신규 syscall 4종: `setpri` / `getstats` / `settrace` / `forkpri` | `kernel/sysproc.c`, `syscall.h` | ✅ |
| 자연어 → 실행 spec 변환 (Solar Pro 3) | `host/nl_shell.py` | ✅ |
| API 키 없을 때 휴리스틱 폴백 | `nl_shell.py` | ✅ |
| 트레이스 파서 + 평가기 + 차트 (Gantt / bar) | `parse_trace.py`, `evaluator.py`, `viz.py` | ✅ |

### 진행 중 / 예정
| 항목 | 예정 주차 | 상태 |
|---|---|---|
| 정량적 3-way 비교 (RR baseline vs MLFQ vs MLFQ+LLM) | — | ✅ |
| 다른 팀원 슬라이스와 통합 | — | ✅ 완료 (2026-05) |
| 최종 영문 기술 보고서 + 데모 영상 | — | ⏳ 예정 |

---

## 4. 처음부터 지금까지의 진행 상황

### Week 09 — 팀 정렬 + 환경 셋업
- Direction B (LLM for OS) 확정, 4인 역할 1차 배정
- GitHub repo 생성 (https://github.com/DNDN123/DNDN)
- 각자 Solar Pro 3 API 키 발급 + `hello_solar.py` 동작 확인
- Python 3.11+ 환경 통일

### Week 10 — 트레이스 → 힌트 모드 구축
- xv6 콘솔 로그 포맷 정의 (`TRACE`, `EXIT` 라인)
- `parse_trace.py`: 로그 → JSON
- `llm_hint.py`: 통계 → Solar Pro 3 질의 → `hints.txt` 생성
- `wrunner.c`: 워크로드 + hints 받아서 `setpri()` 후 실행
- baseline vs LLM 비교 차트 (`evaluator.py`, `viz.py`)

### Week 11 — 자연어 → 실행 모드 구축 ⭐ 분기점
- `prompts.py`: `NL_TO_SPEC_PROMPT` 추가 (자연어 → JSON spec)
- `nl_shell.py`: 자연어 REPL → Solar → xv6 명령 한 줄 출력
- `nlrun.c`: 받은 큐 레벨로 `forkpri()` 후 exec
- 6건의 커널 패치 (1건 빌드 실패 버그, 5건 품질 이슈):
  - 🚨 `forkret()` → 강의용 xv6 호환 수정
  - C1: `last_boost_tick` 멀티 CPU race → `boost_lock`
  - C2: `total_io_blocks` 가 kwait/pause 도 카운트 → 필터링
  - C3: scheduler가 락 잡은 채로 printf → `swtch` 후로 이동
  - I2: 무조건 trace ON → `settrace()` syscall로 토글
  - I4: fork→setpri race → `forkpri()` syscall로 원자화
- ✅ **Hello World 시연 통과** — 약한 팀의 가장 큰 분기점 돌파

### Week 12 — 정량적 평가 ✅
- baseline (RR) vs MLFQ vs MLFQ+LLM 3-way 매트릭스 실험
- 평가 지표: turnaround time, response time, throughput, Jain's fairness index
- 워크로드 6종 (`cpu_heavy`, `io_heavy`, `mixed`, `realprog`, `stress`, `three_way`)
- 차트 자동 생성 (`run_eval_matrix.sh`)

### Week 13 — 팀 통합 (예정) ⏳
- 4명 슬라이스 머지 → 단일 xv6 트리에서 빌드/부팅
- 모듈 간 인터페이스 충돌 해결
- 통합 데모 시나리오 작성

### Week 14 — 최종 발표 (예정) ⏳
- 영문 기술 보고서 (architecture + OS-concept-to-file mapping)
- 영문 발표 슬라이드
- 백업 데모 영상 (라이브 실패 대비)
- 각자 본인 슬라이스 섹션 발표

---

## 5. 저장소 구조

```
.
├── README.md  CLAUDE.md            ← 문서
├── .gitignore
├── os/                             ← ⭐ 빌드 대상 xv6 (4팀 통합 커널+유저)
│   ├── kernel/                     ← MLFQ + reap + trace + thread/futex + ps
│   ├── user/                       ← nlrun, ps, setprio, kill, killall, killheavy,
│   │                                 reap, uptime, sysinfo, tracepid, *_burner, bgq ...
│   └── workloads/                  ← cpu_heavy/io_heavy/mixed/three_way/realprog/bgq
├── host/                           ← 호스트 Python (NL 브리지)
│   ├── nl_shell.py                 ← 자연어 REPL
│   ├── executor.py                 ← ★ M3: 인텐트→xv6 명령 + 안전가드
│   ├── agent.py                    ← ★ M4: 추상요청("정리해줘") 다단계 분해
│   ├── prompts.py  parse_trace.py  evaluator.py  viz.py
│   └── adapters/                   ← process_bridge.py / thread_bridge.py
├── ml/                             ← 모델 학습 (Windows/GPU)
│   ├── data/                       ← seeds/, augmented*, train/test.jsonl
│   ├── scripts/                    ← train, evaluate, build_train, augment_offline,
│   │                                 gen_intent_seeds, compare, ask.py, run_all.sh
│   └── models/                     ← smartmlfq-qwen-3b-r64-e10 (gitignored, 큼)
├── autonomous-agent/               ← Phase-2 자율 스케줄러 에이전트 (별도)
└── docs/                           ← nl-os-agent, syscall-allocation, trace-format,
                                      integration-overview, MERGE_NOTES, charts/ ...
```

> **`os/`(커널) + `host/`(NL 브리지)** 가 메인 빌드/데모 대상. `ml/`은 모델 학습, `autonomous-agent/`는 Phase-2 실험.

---

## 6. Quick Start

### 1) xv6 빌드 (통합본 — 맥북/리눅스, RISC-V 툴체인 필요)
```bash
cd os && make clean && make qemu
# QEMU 안: ps / setprio / killall cpu_burner / killheavy / reap / uptime / sysinfo
```

### 2) 호스트 Python 환경
```bash
cd host
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
cp .env.example .env       # UPSTAGE_API_KEY 입력 (없어도 동작)
python3 hello_solar.py     # 'ok' 가 뜨면 OK
```

### 3) 자연어 → 실행 흐름
```bash
# 호스트: 자연어를 xv6 명령으로 변환
python nl_shell.py --once "Run a heavy job in background"

# xv6 (QEMU 안):
$ nlrun 2 cpu_burner 1000000
TRACE tick=... pid=... pri=2 slice=...     # MLFQ가 LLM 힌트 받아들임
EXIT  pid=... name=cpu_burner ... final_pri=2
```

### 4) Batch hint 흐름 (참고)
```bash
# 1. xv6에서 baseline 워크로드를 돌리고 콘솔 로그를 캡처해둔 뒤:
python parse_trace.py qemu_baseline.log --output baseline.json

# 2. baseline 통계 → Solar → hints.txt (API 키 없으면 휴리스틱 폴백)
python llm_hint.py baseline.json --output hints.txt

# 3. xv6 (QEMU 안): hints.txt를 받아 setpri 후 워크로드 재실행
$ wrunner cpu_heavy.txt hints.txt
TRACE ... / EXIT ...                       # LLM-guided 트레이스 캡처

# 4. baseline vs LLM 비교 차트
python parse_trace.py qemu_llm.log --output llm.json
python evaluator.py --baseline baseline.json --llm llm.json
python viz.py --baseline baseline.json --llm llm.json --out-dir docs/charts/batch_demo
```

---

## 7. 설계 메모

### LLM이 커널에 들어오지 않는 이유
Solar Pro 3는 JSON을 반환합니다. 그 JSON에서 우리가 추출하는 건 큐 레벨 정수 하나뿐.
이 정수는 `forkpri()` syscall의 인자로 전달되며, 유효 범위(`0..2`)를 벗어나면 거부됨.
즉, LLM 응답에 어떤 이상한 텍스트가 섞여 있어도 커널에는 정수 한 개만 도달.

### 폴백이 단순 휴리스틱인 이유
`nl_shell.py` 의 폴백은 *"heavy", "background", "interactive"* 같은 키워드만 본다.
의도적으로 결정론적으로 만들었어요 — 네트워크가 끊기든, API 키가 잘못됐든,
**같은 입력이면 항상 같은 spec** 이 나옴. 발표 시연을 안정시키는 게 목적.

---

## 8. 강의계획서 요구사항 체크리스트

| 요구사항 (강의계획서 기준) | 충족 위치 |
|---|---|
| OS 개념의 **substantive** 구현 | MLFQ, syscall path, spinlock, sleep/wakeup 모두 실제 xv6 커널 C |
| LLM thin-wrapper **금지** | LLM 출력은 syscall 경계에서 2비트 정수로 축약 |
| Public GitHub repo + 셋업/실행/데모 | 본 저장소 README (위 §6) |
| 최종 산출물 영문 | 최종 슬라이드/보고서 (작성 중) |
| Solar Pro 3 backend | `.env.example` 의 `UPSTAGE_MODEL=solar-pro3` 기본값 |

---

## 9. 라이선스 / 크레딧

- **xv6-riscv**: MIT License (MIT). 본 저장소의 패치는 강의에서 제공된 미수정
  xv6-riscv 트리 위에 적용됩니다.
- **Solar Pro 3**: Upstage 제품. OpenAI 호환 Chat Completions 엔드포인트로 호출.
