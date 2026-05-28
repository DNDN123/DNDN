# Phase 2 — Autonomous OS Agent (Prototype)

xv6 위에 떠 있는 **closed-loop LLM agent**. 시스템 상태를 주기적으로 보고,
이상 징후가 있으면 LLM에게 조언을 받아 우선순위를 자동 조정한다.

## 핵심 메시지

> *"LLM은 OS 안에 들어가지 않는다. 호스트 측에서 syscall 경계로만 OS와 상호작용한다.
>  결정의 책임은 LLM, 실행의 책임은 우리가 직접 짠 xv6 MLFQ 스케줄러."*

---

## 아키텍처

```
┌───────────────────────────────────────────────────────────┐
│ xv6 kernel (integration/)                                 │
│   ┌─────────────────────────────────────┐                 │
│   │  3-level MLFQ + getstats + setpri   │                 │
│   └────────┬────────────────────▲───────┘                 │
│            │ getstats output    │ setpri input            │
└────────────│────────────────────│─────────────────────────┘
             │                    │
             ▼                    │
┌────────────│────────────────────│─────────────────────────┐
│            ▼                    │                          │
│      observer.py                │                          │
│      (parse snapshot)           │                          │
│            │                    │                          │
│            ▼                    │                          │
│      anomaly_detector.py        │                          │
│      (cheap rule filter)        │                          │
│            │                    │                          │
│            ▼                    │                          │
│   if anomalies → llm_advisor.py │                          │
│                  (Solar / Ollama)│                         │
│            │                    │                          │
│            ▼                    │                          │
│      safety_guard.py ───────────┘                          │
│      (3-layer filter)                                      │
└────────────────────────────────────────────────────────────┘
```

---

## 파일 구성

| 파일 | 역할 |
|---|---|
| `observer.py` | QEMU stdio로 `getstats` 호출, 스냅샷 파싱 |
| `anomaly_detector.py` | 저비용 룰: starvation / runaway / I/O misclassified |
| `llm_advisor.py` | Solar 또는 Ollama 호출, `setpri` 액션 제안 |
| `safety_guard.py` | 3중 가드 (init 보호, prio 범위, rate limit) |
| `main.py` | 루프 + 로깅 + baseline 비교 |

---

## 4가지 Baseline (자동 비교)

| baseline | 의미 | 명령 |
|---|---|---|
| **llm** (기본) | LLM agent 조언 | `python main.py --baseline llm` |
| rule | 정적 룰 (starving→HIGH, runaway→LOW) | `python main.py --baseline rule` |
| random | 무작위 setpri (sanity check) | `python main.py --baseline random` |
| none | 아무 것도 안 함 (pure MLFQ) | `python main.py --baseline none` |

각각 같은 워크로드로 돌려 비교하면 정량 평가 가능.

---

## 사용법

### 빠른 시작 (Solar API 사용)
```bash
export UPSTAGE_API_KEY=up_...
cd agent
python main.py --duration 120        # 2분 동안 자동 동작
```

### Fine-tuned 모델 사용 (Phase 1 끝난 후)
```bash
# 5070 Ti에서 학습 후, Ollama에 등록한 모델 swap
export UPSTAGE_BASE_URL=http://localhost:11434/v1
export UPSTAGE_MODEL=smartmlfq
export UPSTAGE_API_KEY=not-needed
python main.py --duration 120
```

### Dry run (실제로 setpri 안 함)
```bash
python main.py --dry-run --duration 60
```

### Baseline 비교 실험
```bash
# 같은 시나리오를 4가지 baseline으로 돌리고 결과 비교
for b in none rule random llm; do
    python main.py --baseline $b --duration 180 --log ../agent_$b.jsonl
done
```

---

## Workload 시나리오 (실험용)

xv6 셸 안에서 다음을 미리 띄워두면 (또는 자동화 스크립트로 띄우면) agent가 어떻게 반응하는지 측정 가능:

### Scenario A — Starvation Recovery
```
# xv6 셸에서
$ cpu_burner 1000000 &
$ cpu_burner 1000000 &
$ cpu_burner 1000000 &
$ mlfq_bench &                    # 이 친구가 starve 될 가능성

# 호스트에서
$ python main.py --baseline rule  --duration 60
$ python main.py --baseline llm   --duration 60
# mlfq_bench 완료까지 tick 수 비교
```

### Scenario B — Mixed Workload Throughput
```
$ cpu_burner 500000 &
$ cpu_burner 500000 &
$ io_burner 500 &
$ io_burner 500 &
# 호스트에서 same as above
```

### Scenario C — Hostile Process Isolation
```
$ infinite_loop &      # 무한루프 (정상 작업 방해)
$ priority_test &      # 정상 작업
# agent가 infinite_loop을 LOW로 demote하는지 측정
```

### Scenario D — Soak Test
```bash
python main.py --duration 14400        # 4시간 무인 동작 + 안정성
```

---

## 결과 분석

`../agent_log.jsonl`에 모든 tick의 (snapshot, concerns, actions) JSONL 기록.

```python
import json
ticks = [json.loads(l) for l in open("../agent_log.jsonl")]
print(f"Total ticks: {len(ticks)}")
print(f"Anomalies: {sum(1 for t in ticks if t['concerns'])}")
print(f"Actions applied: {sum(len(t['actions_applied']) for t in ticks)}")
print(f"Actions rejected: {sum(len(t['actions_rejected']) for t in ticks)}")
```

---

## 안전 설계

3중 가드:
1. **Op whitelist**: `setpri`만 허용. `kill`/`exec` 절대 X
2. **PID protection**: pid 0, 1 (init) 보호
3. **Rate limit**: 초당 5개 액션 제한

→ LLM이 어떤 출력을 내도 kernel은 안전.

---

## 평가 지표 (Phase 2 발표용)

| 지표 | 측정 방법 | 목표 |
|---|---|---|
| Starvation recovery | mlfq_bench 완료 tick | baseline 대비 절반 |
| Mixed throughput | 총 완료 wall-clock | baseline 대비 10%+ |
| Hostile isolation | 정상 작업 완료율 | 95%+ |
| Soak stability | 4시간 crash 없음 | 100% |
| Adversarial guard | rejected/proposed | 100% reject |

---

## 의존성

- Python 3.10+
- `requests`
- xv6 통합본의 `getstats` / `setpri` syscall (이미 hyunsung 슬라이스에 있음)
- (선택) Ollama with fine-tuned model (Phase 1 끝나면)
- (선택) `UPSTAGE_API_KEY` (Solar 기본 backend)

---

## 한계 / 정직한 limitations

- xv6의 `getstats`/`setpri` syscall 출력 포맷이 위 코드의 정규식과 일치해야 함
  (필요 시 `observer.py`의 `_PROC_RE`를 통합본 출력에 맞게 수정)
- LLM 호출 latency ~ 100ms~1s. 그래서 1초 주기 ok, ms 주기 불가능
- `--baseline none`으로 비교군 잡아도 xv6 MLFQ 자체는 항상 동작 중 (이미 hyunsung MLFQ가 강력)
- LLM agent가 항상 더 좋다는 보장 없음. **이걸 측정으로 검증하는 게 Phase 2의 목적**

---

## 다음 단계

Phase 2 실험 흐름:
1. 통합본 빌드 + agent 환경 설정
2. 4가지 워크로드 시나리오 작성 (xv6 user 프로그램으로)
3. 각 시나리오 × 4 baseline × 10 runs = 160 측정
4. matplotlib으로 그래프 (Phase 2 보고서)
5. Phase 1 fine-tuned 모델로 swap 후 재측정 (Solar vs Ours)
