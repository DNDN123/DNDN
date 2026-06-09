# DNDN Project — 우리가 만든 xv6 위에서 자연어로 움직이는 OS

> 4인 팀이 함께 xv6-riscv 커널의 4개 슬라이스(**Scheduler · Syscall · Thread · Process**)를
> 직접 구현해 **하나의 빌드 트리로 통합**하고, 그 위에 **xv6 콘솔 안에서 바로 자연어로
> OS를 조작하는 NL-shell**을 얹었다. 번역 두뇌는 **클라우드(Solar Pro 3)와 우리가
> 파인튜닝한 온디바이스 모델(Qwen-3B LoRA)을 같은 인터페이스로 교체**할 수 있고,
> **LLM은 *조언만* 하고 결정·실행·교정은 *커널이* 내린다.**
>
> **Team Project · Direction B (LLM for OS) · 2026 Spring** · GitHub: `DNDN123/DNDN`

### 한 줄 요약
> **"우리가 만든 xv6 위에서, LLM은 *조언만* 하고 결정은 *커널이* 한다 — 자연어로 움직이는 OS."**
>
> LLM은 절대 커널 안에 들어오지 않는다. LLM은 *힌트* 만 주고, 실제 결정·실행·교정은
> 우리가 직접 구현·통합한 xv6 커널(스케줄러·시스템콜·스레드·프로세스)이 내린다.

### 최종 발표 — NL-shell 확장 (이번 통합의 핵심)
> 기존엔 **호스트(PC) 셸**에서만 자연어를 받았다. 최종본은 한 걸음 더 나아가
> **xv6 게스트 콘솔 안에서 직접** 평범한 한국어/영어를 입력하면 커널 명령으로 번역·실행된다:
>
> ```
> $ 무거운 프로세스 정리해줘
> exec 무거운 failed              ← xv6 sh: 실제 명령이 아님
> [bridge] (자연어로 해석) ...     ← 브리지가 그 줄을 다시 해석
> [bridge] -> killheavy           ← 번역 후 같은 콘솔에 주입, xv6가 실행
> ```
>
> - **실제 명령(`ps`, `ls`, `kill 7`)은 가로채지 않고** 그대로 빠르게 실행. xv6가 exec에
>   실패한 줄(첫 단어가 프로그램이 아닌 줄)만 자연어로 간주.
> - 번역 백엔드는 **온디바이스 파인튜닝 모델 → Solar → 오프라인 규칙** 순으로 자동 선택
>   (`nlos.py` 한 줄 실행). 어떤 백엔드든 **브리지·가드·커널·xv6 명령은 동일**.
> - LLM은 여전히 커널 밖. 콘솔 경계를 넘는 건 **번역된 짧은 명령 한 줄뿐.**

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
> 이 저장소의 `integration/` 입니다.

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
3. **LLM 없이도, 클라우드 없이도 동작** — 1순위는 **우리가 파인튜닝한 온디바이스 모델**(로컬
   추론, Solar 불필요), 없으면 Solar, 그것도 없으면 결정론적 키워드 휴리스틱으로 폴백 →
   네트워크·키가 끊겨도 데모가 안 깨짐. (온디바이스 모델 상세는 §5.)

---

## 4. 시스템 구조 / 데이터 흐름

```
[유저 자연어]  "무거운 작업 백그라운드로 돌려줘"   ──┐ 두 진입점, 같은 파이프라인
      │ (A) 호스트 셸: nlos.py / nl_shell.py     │
      │ (B) xv6 콘솔 안에서 직접 입력 → nlbridge  │
      ▼   번역 두뇌 (백엔드 교체 가능)            │
  온디바이스 Qwen-3B LoRA  ─또는─  Solar Pro 3  ─또는─  오프라인 규칙
      │  → JSON spec {cmd,args,queue_hint,reason} → SafetyGuard(안전가드) → 명령 문자열
      ▼   "nlrun 2 cpu_burner 1000000"
════════ syscall/콘솔 경계 — 여기서 LLM 격리 (번역된 명령 한 줄만 통과) ════════
      ▼   xv6 KERNEL  (4슬라이스가 한 트리에서 맞물림)
  forkpri(2) ─► 3-단계 MLFQ가 실제 스케줄링      … Scheduler (잘못된 힌트는 demotion이 자동 교정)
      │
      ├─ thread_create / futex_wait·wake        … Thread   (프로세스 내 동시 실행·동기화)
      ├─ trace_on / trace_stats                 … Syscall  (실행된 syscall을 per-pid 기록)
      ├─ ps / sysinfo                           … Process  (스케줄링 결과를 외부에서 관찰)
      └─ reap / killheavy / killall             … NL-shell (정리·제어 요청을 안전 가드로 실행)
```

> **NL-shell 의도(intent) 20종** — 워크로드(cpu/io/mixed) · 파일(ls/cat/rm/mkdir/ln/echo) ·
> 프로세스(ps/kill/setpri/trace) · 정리(killall/killheavy/reap) · 시스템(uptime/sysinfo) ·
> 정보(explain) · 거부(reject). `SafetyGuard`가 pid 0/1 종료를 차단하고 파괴적 명령은
> 확인을 요구합니다. `explain`/`reject`는 OS를 건드리지 않습니다.

---

## 5. 온디바이스 LLM — 우리가 직접 만들어 Solar를 대체했다

이번 최종본의 가장 큰 진전은 **번역 두뇌를 클라우드(Solar Pro 3)에서 우리가 직접
파인튜닝한 온디바이스 모델로 갈아끼웠다**는 것입니다. 더 이상 외부 API·네트워크·키가
없어도 자연어→OS 번역이 **PC 안에서 완전히 로컬로** 돌아갑니다.

### 왜 직접 만들었나
- **"LLM 껍데기"를 벗어나기 위해** — Solar에 매번 물어보는 구조는 결국 외부 API 래퍼입니다.
  우리 작업의 핵심(자연어→인텐트 번역)을 **우리 모델 안으로 내재화**해야 진짜 우리 시스템이 됩니다.
- **재현성·오프라인** — 발표장 네트워크/키에 의존하지 않고, **같은 입력이면 같은 출력**.
- **비용 0 / 지연 예측 가능** — 호출당 과금 없이, 로컬 GPU(또는 Apple MPS/CPU)에서 동작.

### 어떻게 만들었나 — Distillation 파이프라인
> **"Solar에게 배워서, Solar를 대체한다."** 클라우드 LLM(Solar·Claude)을 *교사*로 써서
> 데이터를 만들고, 그 지식을 작은 학생 모델에 증류(distill)해 클라우드를 떼어냈습니다.

```
시드 인텐트(gen_intent_seeds) ─► 증강(augment, Solar/Claude로 ×10 변형)
  ─► 누수 0 그룹 split(build_train) ─► LoRA fine-tune(train) ─► 평가/비교(evaluate·compare)
```

| 항목 | 값 |
|---|---|
| **베이스 모델** | `Qwen/Qwen2.5-3B-Instruct` (3B, 학생 모델) |
| **파인튜닝 방식** | LoRA — `r=64`, `alpha=64`, `dropout=0.05`, 10 epochs |
| **타깃 모듈** | q/k/v/o · gate/up/down_proj (attn+MLP 전체) |
| **학습 데이터** | 20 인텐트 · 한/영 ~반반 · train ≈ 7,000 / test 1,584 (그룹 split로 누수 0) |
| **데이터 출처** | 클라우드 LLM(Solar/Claude) 증류 + 결정론적 오프라인 증강 |
| **학습 환경** | RTX 5070 Ti (Blackwell, CUDA 12.8) |
| **서빙** | `model_server.py` — 학습한 LoRA를 **OpenAI 호환 `/v1` 엔드포인트**로 로컬 서빙 (CUDA/MPS/CPU) |

### 성능 — 직접 만든 모델이 실제로 쓸 만한가
홀드아웃 테스트셋(n=1,584)에서:

| 지표 | 우리 온디바이스 모델 |
|---|---|
| Valid JSON | **100.0%** |
| cmd 정확도 | **97.0%** |
| queue_hint 정확도 | **99.3%** |
| cmd+queue 둘 다 정답 | **96.5%** |
| 지연(p50 / p95) | 2.0s / 2.6s |
| 언어별 cmd 정확도 | EN 97.4% · KO 96.7% |

> 18/20 인텐트가 ~100%. JSON은 항상 유효해 파서가 깨지지 않고, 큐 힌트는 99%대로
> 스케줄러에 안정적으로 전달됩니다. (소프트스팟: `explain`·`mixed_burner` — 데이터 추가로 개선 가능.)

### 드롭인 교체 — 시스템 나머지는 1줄도 안 바뀐다
온디바이스 모델을 **Solar와 똑같은 OpenAI 호환 인터페이스**로 서빙했기 때문에,
백엔드 전환은 환경변수(`UPSTAGE_BASE_URL` / `UPSTAGE_MODEL`)만 바꾸면 됩니다.
**브리지·안전가드·커널·xv6 명령은 전부 동일** — 즉 "Solar를 우리 모델로 갈아끼웠다"는
말이 코드 구조 그대로입니다.

```bash
# 터미널 1 — 우리가 학습한 모델을 로컬 서빙 (Solar 없이)
cd integration/host && python3 model_server.py        # → http://127.0.0.1:11434/v1

# 터미널 2 — 같은 브리지를, 이번엔 로컬 모델로
cd integration/host && python3 nlos.py --backend local
#   (또는 nlos.py 가 로컬 모델을 감지하면 자동으로 이 경로를 택함)
```

> ⚠️ 학습된 가중치(`ml/models/.../lora`, 베이스까지 ~수 GB)는 **저장소에 커밋하지 않습니다**
> (`.gitignore`). 재현은 위 `ml/scripts` 파이프라인으로 학습하거나, 가중치를 별도 배포로 받습니다.

---

## 6. Syscall 번호 최종 분배 (통합 결과)

| 번호 | 슬라이스 | 추가된 syscall |
|---|---|---|
| 1~21 | stock xv6 | (미수정) |
| 22~24 | `minju` | `trace_on` / `trace_off` / `trace_stats` |
| 25~29 | `jinhwan` | `thread_create/join/exit` + `futex_wait/wake` |
| 30~33 | `hyunsung` | `setpri` / `getstats` / `settrace` / `forkpri` |
| 34~35 | `haneol` | `ps` / `sysinfo` |
| 36 | NL-shell | `reap` (버려진 좀비를 init으로 reparent해 안전 정리) |

> 4팀이 모두 22~25 영역을 쓰려 한 게 가장 큰 충돌이었고, 위 표로 합의해 15개 syscall이
> 충돌 없이 공존합니다. `reap`(36)은 NL-shell의 `정리해줘` 류 요청을 받쳐주는 syscall로,
> `freeproc()`를 직접 부르지 않고 init의 검증된 `wait()` 경로로만 좀비를 회수합니다.
> 상세 결정 근거는 `integration/MERGE_NOTES.md`.

---

## 7. 최종 결과 — 빌드 · 검증 (모두 통과)

| 항목 | 상태 | 근거 |
|---|---|---|
| 빌드 / 부팅 | ✅ PASS | WSL + QEMU, 단일 트리 |
| 라이브 데모 · **Scheduler** | ✅ PASS | `nlrun 2 cpu_burner` → 3-단계 MLFQ 강등 관찰 + `getstats` |
| 라이브 데모 · **Syscall** | ✅ PASS | `trace_on` 후 `tracetool dump` → per-pid syscall JSON |
| 라이브 데모 · **Thread** | ✅ PASS | `threadtest` → kthread 생성·join + futex mutex/condvar |
| 라이브 데모 · **Process** | ✅ PASS | `ps` / `sysinfo` 로 실행 중 프로세스·자원 관찰, `bgq` |
| 라이브 데모 · **NL-shell** | ✅ PASS | xv6 콘솔에서 `무거운 프로세스 정리해줘` → `killheavy`, `reap` 회수 |
| 정량 평가 (baseline/heuristic/Solar) | ✅ PASS | `three_way` avg_turnaround **−7.3% / −5.8%** |
| **온디바이스 모델** 정확도 | ✅ PASS | test n=1,584: cmd **97.0%** / queue **99.3%** / JSON **100%** (Solar 대체) |
| 자동 회귀 (`sanity_check.sh`) | ✅ **8/8** | 2026-06-03 fresh ext4 재실행, exit 0 |
| 통합·보안 수정 | ✅ | `K1~K7` (allocproc 누수, 배열 오버플로, thread teardown race, prompt-injection 가드 등) |

---

## 8. 저장소 구조

```
.
├── README.md                  ← 이 파일 (프로젝트 개요)
├── integration/               ← ⭐ 4팀 통합 결과 (메인 산출물)
│   ├── xv6-riscv/             ← 통합 커널 + user + workloads (빌드 대상)
│   │   ├── kernel/            ← MLFQ + trace + thread/futex + ps + reap 전부
│   │   ├── user/              ← nlrun, wrunner, threadtest, tracetool, ps, bgq …
│   │   │                         + NL-shell 도구: ask, killall, killheavy, reap,
│   │   │                           tracepid, uptime, sysinfo
│   │   └── workloads/         ← cpu_heavy / io_heavy / mixed / three_way / realprog / bgq …
│   ├── host/                  ← 호스트 Python
│   │   ├── nlos.py            ← ⭐ NL-shell 런처 (백엔드 자동 선택: local→solar→offline)
│   │   ├── nlbridge.py        ← ⭐ xv6 콘솔 ↔ 자연어 브리지 (QEMU 콘솔 미러링·주입)
│   │   ├── executor.py        ← intent 분류 + build_command + SafetyGuard (20종)
│   │   ├── model_server.py    ← 온디바이스 Qwen-3B LoRA 추론 서버 (OpenAI 호환)
│   │   ├── qemu_probe.py      ← QEMU 부팅·프롬프트 감지 헬퍼
│   │   ├── nl_shell.py        ← 호스트 셸 진입점 (자연어 → 큐 레벨)
│   │   ├── nl_demo.py         ← 터미널 데모 (자연어 → MLFQ 시뮬레이션)
│   │   ├── adapters/          ← process / thread Intent 브리지
│   │   └── test_nlos.py       ← NL-shell 단위 테스트
│   ├── ml/                     ← 온디바이스 모델 학습·서빙 자산
│   │   ├── scripts/            ← 증류 파이프라인: gen_intent_seeds → augment → build_train → train → evaluate/compare
│   │   ├── data/               ← seeds / augmented / train.jsonl / test.jsonl (그룹 split, 누수 0)
│   │   └── models/            ← 학습된 Qwen-3B LoRA 가중치 (~472MB, .gitignore — 커밋 안 함)
│   ├── MERGE_NOTES.md         ← 충돌 결정 / K1~K7 fix 매핑
│   ├── sanity_check.sh        ← 한 줄 자동 회귀 (build+boot+8 마커)
│   ├── REPORT.html / _EN      ← 보고서 (한/영)
│   ├── SLIDES.html / _EN      ← 발표 슬라이드 15장 (한/영)
│   ├── SLIDES_NLSHELL.html    ← ⭐ 최종 발표 — NL-shell 확장 통합 덱
│   ├── DEMO.html              ← 브라우저 인터랙티브 데모
│   ├── demo.gif               ← 실제 QEMU 세션 캡처
│   └── chat_demo.gif          ← 콘솔 내부 자연어 대화 세션 캡처
└── docs/                      ← 설계 문서 + 정량 평가 차트
    ├── syscall-allocation.md / trace-format.md / hints-format.md
    ├── hello-world.md / integration-checklist.md / security-policy.md
    └── charts/{standalone,integration}/   ← 워크로드별 평가 결과
```

---

## 9. 산출물 (Deliverables)

| 산출물 | 위치 | 설명 |
|---|---|---|
| **통합 트리** | `integration/xv6-riscv/` | 빌드·부팅 가능한 단일 xv6 (NL-shell 도구 포함) |
| **NL-shell 런처** | `integration/host/nlos.py` | ⭐ xv6 콘솔에서 자연어로 OS 조작 (백엔드 자동 선택) |
| **온디바이스 모델** | Qwen-3B LoRA (`ml/models/...`, 로컬 전용) | 파인튜닝 번역 모델, Solar와 교체 가능 |
| **보고서** | `integration/REPORT.html`, `REPORT_EN.html` | 한/영 기술 보고서 |
| **슬라이드** | `integration/SLIDES.html` / `_EN` / `SLIDES_NLSHELL.html` | 한/영 발표 덱 + 최종 NL-shell 확장 덱 |
| **데모 GIF** | `integration/demo.gif`, `chat_demo.gif` | QEMU 세션 (4슬라이스) + 콘솔 내부 자연어 대화 |
| **인터랙티브 데모** | `integration/DEMO.html` | 브라우저: 자연어 → 큐 → MLFQ 애니메이션 |
| **터미널 데모** | `integration/host/nl_demo.py` | 셸: 자연어 → 큐 → MLFQ 시뮬레이션 |
| **회귀 자동화** | `integration/sanity_check.sh` | build + boot + 8 마커 검증 (8/8) |

---

## 10. Quick Start

> 통합 트리 빌드·실행의 상세 절차는 **`integration/README.md`** 를 참조하세요. 요약:

```bash
# 1) 통합 xv6 빌드 (WSL2 Ubuntu / Linux + riscv 툴체인 + qemu)
cd integration/xv6-riscv && make clean && make

# 2) 4팀 슬라이스 라이브 데모 (단일 QEMU 세션)
make qemu CPUS=2
#   $ ps  /  tracetool dump  /  nlrun 2 cpu_burner 50000  /  threadtest  /  bgq bgq.txt

# 3) 한 줄 회귀 검증
XV6_DIR=integration/xv6-riscv bash integration/sanity_check.sh        # → 8/8 PASS

# 4) ⭐ NL-shell — xv6 콘솔 안에서 직접 자연어로 OS 조작 (백엔드 자동 선택)
cd integration/host
python3 nlos.py                    # 로컬 모델 있으면 그걸로, 없으면 Solar, 둘 다 없으면 오프라인 규칙
python3 nlos.py --backend local    # 온디바이스 파인튜닝 모델 강제 (Qwen-3B LoRA)
python3 nlos.py --backend solar    # Solar Pro 3 강제 (UPSTAGE_API_KEY 필요)
python3 nlos.py --backend offline  # LLM 없이 결정론적 규칙 (키/모델 없는 시연용)
#   xv6 콘솔에서:  무거운 프로세스 정리해줘   → killheavy
#                  7번 프로세스 꺼줘          → kill 7

# 5) 자연어 → OS 데모 (호스트 셸, 키 없어도 휴리스틱으로 동작)
python3 nl_shell.py --once "Run a heavy job in background"   # → nlrun 2 cpu_burner ...
python3 nl_demo.py  --once "밤새 돌려도 되는 통계 집계"        # → 큐 번역 + MLFQ 애니메이션
```

---

## 11. 설계 메모

**LLM이 커널에 안 들어오는 이유** — 온디바이스 모델(또는 Solar)은 JSON을 반환하지만, 거기서
추출하는 건 큐 레벨 정수 하나뿐. 이 정수는 `forkpri()` syscall 인자로 전달되며 `0..2` 밖이면
거부됩니다. LLM 응답에 어떤 텍스트가 섞여도 커널엔 정수 한 개만 도달합니다. **모델을 우리가
직접 만들었어도 이 격리 원칙은 그대로** — 커널은 여전히 LLM의 존재를 모릅니다.

**폴백이 단순 휴리스틱인 이유** — `nl_shell.py` 폴백은 *"heavy/background/interactive"* 같은
키워드만 봅니다. 의도적으로 결정론적 — 네트워크가 끊겨도 **같은 입력이면 항상 같은 결과** →
발표 시연 안정화가 목적.

---

## 12. 라이선스 / 크레딧

- **xv6-riscv**: MIT License. 강의 제공 미수정 트리 위에 통합 패치를 적용.
- **온디바이스 모델**: `Qwen/Qwen2.5-3B-Instruct`(Apache-2.0) 베이스에 우리가 LoRA 파인튜닝.
  학습/서빙 코드는 우리 작성(`ml/`, `host/model_server.py`). **이 모델이 기본 번역 두뇌**입니다.
- **Solar Pro 3**: Upstage. 온디바이스 모델의 *폴백* 및 학습 데이터 증류(distillation)의 *교사*로 사용.
- **Team DNDN** (4인 공동): Scheduler(`hyunsung`) · Syscall(`minju`) · Thread(`jinhwan`) · Process(`haneol`).
