# 자연어 OS 셸 (LLM for OS) — 통합 문서

xv6-riscv 위에서 **자연어로 OS를 다루는** 시스템의 전체 설명. 데이터 → 학습 →
인텐트 분류 → 안전가드 → 실제 syscall 실행, 그리고 추상 명령 분해(에이전트)까지.

> 한 줄: **"LLM은 커널에 안 들어간다. 자연어를 작은 인텐트 JSON으로 바꿔주고, 실제
> 실행은 우리가 구현한 xv6 syscall이 한다. 위험 동작은 가드가 막는다."**

---

## 1. 파이프라인

```
자연어 입력
   │  (로컬 파인튜닝 SLM, OpenAI 호환 / Solar 폴백)
   ▼
spec = {"cmd","args","queue_hint","reason"}      ← 20종 인텐트 중 하나
   │  executor.py
   ├─ build_command(spec)  → 정확한 xv6 명령 문자열
   └─ SafetyGuard.evaluate → allow | confirm | reject
        · kill pid 0·1 거부 (init 보호)
        · kill / rm / killall / killheavy = 파괴적 → 확인 게이트
        · setpri prio 0..2 검증
   ▼
xv6 콘솔에서 실행 (ps, kill, setprio, killall, reap, ...)

추상 요청("정리해줘") → agent.py
   ps 관찰 → 규칙 분해 [reap, killheavy ...] → 각 단계 가드 → 실행
```

---

## 2. 인텐트 20종

`cmd` (모델이 낼 수 있는 출력 전부):

| 그룹 | cmd | args | xv6 명령 |
|---|---|---|---|
| 작업실행 | `cpu_burner`/`io_burner`/`mixed_burner` | [iters] | `nlrun <q> <cmd> <iters>` |
| | `echo` | [text] | `echo <text>` |
| 파일 | `ls` | [path?] | `ls [path]` |
| | `cat` | [path] | `cat <path>` |
| | `rm` ⚠ | [path] | `rm <path>` |
| | `mkdir` | [path] | `mkdir <path>` |
| | `ln` | [target,link] | `ln <t> <l>` |
| 프로세스 | `ps` | [] | `ps` |
| | `kill` ⚠ | [pid] | `kill <pid>` |
| | `setpri` | [pid,prio] | `setprio <pid> <prio>` |
| | `trace` | [pid,on/off] | `tracepid <pid> <0/1>` |
| 정리 | `killall` ⚠ | [name] | `killall <name>` |
| | `killheavy` ⚠ | [n?] | `killheavy [n]` |
| | `reap` | [] | `reap` |
| 시스템 | `uptime` | [] | `uptime` |
| | `sysinfo` | [] | `sysinfo` |
| 기타 | `explain` | [topic] | (답변만, OS 동작 없음) |
| | `reject` | [] | (거부) |

⚠ = 파괴적 → 안전가드가 확인 요구. `kill`/`killall`/`killheavy`는 pid 0·1·sh·init 보호.

`queue_hint` 0=HIGH(대화형/IO) 1=MID 2=LOW(무거움/백그라운드) — 작업실행류에만 의미.

---

## 3. 데이터 & 학습 (Windows + RTX 5070 Ti)

데이터: 한국어/영어 ~반반, 누수 0(그룹단위 split), train 5951 / test 1584.

```powershell
# 0. (최초) Blackwell용 torch + 스택
cd scripts
python -m pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu128
python -m pip install -r requirements.txt

# 1. 데이터 (GPU 불필요, 무료·결정론적)
python gen_intent_seeds.py
python augment_offline.py --multiplier 8
python build_train.py            # --balance oversample 로 클래스 균형 옵션

# 2. 학습 + 평가 (PYTHONUTF8 필수 — trl cp949 우회)
$env:PYTHONUTF8="1"
python train.py                  # qwen-3b r64 e10 (≈30~60분); --epochs 5 로 단축 가능
python evaluate.py --backend hf --model ..\models\smartmlfq-qwen-3b-r64-e10\lora
```

산출물: `models/smartmlfq-qwen-3b-r64-e10/{lora,merged}`, 정확도 표 `eval_report.md`.

스택: plain `transformers + peft` LoRA bf16 (unsloth/bitsandbytes 불필요 → Windows에서 안정).

---

## 4. xv6 커널/유저 추가분 (맥북/리눅스에서 빌드)

신규 syscall: `setpri` `getstats` `settrace` `forkpri` `ps` `sysinfo` **`reap`(36)**.
신규/관련 유저 프로그램:

| 프로그램 | 역할 |
|---|---|
| `nlrun <q> <prog> [args]` | 큐 지정 실행 (forkpri) |
| `ps` / `setprio` / `kill` | 목록 / 우선순위 / 종료 |
| `killall <name>` | 이름으로 일괄 종료 (pid 0·1·sh·init 보호) |
| `killheavy [n]` | run_ticks 최대 n개 종료 |
| `reap` | 버려진 좀비 정리 (init 재지정 → wait() 회수) |
| `uptime` / `sysinfo` | 업타임 / 여유메모리·프로세스 수 |
| `tracepid <pid> <0/1>` | 특정 pid syscall 추적 토글 |

```bash
cd integration/xv6-riscv && make clean && make qemu
# xv6: cpu_burner 200000000 & ; ps ; killall cpu_burner ; killheavy ; reap ; uptime ; sysinfo
```

`reap` 안전성: `freeproc`를 직접 호출하지 않고 좀비를 init으로 재지정 + `wakeup(initproc)`
→ init의 `wait()`가 검증된 경로로 회수. 락 순서 `wait_lock → p->lock` (reparent와 동일).

---

## 5. 호스트 실행기 / 에이전트

- `integration/host/executor.py` — **M3**. NL→spec(classify) / spec→명령(build_command) /
  SafetyGuard. 오프라인 자가검증: `python executor.py --selftest`. 라이브 REPL: `python executor.py`.
- `integration/host/agent.py` — **M4**. 추상 요청을 규칙 기반으로 분해.
  `좀비 정리`→`[reap]`, `무거운 거 정리`→`[killheavy]`, `정리해줘`(혼잡시)→`[reap, killheavy]`.
  자가검증: `python agent.py --selftest`.

두 파일은 `train.py`와 **동일한 SYSTEM_PROMPT**를 사용(평가·추론 일관성).

라이브 자동실행은 `repl(execute_fn=<QEMU 드라이버>)`에 드라이버를 연결하면 됨
(QEMU 부팅 상태 필요; `agent/observer.py`의 `QemuDriver` 재사용 가능).

---

## 6. 안전 모델 (3중)

1. **출력 축약**: LLM 응답이 무엇이든 syscall 경계에서 작은 인텐트로만 도달.
2. **SafetyGuard**: pid 0·1 보호, 파괴적 동작 확인 게이트, 인자/범위 검증.
3. **커널 자가교정**: 잘못된 큐 힌트도 MLFQ demotion이 몇 tick 내 교정. killall/killheavy/kill은 init/shell 보호.

---

## 7. 현재 상태 & 범위

| 항목 | 상태 |
|---|---|
| 데이터 20인텐트 / 학습 산출물 | ✅ |
| 정확도 측정 | ⏳ `evaluate.py` 실행 필요 |
| 커널/유저 C 추가분 | ✅ 작성 (맥북 `make qemu` 검증 대기) |
| M3 executor / M4 agent | ✅ 오프라인 검증 |
| 라이브 QEMU 자동실행 연결 | ⬜ 빌드 통과 후 |

**범위 밖** (xv6에 기능 자체가 없음): 네트워크/다운로드, 유저·권한(sudo/chmod),
서비스·패키지매니저, 프로세스 일시정지(SIGSTOP). 이들은 자연어로도 불가하며 `reject`로 처리.
