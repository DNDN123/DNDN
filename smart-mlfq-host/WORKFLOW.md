# Smart-MLFQ — 호스트 Python 워크플로우

> xv6 측 MLFQ 가 정상 동작하는 상태에서, **baseline → LLM 분석 → LLM-guided
> → 비교** 전체 흐름을 손으로 실행하는 가이드. 한 사이클 약 15분.

---

## 사전 준비 (한 번만)

### 1) Python 환경 셋업 (WSL)

```bash
cd "/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/smart-mlfq-host"
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 2) Solar API 키

```bash
cp .env.example .env
nano .env
# UPSTAGE_API_KEY=<실제키>  로 변경
```

키 발급: https://www.upstage.ai/events/ai-initiative-2025-ko

### 3) API 동작 확인

```bash
python3 hello_solar.py
# 응답이 출력되면 OK
```

API 키 없어도 진행 가능 — 그 경우 `llm_hint.py` 가 자동으로 **휴리스틱 fallback**
(io_blocks > run_ticks/4 이면 HIGH 등) 으로 동작합니다.

---

## 1 사이클 실험 (15분)

### Step 1 — 작업 폴더 준비

```bash
mkdir -p ~/mlfq-experiments/mixed
cd ~/mlfq-experiments/mixed
```

### Step 2 — Baseline 실행 + 로그 캡처

```bash
# WSL의 별도 셸에서. mixed.txt 또는 cpu_heavy 등 워크로드 이름 선택.
cd ~/xv6-riscv

# hints.txt 를 비워 baseline 모드 보장
echo "# empty (baseline)" > hints.txt

# 빌드 + qemu 를 tee 로 캡처
make qemu CPUS=1 2>&1 | tee ~/mlfq-experiments/mixed/baseline_run.log
```

QEMU 가 부팅되면 xv6 콘솔에서:

```
$ wrunner mixed.txt
```

`ALL_DONE wrunner pid=...` 가 보이면 종료:

```
Ctrl-A 누르고 손 떼고 X
```

`~/mlfq-experiments/mixed/baseline_run.log` 에 모든 출력이 저장됨.

### Step 3 — 트레이스 파싱

```bash
cd "/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/smart-mlfq-host"
source venv/bin/activate

python3 parse_trace.py \
    ~/mlfq-experiments/mixed/baseline_run.log \
    --output ~/mlfq-experiments/mixed/baseline.json
```

화면에 다음과 비슷한 요약이 떠야 합니다:
```
[parse] traces: 250
[parse] exits: 5
  pid=  4 cpu_burner     arr= 80 comp=130 TT=50 run=30 io= 0 final_pri=2
  pid=  5 cpu_burner     arr= 81 comp=132 TT=51 run=32 io= 0 final_pri=2
  pid=  6 io_burner      arr= 82 comp= 95 TT=13 run= 4 io=20 final_pri=0
  ...
```

### Step 4 — LLM 분석 + hints 생성

```bash
python3 llm_hint.py \
    ~/mlfq-experiments/mixed/baseline.json \
    --output ~/mlfq-experiments/mixed/hints.txt
```

출력 예시:
```
[llm] aggregated stats for 2 program(s):
  cpu_burner: run_ticks=31.0 io_blocks=0.0 (n=3)
  io_burner:  run_ticks=4.0  io_blocks=20.0 (n=2)
[llm] querying solar-pro3 (effort=low)...
[llm] hints: {'cpu_burner': 2, 'io_burner': 0}
[llm] wrote ~/mlfq-experiments/mixed/hints.txt
```

`hints.txt` 내용 확인:
```bash
cat ~/mlfq-experiments/mixed/hints.txt
# cpu_burner 2
# io_burner 0
```

### Step 5 — LLM-guided 실행 + 로그 캡처

```bash
# 생성된 hints 를 xv6 디렉토리로 복사
cp ~/mlfq-experiments/mixed/hints.txt ~/xv6-riscv/hints.txt

cd ~/xv6-riscv
make qemu CPUS=1 2>&1 | tee ~/mlfq-experiments/mixed/llm_run.log
```

xv6 콘솔에서:
```
$ wrunner mixed.txt hints.txt
```

이번엔 `spawn cpu_burner pid=N priority=2` 같은 메시지가 보여야 합니다.

`ALL_DONE` 나오면 `Ctrl-A X` 로 종료.

### Step 6 — LLM 트레이스 파싱

```bash
cd "/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/smart-mlfq-host"

python3 parse_trace.py \
    ~/mlfq-experiments/mixed/llm_run.log \
    --output ~/mlfq-experiments/mixed/llm.json
```

### Step 7 — 평가 (수치 비교)

```bash
python3 evaluator.py \
    --baseline ~/mlfq-experiments/mixed/baseline.json \
    --llm ~/mlfq-experiments/mixed/llm.json \
    --output ~/mlfq-experiments/mixed/metrics.json
```

기대 출력:
```
========================================================================
Smart-MLFQ Evaluation Report
========================================================================
  Baseline: 5 workload exits, 250 traces
  LLM:      5 workload exits, 240 traces

  Metric                       Baseline          LLM         Δ%
  ------------------------------------------------------------
  avg_turnaround                  35.40        28.20     -20.34
  avg_response                     8.20         3.40     -58.54
  throughput_per_tick              0.10         0.13      30.00
  fairness_jain                    0.85         0.91       7.06

  Program             Base TT     LLM TT         Δ%
  --------------------------------------------------
  cpu_burner            48.50      55.20      13.81
  io_burner             16.00       8.00     -50.00
```

I/O-bound 가 빨라지고 CPU-bound 는 약간 느려지는 게 정상.

### Step 8 — 차트 생성

```bash
python3 viz.py \
    --baseline ~/mlfq-experiments/mixed/baseline.json \
    --llm ~/mlfq-experiments/mixed/llm.json \
    --out-dir ~/mlfq-experiments/mixed/charts/
```

`~/mlfq-experiments/mixed/charts/` 에 PNG 4장 생성:
- `avg_turnaround.png` — baseline vs LLM 막대그래프
- `per_pid_tt.png` — 프로세스별 turnaround 추이
- `gantt_baseline.png` — baseline Gantt
- `gantt_llm.png` — LLM Gantt

탐색기에서 열어 확인. WSL의 `\\wsl$\Ubuntu\root\mlfq-experiments\mixed\charts\` 경로 또는 그냥 Windows로 복사:

```bash
cp -r ~/mlfq-experiments/mixed/charts "/mnt/c/Users/Hss/Desktop/mlfq_charts"
```

---

## 다른 워크로드도 같은 패턴

`mixed` 자리를 `cpu_heavy`, `io_heavy` 로 바꿔서 동일 8단계 반복.

추천 실험 순서:
1. **mixed** (가장 흥미로움, LLM 효과 큼)
2. **cpu_heavy** (LLM 효과 거의 없음 — 모두 LOW로 가야 마땅)
3. **io_heavy** (LLM 효과 작음 — 모두 HIGH 유지)

세 가지 다 한 후 비교 차트 모으면 보고서·발표 자료 완성.

---

## 문제 해결

### `[parse] traces: 0 exits: 0`

로그 파일이 비었거나, `make qemu` 출력이 들어있지 않음. `tee` 로 정확히
캡처했는지 확인:
```bash
head -20 ~/mlfq-experiments/mixed/baseline_run.log
# "xv6 kernel is booting" 같은 게 나와야 함
```

### `[llm] no programs to advise on`

EXIT 가 모두 `wrunner` / `sh` / `init` 라 분석 대상이 없음.
xv6에서 `wrunner <workload>.txt` 를 실제로 실행했는지 확인.

### LLM 응답이 JSON 아님 → 휴리스틱 fallback 됨

프롬프트 튜닝 필요. `prompts.py` 의 PRIORITY_RECOMMENDATION_PROMPT_FEWSHOT
수정.

### `pip install matplotlib` 실패

```bash
sudo apt install -y python3-matplotlib
# 또는
pip install matplotlib --break-system-packages
```

### 차트가 보이지 않음

WSL 안에서는 GUI 가 없어요. PNG 파일을 Windows로 복사해서 봐야 합니다.

---

## 다음 단계 (실험 데이터가 모인 후)

1. **결과 표·차트 정리** — 3 워크로드 × baseline/LLM 6개 비교
2. **README 업데이트** — 결과 highlight
3. **기술 보고서** — 분석, 한계, future work
4. **데모 영상** — 2~3분 시연
5. **발표 슬라이드 (영문)**

이 4가지가 Week 14 발표용 deliverable 입니다.
