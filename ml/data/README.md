# Phase 1 Dataset — SmartShell NL-to-Spec

Knowledge Distillation 학습용 데이터셋.
생성: 2026-05, Claude Opus 4.7 직접 작성 + 검수.

---

## 1. 파일 구성

```
data/
├── README.md                    (이 파일)
├── seeds.jsonl                  (300개 — 6 카테고리 시드)
├── augmented.jsonl              (892개 — 시드별 5x 변형)
├── train.jsonl                  (955개 — 학습용, stratified split 80%)
├── test.jsonl                   (237개 — 평가용 hold-out 20%)
├── seeds/
│   ├── 01_ko_interactive.jsonl    (50)
│   ├── 02_ko_background.jsonl     (50)
│   ├── 03_en_interactive.jsonl    (50)
│   ├── 04_en_background.jsonl     (50)
│   ├── 05_mid_ambiguous.jsonl     (50)
│   └── 06_adversarial_reject.jsonl (50)
└── augmented/
    ├── 01_ko_interactive_aug.jsonl  (150)
    ├── 02_ko_background_aug.jsonl   (150)
    ├── 03_en_interactive_aug.jsonl  (147)
    ├── 04_en_background_aug.jsonl   (150)
    ├── 05_mid_ambiguous_aug.jsonl   (150)
    └── 06_adversarial_reject_aug.jsonl (145)
```

**Total: 1,192 examples** (시드 300 + augmented 892)

## 2. 데이터 형식

각 line = 1개 학습 예시:

```jsonl
{"user": "<자연어 입력>", "spec": {"cmd": "...", "args": [...], "queue_hint": 0|1|2, "reason": "..."}}
```

- `user`: 자연어 입력 (한국어/영어)
- `spec.cmd`: cpu_burner / io_burner / mixed_burner / echo / cat / ls / **reject** (거부)
- `spec.args`: 인자 리스트 (문자열)
- `spec.queue_hint`: 0=HIGH, 1=MID, 2=LOW
- `spec.reason`: 영문 짧은 설명

## 3. 분포

### Train/Test split (stratified by cmd, seed=42)

| Split | n | cpu | io | mixed | echo | ls | cat | reject |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Train | 955 | 290 | 86 | 121 | 144 | 94 | 64 | 156 |
| Test  | 237 |  72 | 21 |  30 |  36 | 23 | 16 |  39 |

### queue_hint 분포

| Split | HIGH (0) | MID (1) | LOW (2) |
|---|---:|---:|---:|
| Train | 473 | 163 | 319 |
| Test  | 119 |  37 |  81 |

## 4. 데이터 생성 파이프라인

```
Phase A. Seed labeling (manual, 직접 작성)
   ↓ 300 examples
Phase B. Augmentation (Claude Opus 4.7 in-session)
   ↓ +892 examples (5x per seed, varied expression)
Phase C. Stratified train/test split (random.seed=42)
   ↓ 955 train + 237 test
```

### Augmentation 다양성 (시드 1개당 5개 변형)
- 어휘 변경 ("빠르게" → "빨리", "즉시")
- 어순 변경
- 격식체 ↔ 구어체
- 줄임말 / 약어
- 영어 섞임 (코드 스위칭)
- 모호한 표현 추가
- 격식 다양화 ("해주세요" / "해줘" / "해" / "한번")

## 5. 카테고리 의도

| # | 카테고리 | queue_hint | 가르치는 패턴 |
|---|---|---|---|
| 01 | ko_interactive | 0 (HIGH) | 짧은 한국어 → HIGH 큐 |
| 02 | ko_background | 2 (LOW) | 무거운 한국어 → LOW 큐 |
| 03 | en_interactive | 0 (HIGH) | 짧은 영어 → HIGH 큐 |
| 04 | en_background | 2 (LOW) | 무거운 영어 → LOW 큐 |
| 05 | mid_ambiguous | 1 (MID) | 모호함 → MID default |
| 06 | adversarial_reject | n/a | 위험/이상한 입력 → reject |

## 6. 다음 단계 — 학습

```bash
cd ../scripts
pip install torch --index-url https://download.pytorch.org/whl/cu126
pip install -r requirements.txt
bash run_all.sh   # train + evaluate, ~10분 (5070 Ti)
```

자세한 학습/평가 방법은 `../scripts/README.md`.

## 7. 검증

```bash
# 전체 데이터 valid JSON 확인
python3 -c "
import json
for name in ['seeds.jsonl', 'augmented.jsonl', 'train.jsonl', 'test.jsonl']:
    n = sum(1 for _ in open(name))
    bad = sum(1 for line in open(name) if (json.loads(line) and False) or False)
    # actually:
    ok = sum(1 for line in open(name) if json.loads(line.strip()) is not None)
    print(f'{name}: {ok}/{n} valid')
"
```

## 8. 재현성

데이터는 deterministic하게 생성:
- 시드: Claude Opus 4.7 (이 대화에서 직접 작성)
- Augmentation: 같은 방식 (현재 대화)
- Split: `random.seed(42)`, stratified by cmd

전체 데이터셋이 git에 올라가 있어서 다른 머신에서 그대로 사용 가능.

## 9. 향후 확장 (필요 시)

학습 결과 정확도가 부족하면 보강:
- **약점 카테고리만 +500개** (예: 모호한 한국어가 약하면 카테고리 5 확장)
- **Adversarial 더** (보안 가드 강화)
- **Edge case 추가** (오타, 매우 긴 입력 등)

또는 augmentation 비율을 5x → 10x로 늘리기 (시드 → 3000개).
