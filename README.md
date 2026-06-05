# DNDN Project — 우리가 만든 xv6 위에서 자연어로 움직이는 OS

> 4인 팀이 함께 xv6-riscv 커널의 4개 슬라이스(**Scheduler · Syscall · Thread · Process**)를
> 직접 구현해 **하나의 빌드 트리로 통합**하고, 그 위에 **LLM(Upstage Solar Pro 3)은
> *조언만* 하고 결정은 *커널이* 내리는** — 자연어로 움직이는 운영체제 프로젝트.
>
> **Team Project · Direction B (LLM for OS) · 2026 Spring** · GitHub: `DNDN123/DNDN`

### 한 줄 요약
> **"우리가 만든 xv6 위에서, LLM은 *조언만* 하고 결정은 *커널이* 한다 — 자연어로 움직이는 OS."**
>
> LLM은 절대 커널 안에 들어오지 않는다. LLM은 *힌트* 만 주고, 실제 결정·실행·교정은
> 우리가 직접 구현·통합한 xv6 커널(스케줄러·시스템콜·스레드·프로세스)이 내린다.

---

## 1. 무엇을 만든 프로젝트인가

운영체제의 **핵심 4개념**을 4명이 한 조각씩 맡아 xv6 커널에 직접 구현하고, 충돌을 해결하며
**하나의 빌드 가능한 트리로 통합**했습니다. 그 위에 호스트(PC) 측 Python 브리지가 자연어를
받아 LLM으로 큐 레벨(0/1/2)로 번역해 OS로 흘려보냅니다. 즉 두 가지를 동시에 증명하는 프로젝트:

1. **OS 내부 구현** — 스케줄러·시스템콜·스레드·프로세스를 커널 레벨 C로 작성
2. **올바른 AI×OS 결합** — LLM은 "조언자"일 뿐, 실행·결정·교정은 OS가 담당 (thin wrapper 아님)

> **4개 슬라이스는 따로 노는 모듈이 아니라 한 커널에서 맞물립니다.** 자연어로 띄운 작업은
> **Scheduler**(`forkpri` → MLFQ)가 우선순위를 잡아 실행하고, 그 실행 흔적을 **Syscall**(per-pid trace)이
> 기록하며, **Process**(`ps`/`sysinfo`)가 외부에서 관찰하고, 같은 프로세스 안에서 **Thread**(kthread+futex)가
> 동시 실행·동기화를 담당합니다. 즉 *한 번의 데모 세션*에서 네 개념이 동시에 동작하는 게 핵심입니다.

---

## 2. 팀 구성 — OS 4개념을 한 조각씩

| 슬라이스 | 담당 (브랜치) | 구현한 OS 개념 | 핵심 결과물 |
|---|---|---|---|
| **Scheduler** | (`hyunsung`) | CPU 스케줄링 | 3-단계 MLFQ + LLM 큐 힌트 |
| **Syscall** | (`minju`) | 시스템콜 | 통합 syscall 후킹 + per-pid trace JSON |
| **Thread** | (`jinhwan`) | 스레드·동기화 | kthread + futex(mutex/condvar) |
| **Process** | (`haneol`) | 프로세스 관리 | `ps` / `sysinfo` + 안전 가드 |

> 각 슬라이스는 *"이 모듈이 막혀도 단독 데모가 가능"* 하도록 설계됐고, 4개를 합친 결과가
> 이 저장소의 `integration/` 입니다. 본 저장소는 `hyunsung` 이 관리하지만 **내용은 팀 4명의 통합 결과**입니다.

---

## 3. 설계 방향성 — 왜 이렇게 했나

강의계획서의 한 줄이 설계를 결정했습니다:

> *"Thin wrappers around an LLM API — or projects where the OS angle is only
> 'it runs on Linux' — do not qualify and will not be accepted."*

그래서 "LLM 껍데기가 아님"을 **코드 구조로** 강제하는 3원칙을 세웠습니다:

1. **LLM 격리** — LLM 출력은 syscall 경계에서 **2비트 정수 `{0,1,2}`** 로 축약. 범위 밖이면 거부.
   커널은 LLM의 존재를 모름.
2. **OS 자가교정** — LLM이 CPU-bound 작업을 HIGH로 잘못 배치해도, 표준 MLFQ demotion이
   몇 tick 안에 LOW로 강등. LLM은 *초기* 큐만 슬쩍 건드림.
3. **LLM 없이도 동작** — API 키가 없으면 결정론적 키워드 휴리스틱으로 폴백 → 데모가 안 깨짐.

---

## 4. 시스템 구조 / 데이터 흐름

```
[유저 자연어]  "무거운 작업 백그라운드로 돌려줘"
      │
      ▼   HOST (Python)
  nl_shell.py → Solar Pro 3 → JSON spec → 검증·안전가드 → 큐 레벨 정수로 축약
      │        (키 없으면 휴리스틱 폴백)
      ▼   "nlrun 2 cpu_burner 1000000"
════════ syscall 경계 — 여기서 LLM 격리 (2비트만 통과) ════════
      ▼   xv6 KERNEL  (4슬라이스가 한 트리에서 맞물림)
  forkpri(2) ─► 3-단계 MLFQ가 실제 스케줄링      … Scheduler (잘못된 힌트는 demotion이 자동 교정)
      │
      ├─ thread_create / futex_wait·wake        … Thread   (프로세스 내 동시 실행·동기화)
      ├─ trace_on / trace_stats                 … Syscall  (실행된 syscall을 per-pid 기록)
      └─ ps / sysinfo                           … Process  (스케줄링 결과를 외부에서 관찰)
```

---

## 5. Syscall 번호 최종 분배 (통합 결과)

| 번호 | 슬라이스 | 추가된 syscall |
|---|---|---|
| 1~21 | stock xv6 | (미수정) |
| 22~24 | `minju` | `trace_on` / `trace_off` / `trace_stats` |
| 25~29 | `jinhwan` | `thread_create/join/exit` + `futex_wait/wake` |
| 30~33 | `hyunsung` | `setpri` / `getstats` / `settrace` / `forkpri` |
| 34~35 | `haneol` | `ps` / `sysinfo` |

> 4팀이 모두 22~25 영역을 쓰려 한 게 가장 큰 충돌이었고, 위 표로 합의해 14개 syscall이
> 충돌 없이 공존합니다. 상세 결정 근거는 `integration/MERGE_NOTES.md`.

---

## 6. 최종 결과 — 빌드 · 검증 (모두 통과)

| 항목 | 상태 | 근거 |
|---|---|---|
| 빌드 / 부팅 | ✅ PASS | WSL + QEMU, 단일 트리 |
| 라이브 데모 · **Scheduler** | ✅ PASS | `nlrun 2 cpu_burner` → 3-단계 MLFQ 강등 관찰 + `getstats` |
| 라이브 데모 · **Syscall** | ✅ PASS | `trace_on` 후 `tracetool dump` → per-pid syscall JSON |
| 라이브 데모 · **Thread** | ✅ PASS | `threadtest` → kthread 생성·join + futex mutex/condvar |
| 라이브 데모 · **Process** | ✅ PASS | `ps` / `sysinfo` 로 실행 중 프로세스·자원 관찰, `bgq` |
| 정량 평가 (baseline/heuristic/Solar) | ✅ PASS | `three_way` avg_turnaround **−7.3% / −5.8%** |
| 자동 회귀 (`sanity_check.sh`) | ✅ **8/8** | 2026-06-03 fresh ext4 재실행, exit 0 |
| 통합·보안 수정 | ✅ | `K1~K7` (allocproc 누수, 배열 오버플로, thread teardown race, prompt-injection 가드 등) |

---

## 7. 저장소 구조

```
.
├── README.md                  ← 이 파일 (프로젝트 개요)
├── integration/               ← ⭐ 4팀 통합 결과 (메인 산출물)
│   ├── xv6-riscv/             ← 통합 커널 + user + workloads (빌드 대상)
│   │   ├── kernel/            ← MLFQ + trace + thread/futex + ps 전부
│   │   ├── user/              ← nlrun, wrunner, threadtest, tracetool, ps, setprio, bgq …
│   │   └── workloads/         ← cpu_heavy / io_heavy / mixed / three_way / realprog / bgq …
│   ├── host/                  ← 호스트 Python (3-레이어)
│   │   ├── nl_shell.py        ← ⭐ Hot path — 자연어 → 큐 레벨 (단일 진입점)
│   │   ├── nl_demo.py         ← 터미널 데모 (자연어 → MLFQ 시뮬레이션)
│   │   ├── adapters/          ← process / thread Intent 브리지
│   │   ├── ops/supervisor.py  ← Cold path — OS-grounded 감사 LLM
│   │   └── dev_chat.py        ← Dev path — 자유 대화 (평가 경로 밖)
│   ├── MERGE_NOTES.md         ← 충돌 결정 / K1~K7 fix 매핑
│   ├── sanity_check.sh        ← 한 줄 자동 회귀 (build+boot+8 마커)
│   ├── REPORT.html / _EN      ← 보고서 (한/영)
│   ├── SLIDES.html / _EN      ← 발표 슬라이드 15장 (한/영)
│   ├── DEMO.html              ← 브라우저 인터랙티브 데모
│   └── demo.gif               ← 실제 QEMU 세션 캡처
└── docs/                      ← 설계 문서 + 정량 평가 차트
    ├── syscall-allocation.md / trace-format.md / hints-format.md
    ├── hello-world.md / integration-checklist.md / security-policy.md
    └── charts/{standalone,integration}/   ← 워크로드별 평가 결과
```

---

## 8. 산출물 (Deliverables)

| 산출물 | 위치 | 설명 |
|---|---|---|
| **통합 트리** | `integration/xv6-riscv/` | 빌드·부팅 가능한 단일 xv6 |
| **보고서** | `integration/REPORT.html`, `REPORT_EN.html` | 한/영 기술 보고서 |
| **슬라이드** | `integration/SLIDES.html`, `SLIDES_EN.html` | 한/영 발표 덱 (15장, 방향키·PDF 인쇄) |
| **데모 GIF** | `integration/demo.gif` | 실제 QEMU 세션 (4슬라이스 + 통합) |
| **인터랙티브 데모** | `integration/DEMO.html` | 브라우저: 자연어 → 큐 → MLFQ 애니메이션 |
| **터미널 데모** | `integration/host/nl_demo.py` | 셸: 자연어 → 큐 → MLFQ 시뮬레이션 |
| **회귀 자동화** | `integration/sanity_check.sh` | build + boot + 8 마커 검증 (8/8) |

---

## 9. Quick Start

> 통합 트리 빌드·실행의 상세 절차는 **`integration/README.md`** 를 참조하세요. 요약:

```bash
# 1) 통합 xv6 빌드 (WSL2 Ubuntu / Linux + riscv 툴체인 + qemu)
cd integration/xv6-riscv && make clean && make

# 2) 4팀 슬라이스 라이브 데모 (단일 QEMU 세션)
make qemu CPUS=2
#   $ ps  /  tracetool dump  /  nlrun 2 cpu_burner 50000  /  threadtest  /  bgq bgq.txt

# 3) 한 줄 회귀 검증
XV6_DIR=integration/xv6-riscv bash integration/sanity_check.sh        # → 8/8 PASS

# 4) 자연어 → OS 데모 (호스트, 키 없어도 휴리스틱으로 동작)
cd integration/host
python3 nl_shell.py --once "Run a heavy job in background"   # → nlrun 2 cpu_burner ...
python3 nl_demo.py  --once "밤새 돌려도 되는 통계 집계"        # → 큐 번역 + MLFQ 애니메이션
```

---

## 10. 설계 메모

**LLM이 커널에 안 들어오는 이유** — Solar Pro 3는 JSON을 반환하지만, 거기서 추출하는 건
큐 레벨 정수 하나뿐. 이 정수는 `forkpri()` syscall 인자로 전달되며 `0..2` 밖이면 거부됩니다.
LLM 응답에 어떤 텍스트가 섞여도 커널엔 정수 한 개만 도달합니다.

**폴백이 단순 휴리스틱인 이유** — `nl_shell.py` 폴백은 *"heavy/background/interactive"* 같은
키워드만 봅니다. 의도적으로 결정론적 — 네트워크가 끊겨도 **같은 입력이면 항상 같은 결과** →
발표 시연 안정화가 목적.

---

## 11. 라이선스 / 크레딧

- **xv6-riscv**: MIT License. 강의 제공 미수정 트리 위에 통합 패치를 적용.
- **Solar Pro 3**: Upstage. OpenAI 호환 Chat Completions 엔드포인트로 호출.
- **Team DNDN** (4인 공동): Scheduler(`hyunsung`) · Syscall(`minju`) · Thread(`jinhwan`) · Process(`haneol`).
