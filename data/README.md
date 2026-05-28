# Seed Dataset — SmartShell NL-to-Spec

Phase 1 (Distillation) 학습 데이터셋의 시드 (seed).

생성: 2026-05. Claude Opus 4.7에 의해 직접 작성 + 검수.

---

## 1. 파일 구성

```
data/
├── README.md                      (이 파일)
├── seeds.jsonl                    (300개 통합본)
└── seeds/
    ├── 01_ko_interactive.jsonl    (50개 — 한국어 HIGH)
    ├── 02_ko_background.jsonl     (50개 — 한국어 LOW)
    ├── 03_en_interactive.jsonl    (50개 — 영어 HIGH)
    ├── 04_en_background.jsonl     (50개 — 영어 LOW)
    ├── 05_mid_ambiguous.jsonl     (50개 — 한/영 혼합 MID)
    └── 06_adversarial_reject.jsonl (50개 — 거부 케이스)
```

## 2. 데이터 형식

각 line은 1개의 chat-template 학습 예시:

```jsonl
{"user": "<자연어 입력>", "spec": {"cmd": "...", "args": [...], "queue_hint": 0|1|2, "reason": "..."}}
```

### 필드 의미
- `user`: 사용자 자연어 입력 (한국어 또는 영어)
- `spec.cmd`: 실행할 프로그램 (cpu_burner / io_burner / mixed_burner / echo / cat / ls)
  - 거부 케이스는 `cmd="reject"` — 통합본의 `validate_spec()`이 자동 거부
- `spec.args`: 인자 배열 (모두 문자열)
- `spec.queue_hint`: MLFQ 큐 레벨 (0=HIGH, 1=MID, 2=LOW)
- `spec.reason`: 짧은 영문 설명 (학습 시 추론 근거로 활용)

## 3. 분포 (300개)

### queue_hint
| 큐 | 개수 | 비율 |
|---|---:|---:|
| 0 (HIGH) | 150 | 50.0% |
| 1 (MID)  |  50 | 16.7% |
| 2 (LOW)  | 100 | 33.3% |

### cmd
| cmd | 개수 |
|---|---:|
| cpu_burner | 77 |
| io_burner | 52 |
| reject | 50 |
| mixed_burner | 41 |
| echo | 30 |
| cat | 30 |
| ls | 20 |

### 언어
| 언어 | 카테고리 | 개수 |
|---|---|---:|
| 한국어 | 01, 02 + 일부 05, 06 | ~125 |
| 영어 | 03, 04 + 일부 05, 06 | ~125 |
| 혼합 | 05 일부 | ~50 |

## 4. 카테고리 의도

| # | 카테고리 | 의도 |
|---|---|---|
| 01 | ko_interactive | 짧고 인터랙티브한 한국어. echo/ls/cat/짧은 io_burner. queue_hint=0 |
| 02 | ko_background | 길고 무거운 한국어. cpu_burner 200만~3000만 / 큰 io / 큰 mixed. queue_hint=2 |
| 03 | en_interactive | 짧고 인터랙티브한 영어. 한국어 카테고리 1 영문 미러 |
| 04 | en_background | 길고 무거운 영어. 한국어 카테고리 2 영문 미러 |
| 05 | mid_ambiguous | 모호한 표현 ("보통", "그냥", "moderate", "regular"). queue_hint=1 default |
| 06 | adversarial_reject | rm -rf, prompt injection, out-of-scope, gibberish. cmd="reject" |

## 5. 다음 단계 (Phase 1 진행)

1. **Augmentation**: 시드 300개 → 변형 4500개 (시드당 15개)
   - 동일 의미, 다른 표현
   - Claude/Solar API 호출 또는 이 대화 내에서 직접 생성

2. **Hold-out test set**: 시드 300개 중 50개는 학습에서 제외해서 평가용

3. **학습**: Qwen 2.5 1.5B (또는 7B) + LoRA on RTX 5070 Ti

자세한 계획은 `../LLM_RESEARCH_PROPOSAL.md` 참고.

## 6. 사용 권장 (재현)

```python
import json
from collections import Counter

seeds = [json.loads(l) for l in open("data/seeds.jsonl")]
print(f"Total: {len(seeds)}")
print(Counter(s['spec']['queue_hint'] for s in seeds))
print(Counter(s['spec']['cmd'] for s in seeds))
```
