# Solo Smart-MLFQ — 1인 6주 로드맵

> **프로젝트**: LLM이 프로세스 행동 패턴을 분석해 MLFQ 스케줄러의 우선순위를 추천
> **혼자 진행** — 4인 협업 오버헤드 없음, 스코프 솔로 친화적 조정
> **목표**: 6주 안에 *완성된* 결과물 (포트폴리오·이력서용 가능한 수준)

---

## 1. 솔로용 스코프 조정 — 무엇을 살리고 무엇을 줄일까

### 코어 (반드시 구현)
- ✅ **3-level MLFQ** (HIGH / MID / LOW)
  - Time slices: 2 / 4 / 8 ticks
  - Demotion: 타임슬라이스 소진 → 한 단계 강등
  - I/O 발생 시: 같은 큐 유지
  - 주기적 priority boost (모든 프로세스 HIGH로 복귀)
- ✅ **프로세스 시뮬레이터** (PCB-like, 상태 전이)
- ✅ **LLM 통합** (Solar API로 초기 우선순위 추천)
- ✅ **평가**: turnaround time, response time, throughput
- ✅ **Gantt 차트** + 비교 그래프

### 단순화 (작게)
- 🔻 워크로드 시나리오 **2종**으로 한정 (CPU-bound + I/O-bound)
- 🔻 LLM 호출은 *초기 우선순위 결정* 1회만 (실행 중 재추천 X)
- 🔻 평가 자동화는 *간단한 스크립트* 수준 (멀티-반복 평균, 표준편차 등은 stretch)
- 🔻 fairness 메트릭은 stretch goal

### 빼기 (스코프 outside)
- ❌ 멀티 CPU
- ❌ 웹 대시보드 (CLI 출력 + 정적 차트로 충분)
- ❌ 동시성·락 (단일 스레드 discrete-event simulation)
- ❌ 다양한 aging 전략 비교

이렇게 줄이면 솔로 6주에 무리 없이 들어갑니다.

---

## 2. 시스템 아키텍처 (단순화 버전)

```
┌──────────────────────────────────────────┐
│  Workload Generator                       │
│  - 2종 시나리오 (CPU-bound / I/O-bound)  │
│  - 프로세스 5~20개, 도착시각·버스트 정의 │
└────────────┬──────────────────────────────┘
             │
             ▼
┌──────────────────────────────────────────┐
│  Discrete-Event Simulator                 │
│  - 매 tick마다 스케줄러 호출              │
│  - 프로세스 상태 전이                     │
└────────────┬──────────────────────────────┘
             │
             ▼
┌──────────────────────────────────────────┐
│  MLFQ Scheduler Core (★)                 │
│  - 3 priority queues                      │
│  - Demotion / I/O retain / boost          │
└────┬─────────────────────────────────┬────┘
     │ 새 프로세스 도착                │ 결정
     ▼                                 │
┌─────────────────────────┐            │
│  LLM Hint Module        │            │
│  - Solar API 1회 호출   │            │
│  - 초기 큐 레벨 추천    │            │
└──────────┬──────────────┘            │
           │ 추천 우선순위              │
           └──── 큐 배치에 사용 ────────┘
                                       │
                                       ▼
┌──────────────────────────────────────────┐
│  Evaluator + Visualizer                   │
│  - 메트릭 계산                            │
│  - Gantt 차트 + 비교 그래프 (matplotlib) │
└──────────────────────────────────────────┘
```

---

## 3. 코드 구조

```
smart-mlfq/
├── README.md                  # 5분 안에 이해되는 설명 (포트폴리오 핵심)
├── requirements.txt
├── .gitignore
├── .env.example
├── src/
│   ├── process.py             # PCB-like 클래스 (~80줄)
│   ├── scheduler.py           # MLFQ 코어 (~250줄)
│   ├── workload.py            # 2종 시나리오 생성 (~120줄)
│   ├── llm_hint.py            # Solar API 통합 (~120줄)
│   ├── prompts.py             # 프롬프트 템플릿 (~50줄)
│   ├── evaluator.py           # 메트릭 계산 (~100줄)
│   ├── viz.py                 # Gantt + 비교 차트 (~150줄)
│   └── main.py                # 통합 entry point (~80줄)
├── workloads/
│   ├── cpu_bound.json
│   └── io_bound.json
├── results/                   # 실험 결과 (gitignore에 추가)
├── docs/
│   ├── proposal.md            # 1페이지 (W1)
│   ├── architecture.md        # 블록 다이어그램 + 모듈 책임 (W2)
│   └── report.md              # 최종 보고서 (W6)
├── tests/
│   └── test_*.py
└── scripts/
    └── run_experiments.py     # 실험 자동화
```

**총 분량 예상**: ~950줄 (4인 1200줄에서 일부 단순화 반영). 솔로에 알맞은 분량.

---

## 4. 주차별 일정 — 솔로 60시간 견적

### Week 1 (5시간) — 셋업 + 설계 시작

**목표**: 환경·repo 셋업, 1페이지 proposal 작성, MLFQ 의사코드 정리

**작업 시간 분배**
- repo 생성 + README 초안 (1h)
- Python 환경 + Solar API hello (1h)
- 1페이지 proposal 작성 (1h)
- MLFQ 의사코드 강의 노트 정리 → `docs/architecture.md` 초안 (2h)

**산출물**
- [ ] GitHub repo public + 초기 README
- [ ] `requirements.txt`, `.gitignore`, `.env.example`
- [ ] Solar API hello world 동작 확인
- [ ] `docs/proposal.md` (1페이지)

---

### Week 2 (10시간) — 프로세스 모델 + Workload 생성

**목표**: 프로세스 시뮬레이터 동작, 워크로드 생성 동작

**작업**
- `process.py`: PCB 클래스, 상태 전이, burst 처리 (3h)
- `workload.py`: CPU-bound + I/O-bound 시나리오 생성 (3h)
- `workloads/*.json` 저장 포맷 정의 + 샘플 생성 (1h)
- 단위 테스트 (`tests/test_process.py`, `test_workload.py`) (2h)
- 디버깅 + 정리 (1h)

**산출물**
- [ ] `python src/workload.py --scenario cpu_bound > workloads/cpu_bound.json` 동작
- [ ] 프로세스 5개 시나리오 print 가능
- [ ] 단위 테스트 5개 이상 통과

---

### Week 3 (12시간) — MLFQ 코어 구현

**목표**: 3-level MLFQ가 동작. LLM 없이 baseline.

**작업**
- 3개 priority queue 자료구조 (1h)
- 스케줄링 로직: 큐에서 꺼내기, 타임슬라이스 추적 (3h)
- Demotion 로직 (타임슬라이스 소진 시 강등) (2h)
- I/O 처리 (블록 → WAITING, 복귀 시 같은 큐) (2h)
- Priority boost (주기적으로 모두 HIGH로) (1h)
- discrete-event 메인 루프 (`main.py`) (2h)
- 디버깅 (1h)

**산출물**
- [ ] **첫 Hello World**: `python src/main.py --workload workloads/cpu_bound.json` 실행 → 프로세스 종료까지 동작
- [ ] 콘솔에 매 tick 어느 프로세스가 실행됐는지 로그 출력
- [ ] 단위 테스트 (스케줄러 핵심 동작) 통과

⚠️ **이번 주가 가장 중요**. Week 3 끝에 baseline MLFQ가 돌아가야 나머지가 흐릅니다.

---

### Week 4 (12시간) — LLM 통합 + Gantt 차트

**목표**: LLM이 초기 우선순위 추천 → 큐 배치에 반영. Gantt 차트로 시각화.

**작업**
- `llm_hint.py`: Solar API 클라이언트 + 응답 파싱 (3h)
- `prompts.py`: priority recommendation 프롬프트 (1h)
- 프로세스 도착 시점에 LLM 호출 → 초기 큐 결정 (2h)
- LLM 응답 캐싱 (동일 프로세스 패턴 재호출 방지) (1h)
- `viz.py`: Gantt 차트 (matplotlib) (3h)
- 통합 테스트 (CPU-bound, I/O-bound 두 시나리오 돌려보기) (2h)

**산출물**
- [ ] LLM 추천 ON/OFF 토글 (`--use-llm` 플래그)
- [ ] Gantt 차트 PNG 출력
- [ ] 단일 시나리오에서 baseline vs LLM 차이 *눈으로* 확인 가능

---

### Week 5 (12시간) — 평가 실험 + 분석

**목표**: 정식 비교 실험 + 차트 + 분석.

**작업**
- `evaluator.py`: turnaround / response / throughput 계산 (3h)
- `scripts/run_experiments.py`: 자동 실험 (시나리오 × baseline/LLM × 반복) (3h)
- 추가 워크로드 (interactive — 짧은 burst + 잦은 I/O) (2h)
- 비교 차트 (막대그래프, Gantt 비교) (2h)
- 결과 분석 — *"LLM이 도움이 된 시나리오는?", "안 된 시나리오는?"* (2h)

**산출물**
- [ ] 실험 결과 JSON / CSV (`results/`)
- [ ] 비교 차트 4장 이상 (시나리오별 메트릭 비교)
- [ ] `docs/report.md` 초안 — 결과 요약 + 분석

---

### Week 6 (9시간) — 마무리 + 포트폴리오용 다듬기

**목표**: README 정성껏, 데모, 코드·문서 정리.

**작업**
- 코드 리팩터링 + docstring (2h)
- README 풀버전 작성 (문제·접근·결과 차트·실행법) (2h)
- 데모 영상 1개 녹화 (2~3분) (1h)
- 블로그 글 1개 (선택) — *"MLFQ에 LLM을 결합해본 후기"* (2h)
- 최종 보고서 (`docs/report.md`) 마무리 (2h)

**산출물**
- [ ] **포트폴리오 수준 README** (스크린샷·차트 포함)
- [ ] 데모 영상 (YouTube 또는 GitHub README에 임베드)
- [ ] 최종 보고서 `docs/report.md`
- [ ] 모든 commit 정리, README 마지막 업데이트

---

## 5. 솔로 진행 시 핵심 룰 (이게 안 지켜지면 4주차에 멘붕)

### 룰 1: 매주 일요일 30분 자가진단

다음 4문항에 답:
- 이번 주 산출물이 git에 푸시됐나?
- 다음 주 작업이 명확한가?
- 어느 부분에서 막혔나? (있다면 다음 주 첫 작업으로)
- *"발표가 다음 주라면 무엇을 보여줄 수 있는가?"*

### 룰 2: 매일 30분 룰

진도가 안 나가도 *매일 최소 30분*은 코드를 본다. 일주일에 한 번 몰아서 5시간 < 매일 30분 × 7일 (절대적으로). 동기 유지에 필수.

### 룰 3: GitHub 잔디는 셀프 deadline pressure

매일 push 자체가 목적은 아니지만, *"오늘은 commit 0개로 끝나는가"* 가 시각적 알람.

### 룰 4: 막히면 24시간 룰

24시간 이상 같은 버그에 막히면:
- 다른 모듈로 잠시 전환
- AI 코딩 도구 (Claude Code, Copilot, etc.) 적극 활용
- Stack Overflow, GitHub Issues 검색
- 친구·동기에게 *말로* 설명해보기 (rubber duck)

### 룰 5: Definition of Done 책상에 붙이기

*"이 프로젝트는 X가 되면 끝이다"* 를 종이에 쓰기:
> *"3-level MLFQ가 2종 워크로드로 돌아가고, baseline vs LLM 비교 차트 4장이 README에 있고, 데모 영상이 있다. 그게 끝이다."*

이걸 책상에 붙이면 스코프 크리프 막아줍니다.

---

## 6. 가장 큰 리스크 + 대응

### 리스크 1: Week 3 (MLFQ 코어) 에서 막힘

**증상**: 큐 이동 로직이 복잡해서 디버깅이 안 잡힘

**대응**:
- 강의 노트 의사코드 그대로 옮기기 (창의력 발휘 X)
- 단위 테스트로 *"3 ticks 후에 P1이 LOW 큐에 있어야 한다"* 식의 검증
- AI 도구에 *"이 MLFQ 구현에서 demotion 로직이 잘못된 것 같다, 트레이스 보고 짚어줘"* 식으로 도움 요청

### 리스크 2: Week 4 (LLM 통합) 에서 응답 파싱 실패

**증상**: LLM이 JSON 형식을 안 지켜서 파싱 에러

**대응**:
- 프롬프트에 *"Respond ONLY with JSON, no other text"* 강조
- Few-shot 예제 3개 포함
- `try/except`로 파싱 실패 시 default 큐 (MID) fallback
- temperature=0.0 사용

### 리스크 3: Week 5 (평가) 에서 *"LLM이 별 도움 안 됨"* 결과

**증상**: baseline과 LLM의 차이가 미미

**대응**: ⭐ **이건 함정이 아니라 좋은 결과예요.**
- 솔직한 분석을 보고서에 쓰기: *"이런 워크로드에서는 효과 없음. 이유는..."*
- 부정적 결과도 학회 페이퍼 가치 있음
- *"우리 시스템이 안 된다"* 가 아니라 *"이 결합 방식의 한계"* 로 프레이밍

### 리스크 4: 4주차 매너리즘

**증상**: 동기 떨어짐, 진도 정체

**대응**:
- Week 4 시작 시 *작은 보상* 미리 약속 (영화 한 편, 외식 등)
- 친구에게 GitHub repo 보여주고 진행 상황 공유
- README의 데모 섹션을 미리 비주얼하게 작성 → 완성된 모습 상상

---

## 7. 포트폴리오 가치 극대화 팁

이 프로젝트가 *"끝까지 다듬어진"* 결과물이 되려면:

### README 5분 룰
- 처음 5분 안에 다음이 다 보여야 함:
  - 한 줄 설명
  - 시스템 다이어그램 1개
  - 결과 차트 1개
  - 실행법 (`pip install -r requirements.txt && python src/main.py`)
- 5분 안에 이해 못 하면 *어떤 면접관도 안 봄*

### 솔직한 분석 섹션
- 면접관이 가장 궁금해하는 건 *"여기서 뭘 배웠나"*
- *"LLM 추천 정확도는 X%. 안 맞은 케이스 분석:"* 같은 정직한 회고 섹션
- 한계 인정 + 다음 단계 제시 (*"향후: real-time 환경에서 LLM latency 처리 필요"*)

### 데모 영상
- 2~3분
- 코드 안 보여줘도 됨 — *결과* 만 보여주기
- *"baseline 돌렸더니 → 이런 Gantt 차트, LLM 결합 → 이런 차이"*

### 블로그 글 (선택, 강추)
- 미디엄·벨로그·티스토리 등에 글 1개
- *"MLFQ에 LLM을 결합한 결과 — 솔직한 분석"*
- 검색 유입으로 *남이 찾아오는* 콘텐츠가 됨
- 면접관이 미리 읽고 들어오는 시나리오 가능

---

## 8. 첫 단계 — 오늘 할 일

### 오늘 (1시간)
- [ ] GitHub repo 생성 (`smart-mlfq`)
- [ ] README 초기 커밋 (한 단락 설명)
- [ ] `.gitignore` (Python), `.env.example` 커밋
- [ ] 노트에 *Definition of Done* 적기

### 내일 (1시간)
- [ ] Solar API 키 신청 (개인용: https://www.upstage.ai/events/ai-initiative-2025-ko)
- [ ] `pip install openai python-dotenv matplotlib`
- [ ] `hello_solar.py` 작성 + 실행

### 이번 주말까지 (3시간)
- [ ] `docs/proposal.md` (1페이지) 작성
- [ ] `docs/architecture.md` 초안 (블록 다이어그램 손그림 사진이라도)
- [ ] MLFQ 의사코드를 강의 노트에서 옮겨 적기

이번 주 5시간 투자하면 Week 1 완료입니다.

---

## 9. 마지막 한 마디

솔로 프로젝트는 *기술 도전*보다 *지속 도전*이에요. 코드 어려움보다 *6주째에도 commit 한다는 것* 이 더 어렵습니다.

**가장 중요한 한 가지**: Week 3 끝에 baseline MLFQ가 돌아가게 만드세요. 이게 되면 나머지 3주는 흐름대로 갑니다. 안 되면 Week 4에 시작점 다시 잡고, 그래도 안 되면 *2-level 다운스케일* 로 후퇴 (HIGH/LOW 두 큐만).

매주 일요일 *"발표가 다음 주라면 뭘 보여줄까"* 자가진단만 정직하게 하면, 6주 끝에 진짜 결과물 손에 들고 있을 겁니다.
