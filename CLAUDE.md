# CLAUDE.md — 프로젝트 컨텍스트 (xv6 자연어 OS 셸)

Claude Code가 세션 시작 시 자동으로 읽는 파일. **작업 전 항상 먼저 읽을 것.**

---

## 한 줄 요약

xv6-riscv(RISC-V 64-bit) 위에, **자연어 → OS 명령**으로 동작하는 시스템.
자연어를 **로컬 파인튜닝 SLM**이 구조화된 인텐트로 변환 → 안전가드 → 실제 xv6 syscall 실행.
- 방향 B (LLM for OS). 학부 OS 학기말 팀 프로젝트.
- **LLM은 절대 커널에 안 들어감**: syscall 경계에서 작은 JSON(인텐트)로 축약됨. 커널은 LLM의 존재를 모름.
- LLM 없이도 동작(결정론적 폴백/규칙). API 키·네트워크 장애에 데모가 안 깨짐.

---

## 디렉토리 구조 (현재 실제 기준)

```
LLM for OS/
├── CLAUDE.md  README.md                      ← 이 파일 / 사용자 문서
├── os/                                        ← ★ 빌드 대상 xv6 (맥북/리눅스)
│   ├── kernel/  (proc.c=MLFQ+reap, sysproc.c, syscall.*)
│   └── user/    (nlrun, ps, setprio, kill, killall, killheavy, reap,
│                 uptime, sysinfo, tracepid, *_burner ...)
├── host/                                      ← 호스트 Python (NL 브리지)
│   ├── nl_shell.py                            ← 기존 NL→spec REPL
│   ├── executor.py    ★ M3: 인텐트→xv6명령 + 안전가드
│   ├── agent.py       ★ M4: 추상요청("정리해줘")→다단계 분해
│   └── prompts.py  parse_trace.py  evaluator.py  viz.py  adapters/
├── ml/                                        ← 모델 학습 (Windows/GPU)
│   ├── data/   (seeds/01..07_*.jsonl, seeds.jsonl, augmented*, train/test.jsonl)
│   ├── scripts/(gen_intent_seeds, augment_offline, build_train, train,
│   │            evaluate, compare, ask.py, requirements.txt, run_all.sh)
│   └── models/ (smartmlfq-qwen-3b-r64-e10/{lora,merged} — gitignored, 큼)
├── autonomous-agent/                          ← Phase-2 자율 스케줄러 에이전트(별도)
├── docs/   (nl-os-agent, syscall-allocation, trace-format, integration-*, charts/ ...)
└── legacy/                                    ← hyunsung 단독 백업본 (smart-mlfq-host/-xv6-patches)
                                                  frozen archive — 내부 경로는 그대로 둠
```

---

## 가장 중요한 원칙

> **"LLM 단순 래퍼"는 인정 안 됨. OS 개념을 xv6 커널에서 직접 구현해야 함.**
- LLM 역할: 자연어 → 인텐트 JSON (호스트 측). 출력은 syscall 경계에서 작은 정수/문자열로 축약.
- 우리 구현: xv6 **3-단계 MLFQ 스케줄러**, syscall 다수(setpri/getstats/ps/reap/...), 안전가드, 호스트 브리지.
- 잘못된 힌트는 MLFQ demotion이 자가 교정. 위험 동작(kill/rm)은 가드가 pid 0·1 보호 + 확인 게이트.

---

## 자연어 인텐트 (모델 출력 스키마) — 20종

`{"cmd": ..., "args": [...], "queue_hint": 0|1|2, "reason": "..."}`

| 그룹 | cmd |
|---|---|
| 작업실행 | cpu_burner io_burner mixed_burner echo |
| 파일 | ls cat rm mkdir ln |
| 프로세스 | ps kill setpri trace |
| 정리 | killall killheavy reap |
| 시스템 | uptime sysinfo |
| 기타 | explain reject |

- `train.py`와 `evaluate.py`와 `executor.py`의 SYSTEM_PROMPT는 **반드시 글자단위로 동일**해야 함.
- 새 인텐트 추가 절차: `gen_intent_seeds.py`에 시드 추가 → `augment_offline.py` → `build_train.py` → SYSTEM_PROMPT 3곳 동기화 → 재학습.

---

## 파이프라인 (자연어 → 실행)

```
자연어 ──(로컬 SLM, OpenAI호환)──▶ spec(JSON)
           executor.py: build_command() ─▶ xv6 명령 문자열
           executor.py: SafetyGuard      ─▶ allow / confirm / reject
                                            (kill 0·1 거부, kill/rm/killall/killheavy 확인)
추상요청("정리해줘") ─▶ agent.py: ps 관찰 → [reap, killheavy ...] 분해 → 각각 가드
```

---

## 빌드 / 실행 명령

### 학습 (Windows + RTX 5070 Ti, Blackwell)
```powershell
cd ml\scripts
python -m pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu128
python -m pip install -r requirements.txt
$env:PYTHONUTF8="1"                 # trl cp949 버그 우회 (필수)
python train.py                      # qwen-3b r64 e10 → ml/models/smartmlfq-qwen-3b-r64-e10
python evaluate.py --backend hf --model ..\models\smartmlfq-qwen-3b-r64-e10\lora
python ask.py "7번 프로세스 꺼줘"    # 즉석 점검 (NL→spec)
```

### xv6 빌드 (맥북/리눅스 — RISC-V 툴체인 필요, Windows 네이티브 불가)
```bash
cd os && make clean && make qemu
# xv6: ps / killall cpu_burner / killheavy / reap / uptime / sysinfo
```

### 데이터 재생성 (GPU 불필요)
```powershell
cd ml\scripts
python gen_intent_seeds.py
python augment_offline.py --multiplier 8     # 무료, 결정론적
python build_train.py                         # 누수 0 그룹 split
```

---

## 함정 / 주의

1. **Windows 한국어 인코딩(cp949)**: 모든 파일 IO는 `encoding="utf-8"`. 학습 실행 전 `$env:PYTHONUTF8="1"`.
2. **Blackwell(sm_120)**: torch는 반드시 cu128 빌드. cu124는 `is_available()=True`여도 커널 실행에서 터짐.
3. **xv6 빌드는 Windows 불가**: RISC-V gcc + qemu 필요 → 맥북/리눅스/WSL2.
4. **syscall 추가는 6곳**: syscall.h(번호) / syscall.c(extern+배열) / sysproc.c(함수) / defs.h / usys.pl / user.h. 새 유저 프로그램은 Makefile UPROGS.
5. **proc 테이블 락 순서**: `wait_lock` → `p->lock`. parent 접근은 wait_lock 안에서.
6. **SYSTEM_PROMPT 동기화**: `ml/scripts/train.py`·`evaluate.py`·`ask.py`·`host/executor.py` 4곳이 다르면 평가 불공정 + 모델 동작 틀어짐.
7. **ml/models/는 gitignore**: 수 GB. 커밋 금지.
8. **legacy/는 frozen archive**: hyunsung 단독 백업본. 내부 경로(smart-mlfq-*)는 그 시점 그대로 — 건드리지 말 것.

---

## 현재 상태 (2026-06)

| 단계 | 상태 |
|---|---|
| 데이터 20인텐트 (한/영 ~반반, train 5951/test 1584) | ✅ |
| 학습 (qwen-3b r64 e10) | ✅ 산출물 존재 (정확도는 evaluate.py 실행 필요) |
| 커널 reap + 유저 killall/killheavy/reap/uptime/sysinfo/tracepid | ✅ 작성 (맥북 빌드 검증 대기) |
| M3 executor + 안전가드 | ✅ 오프라인 검증 |
| M4 agent (추상요청 분해) | ✅ 오프라인 검증 |
| 라이브 자동실행(QEMU 드라이버 연결) | ⬜ 빌드 통과 후 연결 |

## 범위 밖 (xv6에 기능 자체가 없음 — 추가 불가/비현실적)
네트워크(다운로드), 유저/권한(sudo/chmod), 서비스/패키지매니저, 프로세스 일시정지(SIGSTOP).
