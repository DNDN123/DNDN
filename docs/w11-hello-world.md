# W11 Hello World 시연 — 재현 절차

자연어 한 줄을 던지면 Solar Pro 3 가 큐 레벨까지 결정해서 xv6 안에서 실행되는 흐름.
*"우리 팀이 W11 분기점을 통과했다"* 를 보여주는 가장 짧은 데모입니다.

## 사전 준비

- xv6-riscv 빌드 + qemu 부팅 환경 (WSL 또는 Linux)
- `smart-mlfq-xv6-patches/apply_patches.sh` 적용 + `make qemu` 동작
- 호스트: `smart-mlfq-host/.env` 에 `UPSTAGE_API_KEY=...` (없어도 휴리스틱으로 시연 가능)

## 1) 자연어 → 큐 레벨 결정 (호스트)

```bash
cd smart-mlfq-host
python3 nl_shell.py --once "Run a heavy job in background"
```

기대 출력:

```
  source     : solar          # API 키 없으면 heuristic
  program    : cpu_burner 1000000
  queue_hint : 2 (LOW)
  reason     : Heavy CPU work, user wants it backgrounded
  xv6 cmd    : $ nlrun 2 cpu_burner 1000000
```

핵심: **마지막 `xv6 cmd` 한 줄**이 그대로 xv6 셸에 붙여넣는 명령입니다.

## 2) xv6 안에서 실행 (QEMU)

`make qemu` 로 띄운 xv6 셸에:

```text
$ nlrun 2 cpu_burner 1000000
TRACE tick=8  pid=4 pri=2 slice=0
TRACE tick=16 pid=4 pri=2 slice=0
...
EXIT  pid=4 name=cpu_burner arrival=8 completion=80 run=72 ioblock=0 final_pri=2
nlrun: child pid=4 done (queue=2, status=0)
```

확인 포인트:
- 첫 `TRACE` 의 `pri=2` — LLM 힌트가 실제로 적용됨 (`forkpri(2)` 가 race 없이 큐 배치)
- `EXIT` 의 `final_pri=2` — CPU-bound 라서 demotion 없이 그대로 LOW 종료
- `ioblock=0` — kwait/pause sleep 이 `total_io_blocks` 에서 필터링됨 (C2 패치 효과)

## 3) 대비 케이스 — interactive 요청

```bash
python3 nl_shell.py --once "Quick check, print hello fast"
```
→ `xv6 cmd : $ nlrun 0 echo hello` → xv6 안에서 `pri=0` 로 시작, 즉시 종료.

## 4) 오프라인 안정성 데모 (API 키 빼고 같은 명령)

```bash
unset UPSTAGE_API_KEY
python3 nl_shell.py --once "Run a heavy job in background"
```
→ `source : heuristic` 으로 바뀌고 결과 spec 은 **동일**. 네트워크가 끊기거나
API 가 죽어도 데모가 안 깨진다는 증거.

## 5) 녹화 시 권장 시퀀스 (30초)

1. `python3 nl_shell.py --once "Run a heavy job in background"` 출력 보여주기 (10초)
2. xv6 셸로 전환, 위에서 받은 `nlrun ...` 붙여넣기 → `TRACE`/`EXIT` 흐름 (15초)
3. `unset UPSTAGE_API_KEY` 후 같은 명령 한 번 더 → `source: heuristic` 강조 (5초)

## 자주 깨지는 지점

| 증상 | 원인 | 대처 |
|---|---|---|
| `nlrun: forkpri failed` | `level` 이 0~2 범위 밖 | `nl_shell.py` 의 `validate_spec` 통과했는지 확인 |
| `TRACE` 가 한 줄도 안 찍힘 | `settrace(0, 1)` 호출 누락 | `nlrun.c` 가 자동으로 켜 줌 — 그래도 안 나오면 `trace_enabled` 상속 확인 |
| Solar 응답 JSON 파싱 실패 | 모델이 마크다운 펜스로 감싸 응답 | `_parse_spec` 의 펜스 제거 로직 작동, 그래도 안 되면 휴리스틱 폴백 |
| 같은 입력에 다른 답 | Solar 가 비결정적 | `temperature=0.0` 고정 + 7일 캐시 (`hints_cache.json`) 로 안정화 |

## 관련 파일

- `smart-mlfq-host/nl_shell.py` — 자연어 → spec REPL
- `smart-mlfq-host/prompts.py` — `NL_TO_SPEC_PROMPT`
- `smart-mlfq-xv6-patches/user/nlrun.c` — `forkpri` 로 큐 진입
- `smart-mlfq-xv6-patches/kernel/proc.c` — `kforkpri()` 구현
- `docs/trace-format.md` — TRACE/EXIT 라인 스펙
