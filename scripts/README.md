# SmartShell — Training Pipeline

Phase 1 (Distillation) 학습/평가 코드. **정확도 우선 모드** 기본 설정.
5070 Ti 머신에서 실행.

---

## Quick Start (5070 Ti machine)

```bash
# 1. 코드 가져오기
git clone https://github.com/DNDN123/DNDN.git
cd DNDN && git checkout haneol
cd "LLM for OS/scripts"

# 2. 환경 설치 (CUDA 12.6 Blackwell)
pip install torch --index-url https://download.pytorch.org/whl/cu126
pip install -r requirements.txt

# 3. (선택) 데이터 자동 증강
export UPSTAGE_API_KEY=up_...      # 또는 export ANTHROPIC_API_KEY=...
python augment.py --backend solar --multiplier 10   # 시드 × 10 변형
python build_train.py                                # train/test 재분할

# 4. 학습 + 평가
bash run_all.sh                # 정확도 모드 (Qwen 7B, 10 epochs)
```

---

## 파일 구성

| 파일 | 역할 |
|---|---|
| `train.py` | LoRA fine-tune. **정확도 우선 기본값** (Qwen 7B, r=64, 10 epochs) |
| `evaluate.py` | Hold-out test 정확도 측정, cmd/queue/lang 분해 |
| `augment.py` | Solar/Claude API로 데이터 자동 증강 |
| `build_train.py` | 모든 데이터 통합 + stratified train/test 분할 |
| `run_all.sh` | 4가지 모드 자동화 |
| `requirements.txt` | Python 의존성 |

---

## run_all.sh 모드

| 모드 | 명령 | 시간 | 용도 |
|---|---|---|---|
| **accuracy** (기본) | `bash run_all.sh` | ~30분 | Qwen 7B, r=64, 10 epochs |
| quick | `MODE=quick bash run_all.sh` | ~5분 | Qwen 1.5B baseline |
| ablation | `MODE=ablation bash run_all.sh` | ~50분 | 4개 모델 크기 비교 (0.5B/1.5B/3B/7B) |
| ensemble | `MODE=ensemble bash run_all.sh` | ~90분 | Qwen 7B × 3 seeds |

---

## train.py 옵션 (수동 호출)

```bash
# 정확도 우선 (기본값)
python train.py --model qwen-7b --epochs 10 --lora-r 64

# 더 빠른 baseline
python train.py --model qwen-1.5b --epochs 5 --lora-r 32

# 빠른 iteration용
python train.py --quick   # epochs=3, r=16, GGUF 스킵

# 더 큰 모델 (가능하면)
python train.py --model qwen-14b --epochs 8 --lora-r 32 --batch-size 4

# Early stopping
python train.py --early-stop-patience 2
```

### 산출물
- `../models/smartmlfq-<model>-r<r>-e<epochs>/`
  - `config.json`     — 학습 설정 (재현성)
  - `lora/`           — LoRA adapter (~50MB~200MB)
  - `merged/`         — base + LoRA 합쳐진 모델
  - `gguf/`           — Q4_K_M 양자화 (Ollama용)
  - `checkpoints/`    — epoch별 중간 저장

---

## augment.py 옵션

```bash
# Solar API로 시드별 10 변형
export UPSTAGE_API_KEY=up_...
python augment.py --backend solar --multiplier 10

# Claude API로 (더 강함)
export ANTHROPIC_API_KEY=sk-ant-...
python augment.py --backend claude --multiplier 15

# 작은 sample로 테스트
python augment.py --backend solar --multiplier 5 --limit 20

# 병렬 처리
python augment.py --workers 8
```

`../data/augmented_auto.jsonl`에 누적 저장됨. `build_train.py`가 통합해서 train/test 분할.

---

## evaluate.py 옵션

### Backend 1: Ollama (Quick local serving)
```bash
ollama create smartmlfq -f ../models/smartmlfq-qwen-7b-r64-e10/gguf/Modelfile
ollama serve &
python evaluate.py --backend ollama --model smartmlfq
```

### Backend 2: HF (LoRA 직접 사용)
```bash
python evaluate.py --backend hf \
    --model ../models/smartmlfq-qwen-7b-r64-e10/lora
```

### 산출물
- 콘솔 즉시 정확도 출력
- `../eval_report.md` — markdown 보고서 (카테고리/queue/언어별 분해)

---

## 측정 지표 + 목표

| 지표 | 최소 | 목표 (정확도 모드) |
|---|---|---|
| Valid JSON | 99% | 100% |
| Correct cmd | 95% | **98%+** |
| Correct queue_hint | 92% | **95%+** |
| Both correct | 90% | **94%+** |
| Latency p50 | <200ms | <200ms |
| 한국어 정확도 | 92% | **96%+** |
| 영어 정확도 | 95% | **98%+** |
| Adversarial refuse | 90% | **97%+** |

---

## 정확도 끌어올리는 단계 (필요 시)

학습 결과가 부족하면 순서대로 시도:

1. **데이터 추가**: `python augment.py --multiplier 15` 으로 시드별 15 변형 더
2. **더 큰 모델**: `--model qwen-14b` (5070 Ti 16GB 한계)
3. **Epochs 늘리기**: `--epochs 15`
4. **LoRA rank**: `--lora-r 128`
5. **Ensemble**: `MODE=ensemble bash run_all.sh` (3-seed 평균)
6. **Curriculum**: 약한 카테고리만 별도 추가 학습

---

## Trouble Shooting

### Unsloth가 Blackwell (sm_120) 미지원
1. `requirements.txt`의 `unsloth` 줄 주석 처리
2. `train.py`의 `from unsloth import FastLanguageModel` 부분을 다음으로 교체:
   ```python
   from transformers import AutoModelForCausalLM, AutoTokenizer
   from peft import LoraConfig, get_peft_model
   model = AutoModelForCausalLM.from_pretrained(model_name,
       device_map="auto", load_in_4bit=True)
   tokenizer = AutoTokenizer.from_pretrained(model_name)
   model = get_peft_model(model, LoraConfig(r=args.lora_r, ...))
   ```
3. 1.5배 느릴 뿐 결과는 동일

### CUDA OOM
- `--batch-size 4 --grad-accum 8`
- `--model qwen-3b` 로 다운그레이드
- `--lora-r 32`

### GGUF 변환 실패
- `--skip-gguf`
- LoRA만 있어도 HF backend로 평가 가능

### Solar/Claude API rate limit
- `--workers 1` 로 줄임
- 시간차 두고 재실행 (append-only 라 이어 받음)

---

## 통합본과 연결

학습 끝나면 통합본의 LLM swap (코드 변경 0):

```bash
# 5070 Ti에서 GGUF만 Mac으로
scp ../models/smartmlfq-qwen-7b-r64-e10/gguf/*.gguf jeonhaneol@mac:~/Downloads/

# Mac에서
ollama create smartmlfq -f Modelfile
export UPSTAGE_BASE_URL=http://localhost:11434/v1
export UPSTAGE_MODEL=smartmlfq
export UPSTAGE_API_KEY=not-needed
cd "../LLM for OS/integration/host"
python3 nl_shell.py
```

통합본의 `nl_shell.py`가 환경변수만 보고 endpoint 자동 swap.

---

## Phase 2 — Autonomous OS Agent

Phase 1 끝나면 (정확도 95%+ 확보 후) `LLM_RESEARCH_PROPOSAL.md`의 Phase 2 진행.
agent loop은 별도 디렉토리 `agent/` 추가 예정 (현재 미구현).
