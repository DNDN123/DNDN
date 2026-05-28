# LLM Fine-tuning 계획서 — Smart-MLFQ 전용 모델

목표: 통합본(`DNDN-hyunsung-demo/integration/host/nl_shell.py`)이 호출하는 Solar Pro 3를 **우리가 학습한 로컬 모델**로 대체.

핵심 제약: 통합본 코드 변경 최소화. `nl_shell.py`의 BASE_URL과 MODEL 2줄만 바꿔서 swap 가능하도록.

---

## 0. 사전 정보 — 통합본이 LLM에게 요구하는 것

### 입력
사용자 자연어 한 줄. 한국어 또는 영어.
예: `"Run a heavy computation in the background"`

### 출력 (Solar가 반환하는 형식)
```json
{
  "cmd": "<program>",
  "args": ["<arg1>", "..."],
  "queue_hint": 0 | 1 | 2,
  "reason": "<short English sentence>"
}
```

### 제약
- `cmd`는 다음 중 하나: `cpu_burner`, `io_burner`, `mixed_burner`, `echo`, `cat`, `ls`
- `queue_hint`: 0 (HIGH) / 1 (MID) / 2 (LOW)
- 판단 휴리스틱:
  - "fast", "quick", "interactive", I/O 작업 → 0
  - "background", "heavy", "long", CPU 집중 → 2
  - 모호하거나 mixed → 1
- 응답은 **순수 JSON만** (마크다운/설명 없이)

### 추가 (선택)
- `DIAGNOSE_PROMPT`: 프로세스 진단 (snapshot → JSON 보고서)
- `TRACETOOL_ANALYZE_PROMPT`: 트레이스 분석

→ 1차 목표는 `NL_TO_SPEC_PROMPT`만. 진단/트레이스는 phase 2.

---

## 1. 환경 — RTX 5070 Ti + macOS 분리 워크플로우

```
┌────────────────────────────────────────────────────┐
│ 5070 Ti 머신 (Linux / Windows WSL2)                │
│                                                    │
│   - 데이터셋 생성 + 라벨링                          │
│   - Qwen 2.5 7B + LoRA fine-tune                   │
│   - GGUF 변환 + Q4_K_M 양자화                       │
│   - 결과물: smartmlfq-q4.gguf (~4.5GB)              │
└────────────────────────────────────────────────────┘
                  │ scp / USB
                  ▼
┌────────────────────────────────────────────────────┐
│ macOS (현재 SmartShell + 통합본 환경)              │
│                                                    │
│   - Ollama 등록: ollama create smartmlfq -f Modelfile│
│   - nl_shell.py에서 BASE_URL/MODEL 2줄 swap        │
│   - 통합본 데모 + 평가                              │
└────────────────────────────────────────────────────┘
```

---

## 2. 모델 선택

| 후보 | VRAM (LoRA 4bit) | 한국어 | 추천도 | 사유 |
|---|---|---|---|---|
| **Qwen 2.5 7B Instruct** | 8GB | 매우 좋음 | ⭐⭐⭐⭐⭐ | 한국어 + JSON 출력 잘함. 5070 Ti sweet spot |
| Llama 3.2 3B | 4GB | 보통 | ⭐⭐⭐ | 빠른 baseline용 |
| Qwen 2.5 1.5B | 3GB | 좋음 | ⭐⭐⭐⭐ | 빠른 prototype |
| Qwen 2.5 14B | 14GB (4bit) | 매우 좋음 | ⭐⭐⭐ | 한계 근접, 별 이득 없음 |

**최종 선택: Qwen 2.5 7B Instruct (4-bit) + LoRA r=32**

---

## 3. 데이터셋 설계

### 구조 (Chat Template — Qwen 호환)

```jsonl
{
  "messages": [
    {"role": "system", "content": "<NL_TO_SPEC_PROMPT 전문>"},
    {"role": "user",   "content": "백그라운드에서 무거운 계산 돌려줘"},
    {"role": "assistant", "content": "{\"cmd\": \"cpu_burner\", \"args\": [\"2000000\"], \"queue_hint\": 2, \"reason\": \"Heavy CPU work backgrounded\"}"}
  ]
}
```

### 데이터 카테고리 (총 2000개 목표)

| 카테고리 | 예시 | 비중 |
|---|---|---|
| HIGH 큐 (interactive/I/O) | "echo 빠르게", "README 열어줘", "ls" | 30% (600개) |
| LOW 큐 (CPU heavy/background) | "무거운 계산 백그라운드", "burner 천만번" | 30% (600개) |
| MID 큐 (mixed/ambiguous) | "보통 작업 하나", "그냥 돌려" | 20% (400개) |
| 다양한 한국어 표현 변형 | 구어체, 격식체, 줄임말 | 골고루 분포 |
| 다양한 영어 표현 변형 | formal, casual, abbreviated | 골고루 분포 |
| Adversarial (안전 가드용) | "rm -rf", "init 죽여", 화이트리스트 외 cmd | 10% (200개) |
| Edge case | 오타, 중복 단어, 매우 짧은 입력 | 10% (200개) |

### 데이터 생성 파이프라인 (3단계)

#### Stage 1 — 시드 라벨링 (수동, 2시간, 120개)

각 (cmd × queue) 조합 별로 10개씩 직접 작성:

```jsonl
{"messages": [{"role": "user", "content": "echo hello 빠르게"}, {"role": "assistant", "content": "{\"cmd\":\"echo\",\"args\":[\"hello\"],\"queue_hint\":0,\"reason\":\"short interactive\"}"}]}
{"messages": [{"role": "user", "content": "Run cpu_burner heavy 1000000"}, {"role": "assistant", "content": "{\"cmd\":\"cpu_burner\",\"args\":[\"1000000\"],\"queue_hint\":2,\"reason\":\"CPU heavy backgrounded\"}"}]}
```

#### Stage 2 — Claude API augmentation (1시간, 1500개)

`generate_data.py`:
```python
import anthropic, json
client = anthropic.Anthropic()

PROMPT = """
다음 의미와 동일한 한국어/영어 표현 15개 만들어줘.
구어체, 격식체, 줄임말, 약간 모호한 표현 다양하게.
원본 입력: "{user_input}"
원본 spec:  {spec_json}

출력 형식: JSON 배열, 각 원소는 {{"user": "<새 표현>"}}만 포함.
"""

for seed in load_seeds():
    response = client.messages.create(
        model="claude-opus-4-7",
        max_tokens=2000,
        messages=[{"role": "user", "content": PROMPT.format(
            user_input=seed["input"], spec_json=json.dumps(seed["spec"])
        )}]
    )
    new_inputs = json.loads(response.content[0].text)
    for nv in new_inputs:
        # 새 표현 + 원본 spec
        save_jsonl({"user": nv["user"], "spec": seed["spec"]})
```

→ 시드 120개 × 15 변형 = 1800개

#### Stage 3 — Adversarial + Edge cases (1시간, 380개)

수동 작성 (작가 본인이 LLM을 망가뜨릴 만한 입력):

```
"rm -rf /"                            → REJECT (cmd not in list)
"sudo make me a sandwich"             → REJECT
"파괴해줘"                            → REJECT
"echo " (빈 args)                     → echo with default
"cpu_burner 999999999999999"          → cpu_burner with huge N
"ㅁㄴㅇㄹ"                            → REJECT (gibberish)
"please run cat README and also ls"  → 첫 번째만 (또는 둘 다 reject)
```

→ 총 ~2000개 데이터셋 완성. Train 1800 / Test 200 분리.

---

## 4. 학습 (Unsloth, RTX 5070 Ti)

### 환경 설정
```bash
# CUDA 12.6+ (Blackwell sm_120 지원)
pip install torch --index-url https://download.pytorch.org/whl/cu126
pip install unsloth bitsandbytes transformers trl peft datasets accelerate
```

### 학습 스크립트 (`train.py`)

```python
from unsloth import FastLanguageModel
from datasets import load_dataset
from trl import SFTTrainer
from transformers import TrainingArguments

# 1. 모델 + LoRA
model, tokenizer = FastLanguageModel.from_pretrained(
    model_name="unsloth/Qwen2.5-7B-Instruct-bnb-4bit",
    max_seq_length=2048,
    load_in_4bit=True,
)
model = FastLanguageModel.get_peft_model(
    model, r=32, lora_alpha=32,
    target_modules=["q_proj","k_proj","v_proj","o_proj",
                    "gate_proj","up_proj","down_proj"],
)

# 2. 데이터셋 로드
def format_chat(example):
    return {"text": tokenizer.apply_chat_template(
        example["messages"], tokenize=False
    )}
dataset = load_dataset("json", data_files="train.jsonl")["train"]
dataset = dataset.map(format_chat)

# 3. 학습
trainer = SFTTrainer(
    model=model, tokenizer=tokenizer,
    train_dataset=dataset,
    dataset_text_field="text",
    max_seq_length=2048,
    args=TrainingArguments(
        per_device_train_batch_size=16,
        gradient_accumulation_steps=2,
        warmup_steps=10,
        num_train_epochs=5,
        learning_rate=2e-4,
        bf16=True,                           # Blackwell native
        logging_steps=5,
        output_dir="outputs",
        save_strategy="epoch",
    ),
)
trainer.train()

# 4. GGUF 양자화 저장
model.save_pretrained_gguf(
    "smartmlfq-q4",
    tokenizer,
    quantization_method="q4_k_m",   # 4-bit, 약 4.5GB
)
```

**예상 학습 시간**: 약 5~8분 (Qwen 7B, 1800개, 5 epoch, 5070 Ti).

---

## 5. 평가 (Hold-out test set, 200개)

### `evaluate.py`

```python
import json, requests
from collections import Counter

test_set = [json.loads(l) for l in open("test.jsonl")]
correct_cmd, correct_queue, valid_json = 0, 0, 0

for ex in test_set:
    user_input = ex["messages"][1]["content"]
    expected = json.loads(ex["messages"][2]["content"])

    # Ollama 로컬 호출 (학습 검증용)
    r = requests.post("http://localhost:11434/v1/chat/completions", json={
        "model": "smartmlfq",
        "messages": ex["messages"][:2],
        "temperature": 0,
        "response_format": {"type": "json_object"},
    })
    raw = r.json()["choices"][0]["message"]["content"]

    try:
        pred = json.loads(raw)
        valid_json += 1
        if pred.get("cmd") == expected["cmd"]: correct_cmd += 1
        if pred.get("queue_hint") == expected["queue_hint"]: correct_queue += 1
    except json.JSONDecodeError:
        pass

n = len(test_set)
print(f"Valid JSON      : {valid_json/n*100:.1f}%")
print(f"Correct cmd     : {correct_cmd/n*100:.1f}%")
print(f"Correct queue   : {correct_queue/n*100:.1f}%")
```

### 목표 지표

| 지표 | 최소 합격 | 목표 |
|---|---|---|
| Valid JSON 비율 | 95% | 99%+ |
| cmd 정확도 | 90% | 95%+ |
| queue_hint 정확도 | 85% | 92%+ |
| 한국어 정확도 (subset) | 85% | 92%+ |
| Adversarial 정확 거부 | 90% | 95%+ |

---

## 6. 배포 — Ollama 등록 + 통합본 swap

### Step 1. 5070 Ti 머신에서 GGUF를 macOS로 복사
```bash
scp smartmlfq-q4.gguf jeonhaneol@<mac-ip>:~/Downloads/
```

### Step 2. macOS에서 Ollama 등록
```bash
brew install ollama
ollama serve &

cat > Modelfile <<'EOF'
FROM ~/Downloads/smartmlfq-q4.gguf

SYSTEM """You are a command translator for an xv6 natural-language shell with a 3-level MLFQ scheduler. Translate user requests into JSON specs of the form: {"cmd":"<program>","args":[...],"queue_hint":0|1|2,"reason":"..."}. Available programs: cpu_burner, io_burner, mixed_burner, echo, cat, ls. queue_hint: 0=HIGH (interactive/IO), 1=MID (mixed), 2=LOW (CPU heavy/background)."""

PARAMETER temperature 0
PARAMETER stop "<|im_end|>"
EOF

ollama create smartmlfq -f Modelfile
ollama run smartmlfq "백그라운드 무거운 작업"   # 검증
```

### Step 3. 통합본 swap (2줄 변경)

```python
# integration/host/nl_shell.py
# 변경 전 (Solar API)
base = os.getenv("UPSTAGE_BASE_URL", "https://api.upstage.ai/v1")
model = os.getenv("UPSTAGE_MODEL", "solar-pro3")

# 변경 후 (로컬 fine-tuned)
base = os.getenv("UPSTAGE_BASE_URL", "http://localhost:11434/v1")
model = os.getenv("UPSTAGE_MODEL", "smartmlfq")
```

또는 환경변수만 export하면 코드 변경 0줄:
```bash
export UPSTAGE_BASE_URL=http://localhost:11434/v1
export UPSTAGE_MODEL=smartmlfq
export UPSTAGE_API_KEY=not-needed
python3 integration/host/nl_shell.py
```

### Step 4. 통합 데모 검증

```bash
# 통합본 빌드 + 실행
cd integration/xv6-riscv && make qemu &
cd ../host && python3 nl_shell.py

# 입력
> 백그라운드로 무거운 계산 돌려줘
  → spec: {cmd: cpu_burner, queue_hint: 2}
  → nlrun 2 cpu_burner ...
  → xv6 forkpri(2), LOW 큐로 들어감
```

---

## 7. 시간 일정 / 마일스톤

| Phase | 작업 | 시간 | 누적 |
|---|---|---|---|
| 0 | 시드 데이터 120개 작성 | 2h | 2h |
| 1 | Claude augmentation 1800개 | 1h | 3h |
| 2 | Adversarial + edge 380개 | 1h | 4h |
| 3 | 환경 설정 (5070 Ti) | 30min | 4.5h |
| 4 | LoRA fine-tune | 8min | ~4.5h |
| 5 | 평가 + 분석 | 30min | 5h |
| 6 | (재학습 1회 — hyperparameter 조정 시) | 8min | ~5h |
| 7 | GGUF 변환 + Mac 이동 | 30min | 5.5h |
| 8 | Ollama 등록 + 통합 테스트 | 30min | 6h |

**총 6시간** — 주말 하루 정도면 완료 가능.

---

## 8. 진행 단계별 결과물

### Phase 0 — 데이터셋
- `seeds.jsonl` (120개) — 수동
- `augmented.jsonl` (1800개) — Claude augment
- `adversarial.jsonl` (380개) — 수동
- `train.jsonl` (1800개), `test.jsonl` (200개)

### Phase 1 — 학습
- `outputs/` — LoRA adapter + checkpoint
- `outputs/training_log.txt` — loss curve

### Phase 2 — 평가
- `eval_report.md` — 정확도 표 + 실패 사례 분석

### Phase 3 — 배포
- `smartmlfq-q4.gguf` (~4.5GB) — 양자화 모델
- `Modelfile` — Ollama 설정

### Phase 4 — 통합 검증
- 통합본 nl_shell.py에서 BASE_URL/MODEL swap
- 데모 영상 (선택)

---

## 9. 추가 옵션 (Phase 2 — 진행도 따라 결정)

### A. DIAGNOSE_PROMPT도 학습
- 입력: 프로세스 snapshot (pid, priority, run, io, ...)
- 출력: `{summary, concerns, healthy, recommended}` JSON
- 추가 데이터 500개 정도

### B. TRACETOOL_ANALYZE_PROMPT도 학습
- 입력: syscall trace 통계
- 출력: 분석 JSON

### C. 모델 비교 실험 (5070 Ti 여유 있으니)
| 모델 | 데이터 | Epoch | 정확도 | 비고 |
|---|---|---|---|---|
| Qwen 2.5 1.5B | 2000 | 5 | ? | baseline |
| Qwen 2.5 3B | 2000 | 5 | ? | mid-size |
| **Qwen 2.5 7B** | 2000 | 5 | ? | 권장 |
| Llama 3.2 3B | 2000 | 5 | ? | 비교군 |

→ 실험 표 그대로 포트폴리오 자료로 활용 가능.

### D. RLHF / DPO 추가 학습
- SFT 끝난 모델에 선호도 학습 추가
- 사람이 두 응답 비교해서 좋은 거 고르기
- 정확도 1~3% 추가 향상 기대

---

## 10. 위험 / 함정

| 위험 | 대응 |
|---|---|
| Unsloth가 Blackwell(sm_120) 미지원 | plain transformers + PEFT 폴백 (1.5배 느림) |
| GGUF 변환 시 메모리 부족 | 4-bit 양자화 대신 5-bit 시도, 또는 7B → 3B 다운그레이드 |
| Ollama 응답 시간 1초 넘음 | 추론 batch size 조정, 또는 vLLM으로 대체 |
| 한국어 정확도 낮음 | Qwen 2.5는 한국어 잘함, 안 되면 더 많은 한국어 시드 추가 |
| JSON 형식 깨짐 | `response_format={"type":"json_object"}` 강제 + Outlines 라이브러리 사용 |
| 통합본 다른 팀원의 코드와 충돌 | swap은 환경변수로만 — 코드 변경 0줄 가능 |

---

## 11. 최종 결과물 (완료 시점)

```
LLM for OS/
├── DNDN-hyunsung-demo/        ← 친구 통합본 (그대로 둠)
└── llm-training/              ← ★ 새로 만들 작업 디렉토리
    ├── data/
    │   ├── seeds.jsonl
    │   ├── augmented.jsonl
    │   ├── adversarial.jsonl
    │   ├── train.jsonl
    │   └── test.jsonl
    ├── scripts/
    │   ├── generate_data.py     ← Claude augmentation
    │   ├── train.py             ← Unsloth 학습
    │   └── evaluate.py          ← 평가
    ├── models/
    │   ├── lora-adapter/
    │   └── smartmlfq-q4.gguf
    ├── Modelfile                ← Ollama 등록용
    ├── eval_report.md           ← 정확도 분석
    └── README.md                ← 진행 기록
```

---

## 12. 실행 명령 요약 (체크리스트)

```bash
# Phase 0-2: 데이터 (5070 Ti 머신)
mkdir -p llm-training/{data,scripts,models}
python3 scripts/generate_seeds.py        # 수동으로 시드 작성
python3 scripts/generate_data.py         # Claude augmentation
python3 scripts/generate_adversarial.py  # adversarial 추가

# Phase 3-4: 환경 + 학습
pip install torch --index-url https://download.pytorch.org/whl/cu126
pip install unsloth bitsandbytes transformers trl peft datasets accelerate
python3 scripts/train.py                 # ~8분

# Phase 5: 평가
ollama serve &
ollama create smartmlfq-eval -f Modelfile
python3 scripts/evaluate.py              # 정확도 측정

# Phase 6: GGUF + Mac 이동
scp models/smartmlfq-q4.gguf user@mac:~/Downloads/

# Phase 7: macOS 등록 + 통합본 swap
ssh user@mac
brew install ollama
ollama create smartmlfq -f Modelfile
export UPSTAGE_BASE_URL=http://localhost:11434/v1
export UPSTAGE_MODEL=smartmlfq
export UPSTAGE_API_KEY=not-needed
cd DNDN-hyunsung-demo/integration/host
python3 nl_shell.py
```

---

## 핵심 메시지

- 통합본은 그대로 두고 환경변수만 바꿔서 LLM swap 가능 (코드 변경 0줄도 OK)
- 학습은 5070 Ti에서 5~8분, 데이터 만드는 시간이 가장 길어 (4시간)
- 결과물은 ~4.5GB GGUF 파일 하나
- 완성 시 외부 API 의존성 0, 오프라인 동작, 비용 0
