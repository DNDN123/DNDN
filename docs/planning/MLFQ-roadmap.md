# Smart-MLFQ — 6주 상세 로드맵

> **프로젝트**: LLM이 프로세스 행동 패턴을 분석해 MLFQ 스케줄러의 우선순위를 추천하는 시스템
> **방향**: Direction B (LLM for OS)
> **팀**: 4인 / 6주 / 시스템 경험 얕은 팀
> **언어**: Python 3.11+
> **LLM**: Upstage Solar Pro 3 (API)

---

## 1. 프로젝트 개요

### 한 줄 소개
> *"우리가 직접 구현한 MLFQ 스케줄러에 LLM이 프로세스의 CPU/I/O 패턴을 분석해 우선순위 큐 레벨을 추천하는 시스템. 기본 MLFQ vs LLM 결합 MLFQ의 turnaround/response time/fairness를 비교 평가."*

### 왜 이 주제인가
- ✅ Week 6–7에 이미 배운 MLFQ 알고리즘 활용 → 자신감
- ✅ 시뮬레이터 직접 구현 → *thin wrapper* 함정 자동 회피
- ✅ 평가 지표가 강의에서 배운 것 그대로 (turnaround, response, fairness)
- ✅ 간트차트로 시각적 데모 임팩트 큼
- ✅ LLM 빠져도 시뮬레이터 단독 동작·평가 가능 (안전망)

### 활용 강의 내용
- **Week 2**: Process, PCB, 상태 전이 → 프로세스 시뮬레이터
- **Week 4**: 동기화 (필요 시) → 큐 락
- **Week 6**: FCFS/SJF/Priority/RR → MLFQ의 구성 요소
- **Week 7**: ⭐ **MLFQ, aging, multi-processor** → 핵심 코드
- **Week 9**: 동기화 패턴 → 큐 간 이동 시 보호

---

## 2. 시스템 아키텍처

```
┌──────────────────────────────────────────────────────────┐
│  Workload Generator                                      │
│  - CPU-bound, I/O-bound, mixed, interactive 프로세스     │
│  - 도착 시각, 버스트 패턴 설정 가능                      │
└────────────────┬─────────────────────────────────────────┘
                 │ 프로세스 스트림
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Process Simulator                                       │
│  - PCB-like 클래스 (PID, state, burst history, ...)      │
│  - 상태 전이: NEW→READY→RUNNING→WAITING→TERMINATED       │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  MLFQ Scheduler Core (★ OS 본질)                         │
│  - 3-level priority queue (HIGH/MID/LOW)                 │
│  - Time slices: 2ms / 4ms / 8ms                          │
│  - Demotion: 타임슬라이스 다 씀 → 한 단계 강등            │
│  - Promotion: I/O 블록 → 그대로 / 주기적 boost           │
│  - Aging: 일정 시간 대기 → 승격                          │
└────┬───────────────────────────────────────────┬─────────┘
     │ 프로세스 행동 트레이스                    │ 결정
     ▼                                           │
┌─────────────────────────────────┐              │
│  LLM Hint Module                │              │
│  - Solar Pro 3 API              │              │
│  - 입력: 프로세스 행동 요약     │              │
│  - 출력: 추천 큐 레벨 (JSON)    │              │
└────────────┬────────────────────┘              │
             │ 추천                              │
             └─────────► (스케줄러가 반영) ──────┘
                                                 │
                                                 ▼
┌──────────────────────────────────────────────────────────┐
│  Evaluation Framework                                    │
│  - Turnaround time, Response time, Throughput            │
│  - Fairness (Jain's index)                               │
│  - 비교: baseline MLFQ vs MLFQ + LLM                     │
└────────────────┬─────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────┐
│  Visualization (Gantt chart, 큐 상태 추이, 비교 차트)    │
└──────────────────────────────────────────────────────────┘
```

---

## 3. 4인 역할 분담

| 사람 | 담당 | 핵심 산출물 | 활용 강의 |
|---|---|---|---|
| **① 스케줄러 코어** | MLFQ 알고리즘 + 큐 관리 + 프로세스 시뮬레이터 | `scheduler.py` (~400줄), `process.py` (~150줄) | W6–7 |
| **② LLM 통합** | Solar API 호출 + 프롬프트 + 응답 파싱 + 캐싱 | `llm_hint.py` (~200줄), `prompts.py` | — |
| **③ 워크로드 생성** | 프로세스 워크로드 생성기 + 트레이스 입출력 | `workload.py` (~200줄), 시나리오 파일들 | W7 평가 방법론 |
| **④ 평가·시각화·문서** | 평가 자동화 + 차트 + 보고서·발표 | `evaluator.py`, `viz.py` (~300줄), 문서 | W7 |

> **누가 ①번을 맡을지가 가장 중요합니다.** Python에 가장 익숙하고 알고리즘 구현에 자신 있는 사람.

---

## 4. 주차별 일정

### Week 09 (이번 주) — 출발

**전체 목표**: 팀 정렬 + 환경 준비 + 1페이지 proposal 초안

**팀 공통 작업**
- 팀 미팅 (1.5시간) — 주제 확정, 역할 배정, GitHub repo 생성
- 1페이지 proposal 초안 작성 (§5 템플릿 참고)
- Solar API 키 분배 + 각자 hello world 동작 확인
- Python 환경 통일 (3.11+, requirements.txt 합의)

**개인 작업** — 각자 hello world만
- 모두: `pip install openai`, Solar API 호출 1회 성공
- ①번: MLFQ 의사코드 강의 노트에서 가져와 정리
- ②번: Solar API 응답을 JSON으로 받는 첫 코드
- ③번: 가짜 프로세스 5개 만들어 출력하는 스크립트
- ④번: matplotlib로 막대그래프 1개 그려보기

**주말까지 산출물**
- [ ] GitHub repo URL (강사 제출용)
- [ ] `docs/proposal-v1.md` (1페이지)
- [ ] `requirements.txt`
- [ ] 모두 Solar API 호출 성공 확인

---

### Week 10 — 설계 + 인터페이스

**전체 목표**: 시스템 블록 다이어그램 + 모듈 인터페이스 합의

**팀 공통 작업**
- 미팅 1회 (1시간): 시스템 다이어그램 + 인터페이스 합의
- 강의 진도: **데드락 (W10 이론)** — 큐 락 설계에 활용

**개인 작업** — *빈 클래스* 구조 합의
- **①** Process / PCB 클래스 정의 + 상태 전이 함수 stub
  - `Process(pid, arrival, bursts).state, .priority, .stats`
- **②** LLM 클라이언트 + 프롬프트 인터페이스 stub
  - `LLMHinter.recommend_priority(process_history) → int`
- **③** Workload 클래스 + 시나리오 1개 (예: "CPU 80% / I/O 20% 혼합 10개 프로세스")
  - `Workload.next_process() → Process or None`
- **④** Evaluator 인터페이스 + Gantt 차트 더미 출력
  - `Evaluator.run(scheduler, workload) → metrics`

**주말까지 산출물**
- [ ] `docs/architecture.md` (블록 다이어그램 + 모듈 책임)
- [ ] `docs/interfaces.md` (모든 함수 시그니처)
- [ ] 각 모듈 빈 클래스 커밋
- [ ] CI 셋업 (lint + 빈 테스트 통과)

---

### Week 11 — Hello World (가장 중요한 주) ⚠️

**전체 목표**: end-to-end로 *"프로세스 1~2개가 MLFQ로 돌아간다"* 를 끝낸다.

**강의 진도**: paging (W11) — 큰 영향 없음, 편하게 코딩

**개인 작업**
- **①** 단일 큐 스케줄러 (Round Robin만, 시간 단위 step 진행)
- **②** Solar API → 우선순위 추천 (mock 입력으로) JSON 응답까지
- **③** 단순 워크로드 1개 (3 프로세스, 도착시각 다름)
- **④** Gantt 차트 v1 (matplotlib로 시간축 + 프로세스 색깔)

**주말까지 산출물 (이걸 못하면 비상)**
- [ ] **Hello World 시연**: 3 프로세스가 Round Robin으로 돌아가고 Gantt 차트 출력
- [ ] LLM이 *fake process_history* 받아 priority 추천 JSON 응답
- [ ] 단위 테스트 1개 이상씩

**리스크 / 대응**
- 통합 안 됨 → 인터페이스 재합의 미팅 즉시
- 막히는 모듈 → 페어 프로그래밍 (D 끝나면 A·B·C 도와주기)

---

### Week 12 — MLFQ 완성 + LLM 실연결

**전체 목표**: 진짜 MLFQ 동작 + LLM 추천 실제 반영 + 첫 비교 실험

**강의 진도**: virtual memory, page replacement (W12) — 직접 영향 없음

**개인 작업**
- **①** 단일 큐 → **3-level MLFQ** 확장
  - Demotion (타임슬라이스 소진), Promotion (I/O 블록), 주기적 boost
- **②** LLM 추천을 실제 스케줄링 결정에 반영
  - 새 프로세스 등장 시 LLM에 *"이 프로세스의 초기 큐는?"* 질문
  - 응답을 큐 배치에 사용
- **③** 다양한 워크로드 시나리오 3종 추가
  - CPU-bound 80%, I/O-bound 80%, 혼합형
- **④** **첫 비교 실험**: baseline MLFQ vs MLFQ+LLM
  - turnaround time 비교 차트 1장

**주말까지 산출물**
- [ ] **3-level MLFQ 시연**: 프로세스 5~10개가 큐를 오가며 동작
- [ ] LLM 추천 ON/OFF 토글로 비교 가능
- [ ] 첫 비교 결과 차트

**리스크 / 대응**
- MLFQ 동작이 이상함 → 강의 노트 의사코드 다시 보면서 검증
- LLM 응답이 일관성 없음 → Temperature 0.0으로 낮추고 few-shot 예제 추가

---

### Week 13 — 평가 실험 + dry-run

**전체 목표**: 객관적 평가 결과 + 발표 한 번 통째로 돌려본다

**강의 진도**: 파일시스템 (W13) — 직접 영향 없음

**개인 작업**
- **①** 코드 리팩터링 + 주석 보강 + 엣지 케이스 처리
- **②** LLM 프롬프트 튜닝 (few-shot 예제 보강, 캐싱 추가)
- **③** 평가용 대규모 시나리오 (100+ 프로세스, 다양한 패턴) 자동 생성
- **④** **본 평가 실험 자동화**
  - 시나리오 × 알고리즘 (baseline / LLM-aug) × 반복 → 결과 차트 다수
  - 메트릭: turnaround, response, throughput, fairness, LLM 호출 횟수

**팀 공통**
- ⭐ **수요일 dry-run 1회**: 발표 시간 재면서 통째로
- 보고서 초안 작성 (각자 자기 모듈 섹션)
- 데모 백업 영상 1개 녹화

**주말까지 산출물**
- [ ] 평가 결과 차트 (최소 3장)
- [ ] 발표 슬라이드 초안 (영문)
- [ ] **데모 백업 영상**
- [ ] 기술보고서 초안 (모듈별 1쪽씩)
- [ ] 개발 프로세스 문서 초안

---

### Week 14 — 최종 발표

**개인 작업**
- 월/화: 슬라이드·시연 최종 수정
- 수: 발표 리허설 2회 이상
- 발표 당일: 백업 영상·슬라이드 USB 준비

**필수 제출물**
- [ ] 1. Application — 동작 코드 (GitHub)
- [ ] 2. 기술보고서 — 아키텍처, OS 개념 매핑, 한계
- [ ] 3. 개발 프로세스 문서 — 계획·일정·이슈·회고
- [ ] 4. 발표 슬라이드 (영문)

---

## 5. 1페이지 Proposal 템플릿 (Week 9 작성용)

```markdown
# Smart-MLFQ: LLM-Guided Multi-Level Feedback Queue Scheduler

## Direction
Direction B — LLM for OS

## Problem
Classical MLFQ uses fixed heuristics (e.g., demote after timeslice expiry,
periodic boost) that may not adapt well to diverse workloads. Process
behavior patterns (CPU/I/O ratio, burstiness, periodicity) often contain
information that fixed heuristics ignore.

## Our Approach
We implement a 3-level MLFQ scheduler in Python and integrate Upstage
Solar Pro 3 as a hint provider. The LLM analyzes recent process
behavior summaries and recommends initial priority assignments
and adjustment hints. We compare baseline MLFQ vs LLM-augmented MLFQ
on a suite of workloads.

## System Diagram
[블록 다이어그램 — 워크로드 → 시뮬레이터 → 스케줄러 ↔ LLM → 평가]

## OS Concepts in Use
- Process model & state transitions (Week 2)
- Scheduling algorithms: FCFS, RR, MLFQ, aging (Week 6–7)
- Synchronization for queue access (Week 4, 9)

## Evaluation Metrics
- Turnaround time
- Response time
- Throughput (jobs/sec)
- Fairness (Jain's index)
- LLM hint accuracy (recommendations matching optimal post-hoc placement)

## Workloads
- CPU-bound (80% CPU bursts)
- I/O-bound (80% I/O bursts)
- Mixed / interactive
- Realistic (mix of all)

## Tech Stack
Python 3.11, OpenAI SDK (Solar API), matplotlib, pytest

## Team
Team [이름], lead [이름]
- ① [이름] — Scheduler core
- ② [이름] — LLM integration
- ③ [이름] — Workload generation
- ④ [이름] — Evaluation, viz, docs

## Deliverables (by Week 14)
1. Public GitHub repo with full code & demo
2. Technical report (architecture, OS concepts, limitations)
3. Development process document
4. Final presentation slides (English)
```

---

## 6. 코드 구조 (참고용)

```
smart-mlfq/
├── README.md
├── requirements.txt
├── docs/
│   ├── proposal-v1.md
│   ├── architecture.md
│   └── interfaces.md
├── src/
│   ├── process.py          # ① PCB-like 클래스
│   ├── scheduler.py        # ① MLFQ 코어
│   ├── llm_hint.py         # ② LLM 통합
│   ├── prompts.py          # ② 프롬프트 템플릿
│   ├── workload.py         # ③ 워크로드 생성기
│   ├── evaluator.py        # ④ 평가 자동화
│   ├── viz.py              # ④ 시각화
│   └── main.py             # 통합 entry point
├── workloads/              # ③ 시나리오 파일들
│   ├── cpu_bound.json
│   ├── io_bound.json
│   └── mixed.json
├── results/                # ④ 실험 결과
│   ├── baseline/
│   └── llm_augmented/
├── tests/
│   └── test_*.py
└── scripts/
    └── run_experiments.py
```

---

## 7. 핵심 LLM 프롬프트 예시 (참고)

```python
# prompts.py
PRIORITY_RECOMMENDATION_PROMPT = """
You are a CPU scheduling advisor. Given a process's recent behavior,
recommend its priority queue level in a 3-level MLFQ scheduler.

Levels: HIGH (interactive, short bursts), MID (mixed), LOW (CPU-bound, long bursts)

Examples:
- Bursts: [CPU 1ms, I/O 50ms, CPU 1ms, I/O 50ms] → HIGH (interactive pattern)
- Bursts: [CPU 100ms, CPU 200ms, CPU 150ms] → LOW (CPU-bound)
- Bursts: [CPU 5ms, I/O 5ms, CPU 10ms, I/O 5ms] → MID (mixed)

Process to classify:
PID: {pid}
Recent bursts (ms): {burst_history}
CPU/I/O ratio: {cpu_io_ratio}

Respond ONLY with JSON:
{{"recommended_level": "HIGH" | "MID" | "LOW", "confidence": 0.0-1.0, "reasoning": "<short>"}}
"""
```

---

## 8. 매주 일요일 자가진단

매주 일요일 밤에 팀이 같이 답:

- [ ] 이번 주 산출물이 git에 커밋됐나?
- [ ] 막힌 사람이 있는데 도움 요청 안 한 사람 있나?
- [ ] 다음 주 작업이 모두에게 명확한가?
- [ ] **"발표가 다음 주라면 무엇을 보여줄 수 있는가?"**

마지막 질문이 가장 중요. 빈 칸이 있으면 비상등.

---

## 9. 가장 중요한 한 가지

⚠️ **Week 11 끝의 Hello World 시연 (3 프로세스 RR + Gantt + LLM mock)** 이 분기점입니다.

- 이걸 끝내면 → 나머지 3주는 살 붙이기
- 못 끝내면 → 즉시 비상 (스코프 축소: 큐 1개로 후퇴 / 또는 LLM 통합 stretch goal로 미루기)

**Week 11 일요일 밤에 *"우리 Hello World 됐어?"* 한 번만 정직하게 답하면 그 뒤 4주는 평탄합니다.**

---

## 10. 비상 대응 — Week 11 못 끝냈을 때

1. **스코프 축소**: 3-level MLFQ → 단일 우선순위 큐 + LLM이 우선순위만 결정. *"MLFQ"* 는 stretch goal로 후퇴.
2. **모듈 분리 발표**: 각 모듈을 단독 데모하는 형식으로 전환.
3. **LLM 통합 후순위**: 시뮬레이터 + 평가만으로 baseline 분석을 깊이 — *"LLM은 안 했지만 MLFQ를 정밀하게 분석했다"* 도 가능 (단, 평가 약해짐).

---

## 마무리

이 로드맵은 *"잘 만들기"* 가 아니라 *"무사히 완성하기"* 에 최적화. 핵심 룰:

1. Week 11까지 무조건 Hello World
2. 매주 일요일 자가진단
3. 막히면 즉시 페어 프로그래밍
4. 인터페이스 합의는 Week 10에 끝내고 변경 최소화
5. 평가 = OS 메커니즘 성능 (turnaround, response, fairness) — LLM 응답 품질이 평가 대상이 아님

이 5개만 지키면 약한 팀도 6주 안에 완주 가능합니다.
