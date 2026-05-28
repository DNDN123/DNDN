# SmartShell — Training Scripts

Phase 1 (Distillation) 학습/평가 코드. 5070 Ti 머신에서 실행.

---

## Quick Start (5070 Ti machine)

```bash
# 1. 코드 가져오기
cd ~  # or wherever
git clone https://github.com/DNDN123/DNDN.git
cd DNDN
git checkout haneol
cd "LLM for OS/scripts"

# 2. 환경 설치
pip install torch --index-url https://download.pytorch.org/whl/cu126
pip install -r requirements.txt

# 3. 전체 파이프라인
bash run_all.sh

# 또는 다른 모델 크기:
MODEL=qwen-7b bash run_all.sh
MODEL=qwen-0.5b EPOCHS=3 bash run_all.sh
```

---

## 파일 구성

| 파일 | 용도 |
|---|---|
| `train.py` | Qwen LoRA fine-tune (Unsloth) |
| `evaluate.py` | Hold-out test set 정확도 측정 (Ollama 또는 HF backend) |
| `requirements.txt` | Python 의존성 |
| `run_all.sh` | 전체 파이프라인 자동화 |

---

## train.py 옵션

```bash
python train.py --model qwen-1.5b   # 기본 (5070 Ti 권장)
python train.py --model qwen-7b     # 더 큰 모델, ~25분
python train.py --model qwen-0.5b   # 빠른 baseline, ~3분
python train.py --epochs 10         # epoch 늘리기
python train.py --lora-r 64         # LoRA rank 늘리기
python train.py --skip-gguf         # GGUF 변환 스킵 (빠른 반복)
```

산출물:
- `../models/smartmlfq-<model>/lora/` — LoRA adapter (~50MB)
- `../models/smartmlfq-<model>/gguf/` — Q4_K_M 양자화 GGUF (~1~4.5GB)
- `../models/smartmlfq-<model>/checkpoints/` — 학습 중간 저장

---

## evaluate.py 옵션

### Backend 1: Ollama (권장, 빠름)
```bash
ollama create smartmlfq -f ../models/smartmlfq-qwen-1.5b/gguf/Modelfile
ollama serve &
python evaluate.py --backend ollama --model smartmlfq
```

### Backend 2: 직접 HF (Ollama 없이)
```bash
python evaluate.py --backend hf --model ../models/smartmlfq-qwen-1.5b/lora
```

산출물:
- 콘솔: 즉시 정확도 출력
- `../eval_report.md`: 카테고리별 분해된 markdown 보고서

---

## 측정 지표

| 지표 | 목표 | 의미 |
|---|---|---|
| Valid JSON | 99%+ | 출력이 파싱 가능한 JSON |
| Correct cmd | 95%+ | 명령 분류 정확도 |
| Correct queue_hint | 92%+ | 큐 분류 정확도 |
| Both correct | 90%+ | 둘 다 정답 (가장 엄격) |
| Latency p50 | <100ms | 응답 속도 중앙값 |
| 한국어 정확도 | 92%+ | 한국어 입력만 |
| 영어 정확도 | 95%+ | 영어 입력만 |

---

## Trouble Shooting

### Unsloth가 sm_120 (Blackwell) 미지원
1. `requirements.txt`에서 `unsloth` 줄을 주석 처리
2. `train.py`의 `FastLanguageModel.from_pretrained` 호출을 plain `transformers.AutoModelForCausalLM` 로 교체
3. 1.5배 느릴 뿐 결과는 동일

### CUDA OOM
- `--batch-size 8` 로 줄이기
- `--model qwen-0.5b` 사용

### GGUF 변환 실패
- `--skip-gguf` 옵션
- LoRA adapter만 있어도 HF backend로 평가 가능
- 또는 별도 변환: https://github.com/ggerganov/llama.cpp

### Ollama not found
- `brew install ollama` (macOS) 또는 https://ollama.com/download

---

## 통합본과 연결

학습 끝나면 통합본의 LLM을 swap:

```bash
# 5070 Ti에서 GGUF만 mac으로 복사
scp ../models/smartmlfq-qwen-1.5b/gguf/*.gguf jeonhaneol@mac:~/Downloads/

# macOS에서 Ollama 등록
cd ~/Downloads
ollama create smartmlfq -f Modelfile

# 통합본 swap (코드 변경 0줄)
export UPSTAGE_BASE_URL=http://localhost:11434/v1
export UPSTAGE_MODEL=smartmlfq
export UPSTAGE_API_KEY=not-needed
cd "../LLM for OS/integration/host"
python3 nl_shell.py
```

---

## 다음 (Phase 2 — Autonomous OS Agent)

`LLM_RESEARCH_PROPOSAL.md`의 Phase 2 참고. agent loop 코드는 별도 디렉토리(`agent/`) 추가 예정.
