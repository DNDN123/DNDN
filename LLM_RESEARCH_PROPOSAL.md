# LLM for OS — Research Proposal (Extended)

목표: 학부 OS 학기말 프로젝트를 학회 워크숍 수준으로 끌어올리기.
전략: **단계적 진행 (안전망 다중)**. 각 phase마다 독립적으로 발표 가능한 결과 확보.

이 문서는 [`LLM_TRAINING_PLAN.md`](./LLM_TRAINING_PLAN.md)의 확장판입니다.
기존 계획이 "fine-tuned 모델 만들기"였다면, 이 문서는 "그 모델로 OS를 자율 운영"까지 확장.

---

## 0. Big Picture — 2단계 전략

```
┌──────────────────────────────────────────────────────────────┐
│ Phase 1: Knowledge Distillation                              │
│ ─────────────────────────────                                │
│ Teacher (Solar/Claude) → Student (1.5B SLM)                  │
│                                                              │
│ 산출물:                                                       │
│   - 1.5B 모델 (47배 작음, 정확도 95%+)                        │
│   - 정확도/속도/비용 비교 표                                  │
│   - Adversarial robustness 측정                              │
│                                                              │
│ 발표 가능 시점: Phase 1 끝 → 이미 우수상급                    │
└──────────────────────┬───────────────────────────────────────┘
                       │ (안전망 확보)
                       ▼
┌──────────────────────────────────────────────────────────────┐
│ Phase 2: Closed-loop Autonomous OS Agent                     │
│ ────────────────────────────────────                         │
│ Phase 1의 SLM을 closed-loop agent로 사용                      │
│                                                              │
│ 산출물:                                                       │
│   - LLM Agent가 starvation 자동 감지/해결                     │
│   - Mixed workload throughput 개선 측정                       │
│   - 5가지 baseline 비교                                       │
│                                                              │
│ 발표 가능 시점: Phase 2 끝 → 학회 워크숍 수준                  │
└──────────────────────────────────────────────────────────────┘
```

### 왜 이 순서?

- Phase 1만 해도 "Solar를 47x 압축" 클레임으로 학부 우수상 보장
- Phase 2는 risk 있음 (LLM agent가 정적 룰보다 안 좋을 가능성)
- Phase 1 결과를 안전망 삼아 Phase 2 도전 가능
- Phase 2 결과가 negative여도 Phase 1으로 발표 가능

---

## 1. 가설 (Research Hypotheses)

### Phase 1 가설
- **H1.1**: OS 도메인의 좁은 작업(spec 변환)은 SLM(1.5B)이 LLM(70B)을 따라잡을 수 있다.
- **H1.2**: Fine-tuned SLM은 adversarial input에 대해 generalist LLM보다 robust하다.
- **H1.3**: 로컬 추론은 cloud API 대비 latency 5배+ 빠르다.

### Phase 2 가설
- **H2.1**: LLM agent는 정적 priority 룰보다 starvation을 빠르게 감지/해결한다.
- **H2.2**: 적응형 LLM 우선순위 조정은 mixed CPU/IO workload throughput을 개선한다.
- **H2.3**: Agent는 비정상 프로세스(무한루프 등)를 자동 격리할 수 있다.

각 가설은 정량 측정 가능. 결과가 가설을 부정해도 "negative result"로 학술적 가치 있음.

---

## 2. 일정 (8주, 안전망 다중)

| Week | Phase | 작업 | "여기까지만 해도 OK" 라인 |
|---|---|---|---|
| 1 | 1 | 환경 세팅 + 시드 데이터 200개 | 데이터셋 완성, 학습 직전 |
| 2 | 1 | Teacher API로 데이터 확장 (3000개) + LoRA 학습 | 1차 모델 학습 완료, 정확도 측정 |
| 3 | 1 | 평가 + ablation (모델 크기 3종 비교) | **★ Phase 1 발표 가능 라인** |
| 4 | 1 | Adversarial test 200개 + 보고서 작성 | **★ Phase 1 완성 (논문 1편 분량)** |
| 5 | 2 | xv6 측 sense layer (getstats 확장) + host agent loop 골격 | 5초 주기 자동 호출 동작 |
| 6 | 2 | 4가지 워크로드 시나리오 작성 + 5가지 baseline 측정 시작 | 1차 측정 데이터 확보 |
| 7 | 2 | 측정 50 runs avg + 시각화 | **★ Phase 2 발표 가능 라인** |
| 8 | 2 | 통합 보고서 + 데모 영상 + GitHub 정리 | **★ 학회 워크숍 제출 가능 수준** |

→ Week 3, 4, 7, 8 끝나는 시점이 발표 시점 후보. 어디서 멈춰도 OK.

---

## 3. Phase 1 상세 — Knowledge Distillation

### 3.1 Teacher 선택

| 후보 | 장점 | 단점 |
|---|---|---|
| Solar Pro 3 | 통합본이 이미 호환, 한국어 강함 | 응답 품질 분산 |
| **Claude Opus 4.7** | 일관된 고품질, JSON 안정 | 비용 |
| GPT-4o | 가장 strong | 비용 |
| Mix (앙상블) | 최고 품질 | 복잡 |

**권장: Claude Opus 4.7** (또는 Solar + Claude 둘 다 호출해서 합의된 것만 채택)

### 3.2 데이터 생성 파이프라인

```python
# Teacher data generation
SEEDS = 200    # 수동 라벨링 (한국어 50, 영어 50, 모호한 표현 50, 극단 50)
AUGMENT = 15   # 시드당 변형 수
RAW = SEEDS * AUGMENT          # = 3000
ADVERSARIAL = 500              # 별도 수동 작성
TOTAL = 3500
TRAIN = 3000, TEST = 500
```

### 3.3 Student 모델 비교 (ablation)

| 모델 | 크기 | 5070 Ti 학습 | 예상 정확도 |
|---|---|---|---|
| Qwen 2.5 0.5B | 0.5B | 3분 | 87% |
| **Qwen 2.5 1.5B** | 1.5B | 8분 | **94%** ⭐ |
| Qwen 2.5 3B | 3B | 12분 | 95% |
| Qwen 2.5 7B | 7B | 25분 | 96% |
| Llama 3.2 1B (대조군) | 1B | 6분 | 88% |

→ 1.5B를 메인으로 가되 다른 4개도 학습 (총 1시간 미만). 비교 표가 풍부해짐.

### 3.4 평가 지표

```
Metric 1: cmd accuracy           (목표 95%+)
Metric 2: queue_hint accuracy    (목표 92%+)
Metric 3: valid JSON ratio       (목표 99%+)
Metric 4: Korean accuracy        (목표 92%+)
Metric 5: English accuracy       (목표 95%+)
Metric 6: latency p50/p99        (목표 Solar 1/5)
Metric 7: adversarial refuse     (목표 95%+)
Metric 8: cost per 1000 queries  (목표 $0)
```

### 3.5 예상 결과 (목표치)

```
┌───────────────────────────────────────────────────────────────────┐
│ Phase 1 Headline Table                                            │
├──────────────┬───────┬─────────┬──────────┬────────┬─────────────┤
│ Model        │ Size  │ Latency │ Accuracy │ Korean │ Adversarial │
├──────────────┼───────┼─────────┼──────────┼────────┼─────────────┤
│ Solar Pro 3  │ ~70B  │ 900ms   │  91.2%   │ 88.4%  │  72%        │
│ Claude Opus  │ ~200B │ 1.2s    │  93.5%   │ 91.0%  │  85%        │
│ Heuristic    │  -    │ <1ms    │  62.3%   │ 41.0%  │  -          │
│ Random       │  -    │ <1ms    │  16.7%   │ 16.7%  │  -          │
│ ★ Ours-0.5B  │ 0.5B  │ 30ms    │  87.6%   │ 82.3%  │  91%        │
│ ★ Ours-1.5B  │ 1.5B  │ 80ms    │  94.1%   │ 91.8%  │  95%        │
│ ★ Ours-3B    │ 3B    │ 140ms   │  95.3%   │ 93.2%  │  97%        │
│ ★ Ours-7B    │ 7B    │ 250ms   │  96.4%   │ 94.6%  │  98%        │
└──────────────┴───────┴─────────┴──────────┴────────┴─────────────┘
```

→ **이 한 표만으로도 우수상급.** "47x 작은 모델로 정확도 상회, latency 11x, robust 23%p"

---

## 4. Phase 2 상세 — Autonomous OS Agent

### 4.1 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                  xv6 kernel                             │
│  ┌───────────────────────────────────────────────────┐  │
│  │  MLFQ scheduler (hyunsung 통합본 그대로)          │  │
│  │  + sense layer: getstats() 확장                   │  │
│  └────┬────────────────────────────────────▲────────┘  │
│       │ snapshot                            │           │
│       │ (pid, prio, run, io, wait_age, ...) │           │
│       ▼                                     │ setpri    │
└───────│─────────────────────────────────────│──────────┘
        │                                     │
┌───────▼─────────────────────────────────────│──────────┐
│             Host LLM Agent (Python)         │           │
│                                             │           │
│  loop every Δt (1초 또는 이벤트):           │           │
│    1. snapshot ← getstats()                 │           │
│    2. anomaly_detector(snapshot)            │           │
│       (값싼 룰, 위험 신호 발견 시만 LLM 호출)│           │
│    3. if anomaly: prompt = build_prompt(...)│           │
│    4. action ← LLM(prompt)                  │           │
│    5. safety_guard(action)                  │           │
│       - pid 1 보호                           │           │
│       - rate limit (max N actions/sec)     │           │
│       - level ∈ {0,1,2} 검증                │           │
│    6. apply(action) ───────────────────────►│           │
│    7. log(snapshot, action, result)         │           │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 4.2 Anomaly Detector (LLM 호출 줄이기)

LLM이 매 초마다 호출되면 너무 느림. 값싼 룰로 사전 필터:

```python
def anomaly_detected(snapshot) -> bool:
    for p in snapshot.procs:
        if p.wait_age > STARVATION_THRESHOLD:  # 100 tick 이상 안 돈 RUNNABLE
            return True
        if p.run_ticks > RUNAWAY_THRESHOLD:   # CPU 너무 오래 잡고 있음
            return True
        if p.io_blocks > 50 and p.priority == 2:  # IO인데 LOW에 잘못 배치
            return True
    return False
```

→ 대부분의 sample은 anomaly 없음 → LLM 호출 안 함 → 비용/속도 절감.

### 4.3 LLM Prompt 설계

```
System: You are an OS scheduler advisor for a 3-level MLFQ on xv6.
You receive a snapshot of all processes. Recommend at most 3 setpri
actions to improve fairness/throughput. Output JSON only.

Snapshot:
  pid=2  prio=2  run=180  io=0   wait_age=140  name=cpu_burner
  pid=3  prio=0  run=20   io=15  wait_age=2    name=io_burner
  pid=5  prio=2  run=0    io=0   wait_age=350  name=mlfq_bench  ← starving!

Output schema:
  {"actions": [
    {"pid": int, "new_pri": 0|1|2, "reason": "short"}
  ]}

Constraints:
  - Never set pid=1 (init) to anything
  - At most 3 actions per call
  - Justify each action briefly
```

### 4.4 Safety Guard (3중)

```python
ALLOWED_ACTIONS = {"setpri"}

def guard(action):
    if action.pid in {0, 1}:        return REJECT, "init protected"
    if action.new_pri not in {0,1,2}: return REJECT, "invalid level"
    if rate_limiter.exceeded():     return REJECT, "rate limit"
    if len(action.batch) > 3:       return REJECT, "too many actions"
    return ACCEPT, action
```

LLM이 어떤 출력을 내도 kernel에 닿기 전에 guard에서 필터링.

### 4.5 평가 시나리오 (4가지)

#### Scenario A — Starvation Recovery
```
Setup: HIGH 큐에 cpu_burner 5개 무한 투입
       동시에 LOW 큐에 mlfq_bench 1개 시작
Metric: mlfq_bench 완료까지 tick 수
Baselines: vanilla / MLFQ / rule-based / LLM agent
```

#### Scenario B — Mixed Workload Throughput
```
Setup: cpu_burner ×3 (1M iter) + io_burner ×2 (500 reads)
Metric: 총 5개 완료까지 wall-clock
Baselines: 동일
```

#### Scenario C — Hostile Process Isolation
```
Setup: 정상 작업 + 무한루프 프로세스 (while(1))
Metric: 정상 작업이 정상 완료되는 비율
Baselines: 동일
```

#### Scenario D — Stability / 4-hour Soak
```
Setup: 4시간 동안 다양한 워크로드 자동 투입
Metric: agent 충돌 없음, 시스템 패닉 없음
```

### 4.6 Baseline 5종 (이게 핵심)

| ID | Baseline | 의미 |
|---|---|---|
| B1 | Vanilla xv6 | 기본 단일 priority |
| B2 | Plain MLFQ (hyunsung) | LLM 없음, MLFQ만 |
| B3 | Random Agent | 랜덤 setpri (sanity check) |
| B4 | Rule-based Agent | "wait_age>X면 부스트" 같은 정적 룰 |
| B5 | **★ LLM Agent (Ours-1.5B)** | Phase 1 모델 |

5종 비교 결과가 풍부해야 "대단하다" 소리 들음.

### 4.7 예상 결과 (목표치)

```
┌────────────────────────────────────────────────────────────────┐
│ Phase 2 Headline Table — Scenario A (Starvation Recovery)     │
├──────────────────┬──────────┬─────────┬─────────┬─────────────┤
│ Baseline         │ 50 runs  │ mean    │ p95     │ failure %   │
├──────────────────┼──────────┼─────────┼─────────┼─────────────┤
│ B1 Vanilla       │  done    │  ∞      │  ∞      │  100%       │
│ B2 Plain MLFQ    │  done    │ 105 tk  │ 180 tk  │   8%        │
│ B3 Random Agent  │  done    │ 220 tk  │ 410 tk  │  22%        │
│ B4 Rule-based    │  done    │  78 tk  │ 110 tk  │   2%        │
│ B5 ★ LLM Agent   │  done    │ ★ 42 tk │ ★ 65 tk │ ★  0%       │
└──────────────────┴──────────┴─────────┴─────────┴─────────────┘
```

→ **이 결과가 나오면 학부 학기말 프로젝트로 거의 최상위급.**

---

## 5. 위험 관리 + 폴백 계획

### Risk 1: LLM agent가 rule-based보다 나쁘다

**대응**: negative result로 정직히 보고. 가설 검증 자체가 과학.
> "OS scheduling은 LLM에 비효율적. 다만 starvation 특정 시나리오에선 의미 있음."

이건 학술적으로 가치 있는 결과.

### Risk 2: 5070 Ti 환경 문제 (Blackwell 호환성)

**대응**: plain transformers + PEFT로 폴백. Unsloth 없이도 학습 가능 (1.5배 느릴 뿐).

### Risk 3: 데이터 라벨링 시간 초과

**대응**: 시드를 100개로 줄이고 augmentation 더 큰 비율(20x). 또는 Teacher API로 시드도 일부 생성.

### Risk 4: Phase 2 구현 시간 부족

**대응**: Phase 1만 완성해서 발표. 이미 우수상급 결과.

### Risk 5: 통합본과 충돌

**대응**: agent loop은 별도 프로세스. 통합본 코드 변경 0. 환경변수로만 swap.

---

## 6. 결과물 (산출물 목록)

### Phase 1 결과물
- [ ] `data/seeds.jsonl` (200개)
- [ ] `data/augmented.jsonl` (3000개)
- [ ] `data/adversarial.jsonl` (500개)
- [ ] `data/test.jsonl` (500개 hold-out)
- [ ] `models/ours-{0.5B, 1.5B, 3B, 7B}.gguf` (4개)
- [ ] `scripts/train.py`, `scripts/evaluate.py`, `scripts/generate_data.py`
- [ ] `reports/phase1.md` (정량 비교 표, 그래프)
- [ ] Ollama 등록 + 통합본 swap 데모

### Phase 2 결과물
- [ ] `agent/observer.py` (snapshot 수집)
- [ ] `agent/anomaly_detector.py` (값싼 룰)
- [ ] `agent/llm_advisor.py` (LLM 호출)
- [ ] `agent/safety_guard.py` (3중 가드)
- [ ] `agent/main.py` (loop)
- [ ] `workloads/{starvation, mixed, hostile, soak}.c` (xv6 user 프로그램)
- [ ] `eval/run_baselines.sh` (5종 baseline 자동 측정)
- [ ] `eval/plots/` (matplotlib 그래프 6~8개)
- [ ] `reports/phase2.md`
- [ ] 4분 데모 영상

### 최종 통합 결과물
- [ ] `FINAL_REPORT.md` (Phase 1 + Phase 2 합본, 영문, 약 8~12 페이지)
- [ ] `README.md` 업데이트 (재현 가이드)
- [ ] GitHub repo 정리 + LICENSE
- [ ] (선택) arxiv-style 1-page abstract

---

## 7. 성공 기준 (학부 OS 평가자 시점)

### 최소 합격선 (Phase 1 끝)
- ✅ 1.5B 모델 정확도 90%+
- ✅ Solar 대비 latency 5x+ 빠름
- ✅ 정량 비교 표 1개
- ✅ 코드 재현 가능
- → **이 정도면 학부 우수상**

### 목표선 (Phase 2 끝)
- ✅ 위 최소 합격선 +
- ✅ LLM Agent가 1개 이상 시나리오에서 baseline 능가
- ✅ 4시간 soak test 안정 동작
- ✅ Adversarial robustness 95%+
- ✅ 4분 데모 영상
- → **"대단하다" 소리, 학과 우수상 / 학회 워크숍 제출 가능**

### 야심선 (학기 끝)
- ✅ 위 목표선 +
- ✅ 모든 4개 시나리오에서 LLM Agent가 baseline 능가
- ✅ 학회 워크숍 발표 (예: KCC, USENIX OSDI poster, MLSys workshop)
- → **학부생으로서 진짜 대단한 수준**

---

## 8. "대단하다" 점수 요소 (체크리스트)

| 요소 | Phase 1 끝 | Phase 2 끝 |
|---|---|---|
| ✅ 단순 LLM 래퍼 아닌 진짜 OS 작업 | ✅ | ✅✅ (closed-loop) |
| ✅ 정량 측정 다수 | ✅ (8 metrics) | ✅✅ (8 + 4 scenarios × 5 baselines) |
| ✅ 학술적 클레임 | ✅ Distillation | ✅✅ Autonomous OS Agent |
| ✅ 재현성 (open source) | ✅ | ✅ |
| ✅ Negative result도 포용 | - | ✅ (정직성) |
| ✅ 학부생 수준 넘는 야심 | △ | ✅✅ |
| ✅ 실용성 (실제 동작) | ✅ | ✅ |
| ✅ 비용 효율 (5070 Ti만 사용) | ✅ | ✅ |

---

## 9. Week 1 — 즉시 시작 가능한 task list

이번 주 안에 끝낼 작업:

### Day 1-2 (4~6시간)
- [ ] `data/` 디렉토리 생성
- [ ] 시드 데이터 200개 직접 작성 (한국어 50, 영어 50, 모호 50, 극단 50)
  - 형식: 통합본의 `NL_TO_SPEC_PROMPT` 출력 schema 그대로
- [ ] Teacher API 키 준비 (Claude API 또는 Solar)
- [ ] `scripts/generate_data.py` 작성 (Teacher 호출 + augmentation)

### Day 3-4 (4~5시간)
- [ ] 5070 Ti 환경 셋업 (CUDA 12.6, PyTorch, Unsloth)
- [ ] Qwen 2.5 1.5B 다운로드 + smoke test
- [ ] 시드 200개 → augmented 3000개 자동 생성
- [ ] 데이터 품질 검수 (50개 sampling)

### Day 5-7 (8~10시간)
- [ ] `scripts/train.py` 작성 (LoRA r=32)
- [ ] 1차 학습 (1.5B 모델, 약 8분)
- [ ] 1차 평가 (test 500개)
- [ ] 결과 기록

### Week 1 끝 시점 결과물
- 시드 데이터 200개
- Augmented 3000개
- Adversarial 500개
- 1차 학습된 1.5B 모델
- 1차 평가 결과 (Excel/CSV)

→ 이게 끝나면 이미 학부 평균 수준 넘음. Phase 1 본격 시작.

---

## 10. 다음 단계 (즉시 진행 가능한 것)

### 옵션 1: Week 1 Day 1-2부터 바로 시작
- 시드 데이터 작성 템플릿 + 첫 50개 자동 생성 시작
- 필요한 도구: Claude API 키 또는 Solar API 키

### 옵션 2: 먼저 Phase 1 evaluation harness부터
- 평가 코드를 먼저 만들고, 데이터 + 학습으로 진행
- "측정 가능한 것부터" 접근

### 옵션 3: Phase 2 prototype부터 (역방향)
- agent loop을 Solar API로 먼저 prototype
- 동작 확인 후 fine-tuned 모델로 swap

→ **권장: 옵션 1.** 데이터가 모든 것의 기반. 데이터 없으면 학습도 평가도 불가능.

---

## 11. 핵심 메시지

- **시간 여유 + 안정성 우선** = 단계적 진행이 정답
- Phase 1만 해도 우수상 보장
- Phase 2가 negative여도 Phase 1 결과로 안전망
- Phase 2가 positive면 "대단하다" 소리 + 학회 워크숍급
- 통합본 코드 변경 0 — 모든 작업은 환경변수와 별도 agent로

진행할 phase/week를 정하면 그에 맞춰 구체적인 스크립트, 시드 데이터 템플릿, 평가 코드를 만들어드립니다.
