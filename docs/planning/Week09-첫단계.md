# Smart-MLFQ — Week 9 첫 단계 가이드

> 이번 주(Week 9) 안에 무조건 끝내야 하는 일들. 시간 순서대로.

---

## ⏱️ 즉시 (오늘~내일)

### 1) 팀 미팅 잡기 (1.5시간)

미팅 안건:
- (5분) 주제 확정 — *"우리는 Smart-MLFQ로 간다"* 합의
- (10분) 팀명·팀 리더 결정
- (20분) 역할 분배 (아래 §3 참고)
- (30분) Proposal 같이 작성 (아래 §4 템플릿)
- (15분) GitHub repo 생성 + 초대
- (10분) 다음 주 미팅 시간 합의

### 2) 미팅 전 각자 준비 (개별 30분)

미팅 들어오기 전에 각자 다음에 답해 오기:
- *"내가 가장 잘하는 것은? (Python 알고리즘 / API·데이터 / 시각화·문서)"*
- *"가장 자신 없는 것은?"*
- *"이번 학기에 다른 과제 부담은 어느 정도인가?"*

이걸로 역할 분배가 빨라집니다.

---

## 📋 이번 주 끝까지 (일요일 밤까지)

### 3) GitHub Repository 생성

**위치**: github.com/[팀계정 또는 리더계정]/smart-mlfq (이름은 자유)

**필수 설정**
- [ ] **Public** (강의 §3 요구사항)
- [ ] README.md 초기 커밋
- [ ] `.gitignore` (Python 템플릿)
- [ ] LICENSE (MIT 권장)
- [ ] 4명 모두 collaborator로 초대

**README 초안 (최소)**
```markdown
# Smart-MLFQ

LLM-Guided Multi-Level Feedback Queue Scheduler — Operating Systems
final project (2026 Spring).

## Direction
Direction B — LLM for OS

## Project Summary
We implement a 3-level MLFQ scheduler in Python, with Upstage Solar
Pro 3 providing process-priority hints based on observed behavior
patterns. We compare baseline MLFQ against LLM-augmented MLFQ on
synthetic and realistic workloads.

## Tech Stack
Python 3.11+, OpenAI SDK (Solar API), matplotlib, pytest

## Setup
```bash
git clone <repo>
cd smart-mlfq
pip install -r requirements.txt
export UPSTAGE_API_KEY=<your_key>  # do NOT commit
python src/main.py
```

## How to Run
[Week 11 채우기]

## Demo
[Week 13 채우기]

## Team
- Lead: [이름]
- ① Scheduler core: [이름]
- ② LLM integration: [이름]
- ③ Workload generation: [이름]
- ④ Evaluation & viz: [이름]

## Documents
- [Proposal](docs/proposal-v1.md)
- [Architecture](docs/architecture.md) (Week 10)
- [Technical Report](docs/report.md) (Week 14)
```

### 4) Solar API 키 분배 + 각자 hello world

**팀 리더**
- 강사로부터 팀 API 키 1개 수령
- 각자 자기 환경 변수에 설정 (절대 git에 커밋 X)
- `.env.example` 만 repo에 커밋 (실제 키 없는 템플릿)

**각자**
```bash
pip install openai python-dotenv
```

`hello_solar.py`:
```python
from openai import OpenAI
import os
from dotenv import load_dotenv

load_dotenv()

client = OpenAI(
    api_key=os.getenv("UPSTAGE_API_KEY"),
    base_url="https://api.upstage.ai/v1"
)

response = client.chat.completions.create(
    model="solar-pro2",  # 정확한 model id는 console.upstage.ai/docs 확인
    messages=[
        {"role": "user", "content": "Hello, who are you?"}
    ]
)

print(response.choices[0].message.content)
```

각자 실행 → 응답 받으면 OK. 막히면 미팅에서 같이 디버깅.

### 5) Python 환경 통일

`requirements.txt` 합의 후 커밋:
```
openai>=1.40.0
python-dotenv>=1.0.0
matplotlib>=3.8.0
pytest>=8.0.0
```

`.gitignore`에 추가:
```
.env
__pycache__/
*.pyc
.pytest_cache/
results/
*.log
```

### 6) 1페이지 Proposal 작성 (§4 템플릿)

**위치**: `docs/proposal-v1.md`

미팅에서 같이 작성 → 일요일까지 finalize. 강사 제출용.

---

## 🎯 일요일 밤 자가진단

다음 6개에 모두 ✅ 면 Week 9 통과:

- [ ] 팀명·리더·역할 합의됨
- [ ] GitHub repo public + 4명 모두 push 가능
- [ ] README 초안 커밋됨
- [ ] `requirements.txt` + `.gitignore` 커밋됨
- [ ] 4명 모두 Solar API hello world 성공
- [ ] `docs/proposal-v1.md` 1페이지 작성 완료

---

## 4. Proposal 템플릿 (그대로 채워 넣으세요)

`docs/proposal-v1.md` 에 복사한 후 [...] 채우기:

```markdown
# Smart-MLFQ: LLM-Guided Multi-Level Feedback Queue Scheduler

**Team**: [팀명]
**Members**: [리더(이름, 학번)], [이름, 학번], [이름, 학번], [이름, 학번]
**Repository**: https://github.com/[org]/smart-mlfq

## Direction
Direction B — LLM for OS

## Problem Statement
Classical Multi-Level Feedback Queue (MLFQ) schedulers rely on fixed
heuristics — demote a process after consuming its time slice, boost
all processes periodically — which may not adapt well to diverse
workloads. Process behavior patterns (CPU/I/O ratio, burst size,
periodicity) often contain information that simple heuristics ignore.

We hypothesize that an LLM, given a summary of recent process
behavior, can recommend better priority assignments than fixed rules,
improving turnaround time and responsiveness.

## Our Approach
1. Implement a 3-level MLFQ scheduler in Python (HIGH/MID/LOW queues,
   time slices 2/4/8 ms, demotion + I/O-based promotion + periodic boost).
2. Integrate Upstage Solar Pro 3 as a *priority hint provider*.
   Given a process's recent burst history, the LLM recommends an
   initial queue level and adjustment hints.
3. Compare baseline MLFQ vs. LLM-augmented MLFQ on a workload suite.

The LLM does not control the scheduler directly; it provides hints
that the scheduler may follow. The MLFQ algorithm itself is fully
implemented by us and is the substantive OS component.

## System Architecture
[블록 다이어그램 — 손그림 또는 draw.io. 워크로드 → 시뮬레이터 →
스케줄러 ↔ LLM → 평가]

## OS Concepts (substantive use)
- **Process model & state transitions** — PCB-like structure (Week 2)
- **CPU scheduling — MLFQ, aging, time slicing** (Week 6–7) ⭐ Core
- **Synchronization** — queue access protection (Week 4, 9)

## Evaluation Metrics
- **Turnaround time** (avg, p50, p95)
- **Response time** (time to first execution)
- **Throughput** (jobs/sec)
- **Fairness** (Jain's index)
- **LLM hint utility** — % of hints that match post-hoc optimal placement

## Workloads
- CPU-bound (80% CPU bursts)
- I/O-bound (80% I/O bursts)
- Interactive (short bursts, frequent I/O)
- Realistic (mix of all)

## Tech Stack
Python 3.11+, OpenAI SDK (Solar API), matplotlib, pytest, JSON for
trace I/O

## Roles
- ① Scheduler core: [이름]
- ② LLM integration: [이름]
- ③ Workload generation: [이름]
- ④ Evaluation, viz, docs: [이름]

## Timeline
- W9 (now): Setup + proposal
- W10: Architecture + module skeleton
- W11: Hello world (RR scheduler + Gantt + mock LLM)
- W12: Full MLFQ + real LLM integration + first comparison
- W13: Full evaluation + dry-run
- W14: Final presentation

## Deliverables (by W14)
1. Public GitHub repo with full code & demo
2. Technical report
3. Development process document
4. Final presentation slides (English)

## Risks & Mitigations
- LLM responses inconsistent → temperature 0.0 + few-shot examples
- Race condition bugs → minimal threading; use single-threaded
  discrete-event simulation where possible
- Module integration delays → strict interface contract by end of W10
```

---

## 5. 역할 분배 가이드 (미팅 중 5분 안에 결정)

각자 §1의 *"내가 잘하는 것"* 답에 따라:

| 자기 답 | 적합한 역할 |
|---|---|
| Python 알고리즘·자료구조 자신 있음 | ① Scheduler core |
| API·데이터 처리 자신 있음, 프롬프트 흥미 | ② LLM integration |
| 알고리즘 + 데이터 생성 좋음 | ③ Workload generation |
| 글쓰기·시각화 좋음, 문서 잘 만듦 | ④ Eval / viz / docs |

**겹치면**: 가장 자신 없는 영역을 피하는 방식으로 조정. 또는 가위바위보.

**1지망이 모두 같으면**: ①번이 가장 핵심이므로 Python 가장 강한 사람이 양보. 나머지는 협상.

---

## 6. 자주 막히는 지점 + 대응

| 문제 | 대응 |
|---|---|
| Solar API 키가 안 옴 | 그동안 mock client 만들어서 진행. `class MockLLM: def recommend(...) → return random.choice([...])` |
| `pip install openai` 가 에러 | 가상환경 사용 (`python -m venv venv`), 또는 `pip install --user` |
| GitHub push 권한 문제 | repo 설정 → Settings → Collaborators 에서 모두 추가 확인 |
| Python 버전 불일치 | 3.11+ 강제. `python --version` 으로 모두 확인 |
| 미팅 일정 안 맞음 | 비동기 협업 — Discord/Slack 채널 만들고 GitHub Issues 활용 |

---

## 7. Week 9 체크리스트 요약 (인쇄해서 옆에 두세요)

오늘:
- [ ] 팀 미팅 일정 잡기
- [ ] 미팅 전 §1 질문에 각자 답 준비

미팅 중:
- [ ] 팀명·리더·역할 합의
- [ ] GitHub repo 생성 + 모두 초대
- [ ] Proposal 같이 작성 시작

미팅 후 ~ 일요일:
- [ ] Solar API 키 분배 + 모두 hello world 성공
- [ ] requirements.txt + .gitignore 커밋
- [ ] README 초안 커밋
- [ ] `docs/proposal-v1.md` 완성

일요일 밤:
- [ ] 자가진단 6항목 모두 ✅
- [ ] Week 10 미팅 안건 정리

---

## 마무리

Week 9는 *"코딩하는 주"* 가 아니라 ***"정렬과 셋업의 주"*** 입니다. 코드 줄 수보다 *합의의 명확성*이 중요해요. 미팅에서 *"우리 모두 같은 그림을 갖고 있다"* 는 확신이 들면 Week 9 성공입니다.

다음 주(Week 10)부터 빈 클래스 골격을 짜기 시작하고, Week 11에 Hello World 가 뜨면 그 뒤는 일정대로 흘러갑니다. 이번 주 셋업이 단단해야 그게 가능합니다.
