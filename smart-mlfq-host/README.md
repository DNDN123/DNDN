# Smart-MLFQ Host Python — LLM 통합 도구

> xv6 측 MLFQ 스케줄러와 Upstage **Solar Pro 3** LLM 사이를 잇는 호스트
> 측 Python 도구 모음. 두 가지 LLM 활용 모드를 함께 지원합니다:
>
> 1. **자연어 → 실행 모드 (W11)** — 사용자의 평범한 문장을 Solar가 해석해서
>    xv6용 명령 + 큐 힌트로 변환 (`nl_shell.py` + `nlrun.c`).
> 2. **트레이스 → 힌트 모드 (W10)** — 과거 실행 통계를 Solar가 분석해서
>    프로그램별 초기 큐 레벨을 추천 (`llm_hint.py` + `wrunner.c`).

---

## 디렉토리 구조

```
smart-mlfq-host/
├── README.md              ← 이 파일
├── requirements.txt       ← Python 의존성
├── .env.example           ← API 키 + 모델 + reasoning_effort 템플릿
├── WORKFLOW.md            ← W10 트레이스→힌트 단계별 가이드
├── DEMO_W11.md            ← W11 자연어→실행 데모 runbook
│
├── hello_solar.py         ← Solar API 키 검증
├── prompts.py             ← LLM 프롬프트 (2개: PRIORITY_… , NL_TO_SPEC_…)
│
├── parse_trace.py         ← xv6 콘솔 로그 → JSON
├── llm_hint.py            ← W10 모드: 트레이스 통계 → hints.txt
├── nl_shell.py            ← W11 모드: 자연어 REPL → xv6 명령
│
├── evaluator.py           ← baseline vs LLM 메트릭 (turnaround·response·throughput·Jain)
├── viz.py                 ← 4종 차트 (avg, per_pid, gantt × 2)
│
└── docs/
    ├── architecture-scheduler.md   ← 블록 다이어그램 + OS 개념 매핑
    └── W11-slides.md               ← W11 발표용 영문 슬라이드 10장
```

---

## 빠른 시작

```bash
# 1) Python 환경
cd smart-mlfq-host
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# 2) API 키 설정
cp .env.example .env
nano .env       # UPSTAGE_API_KEY 입력

# 3) Solar API 동작 확인
python3 hello_solar.py
# → response: 'ok'  가 뜨면 OK
```

API 키 없이도 두 모드 모두 동작합니다 — **자동으로 키워드 휴리스틱으로
폴백**합니다.

---

## 두 가지 모드, 어떻게 다른가

### 모드 1 — 자연어 → 실행 (W11)

사용자가 평범한 문장을 입력 → Solar Pro 3가 JSON spec 생성 → xv6의
`nlrun` 사용자 프로그램이 `forkpri()`로 그 큐 레벨에서 실행.

```bash
$ python nl_shell.py --once "Run a heavy computation in background"

  source     : solar
  program    : cpu_burner 2000000
  queue_hint : 2 (LOW)
  reason     : Heavy CPU work, user wants it backgrounded
  xv6 cmd    : $ nlrun 2 cpu_burner 2000000
```

위 마지막 줄(`xv6 cmd`)을 xv6 QEMU 콘솔에 그대로 입력하면 됩니다.
TRACE 라인이 `pri=2`로 찍히면 MLFQ가 힌트를 받아들였다는 증거.

자세한 흐름은 [`DEMO_W11.md`](DEMO_W11.md) 참고.

### 모드 2 — 트레이스 → 힌트 (W10)

baseline 실행 → 트레이스 캡처 → 프로그램별 평균 통계 집계 →
Solar에게 *"이 프로그램은 어느 큐로 시작해야 하나"* 질의 → `hints.txt`
생성 → xv6를 두 번째로 부팅해서 `wrunner workload.txt hints.txt` 로
적용.

```
baseline.log → parse_trace.py → baseline.json
                                       │
                                  llm_hint.py (Solar 또는 휴리스틱)
                                       │
                                   hints.txt
                                       │
                                       ▼
                            wrunner workload.txt hints.txt
                                       │
                                       ▼
                                    llm.log
                                       │
                                  parse_trace.py
                                       │
                                       ▼
                                    llm.json
                                       │
                                       ▼
                          evaluator.py  +  viz.py
                                       │
                                       ▼
                              metrics.json + charts/
```

8단계 상세 절차는 [`WORKFLOW.md`](WORKFLOW.md) 참고.

---

## 두 모드의 공통 부품

| 부품 | 두 모드 모두 사용 |
|---|---|
| `prompts.py` | 두 프롬프트 (`PRIORITY_RECOMMENDATION_PROMPT_FEWSHOT`, `NL_TO_SPEC_PROMPT`) 보유 |
| `parse_trace.py` | xv6 콘솔 출력에서 TRACE/EXIT 라인만 골라 JSON으로 |
| `evaluator.py` | 두 트레이스 비교 (앞으로 3-way도 지원 예정) |
| `viz.py` | Gantt + 막대 + 라인 차트 |
| Solar 클라이언트 (`.env`) | `UPSTAGE_MODEL=solar-pro3`, `UPSTAGE_REASONING_EFFORT=low` 기본값 |

`SKIP_NAMES = {"wrunner", "nlrun", "sh", "init"}` — 두 모드 모두에서
프레임워크 프로세스는 통계·차트에서 제외됩니다.

---

## 환경 변수 (`.env`)

| 변수 | 기본값 | 비고 |
|---|---|---|
| `UPSTAGE_API_KEY` | (필수) | Solar 콘솔에서 발급. 미설정 시 휴리스틱 폴백. |
| `UPSTAGE_BASE_URL` | `https://api.upstage.ai/v1` | |
| `UPSTAGE_MODEL` | `solar-pro3` | |
| `UPSTAGE_REASONING_EFFORT` | `low` | `low` / `medium` / `high` 중. 분류성 태스크는 `low`로 충분. |

---

## 핵심 워크플로우 한 줄 요약

```
W11 모드:  자연어  → nl_shell.py → Solar → spec JSON → xv6 nlrun → MLFQ 실행 → TRACE
W10 모드:  baseline → parse_trace → llm_hint → hints.txt → 재부팅 → 비교 차트
```
