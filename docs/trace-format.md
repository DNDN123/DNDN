# Trace Log Format

xv6 커널이 콘솔로 emit하는 두 종류의 라인 — **`TRACE`**(스케줄링 이벤트) 와
**`EXIT`**(프로세스 종료 통계) — 의 정식 명세. `parse_trace.py`가 이 두 형식의
정규식을 기준으로 JSON을 만들고, 그 JSON을 `llm_hint.py / evaluator.py / viz.py` 가
공통 입력으로 사용합니다.

## emit 주체

두 라인 모두 `legacy/smart-mlfq-xv6-patches/kernel/proc.c` 의 두 지점에서 출력됩니다:

| 라인 | emit 위치 | 조건 |
|---|---|---|
| `TRACE` | `scheduler()` — `swtch()` 가 반환된 직후 | `p->trace_enabled == 1` |
| `EXIT`  | `kexit()` — 정리 직전 | `p->trace_enabled == 1` |

`p->trace_enabled` 는 `settrace(pid, on)` syscall 또는 `nlrun` 의 부트스트랩으로
켜고, `kfork()/kforkpri()` 는 부모 값을 자식이 상속합니다.

## `TRACE` 라인

```
TRACE tick=<TICK> pid=<PID> pri=<LEVEL> slice=<SLICE>
```

| 필드 | 타입 | 의미 |
|---|---|---|
| `tick`  | uint | 스케줄러가 이 프로세스를 선택한 시점의 `ticks` 값 |
| `pid`   | int  | 선택된 프로세스의 pid |
| `pri`   | 0\|1\|2 | 큐 레벨 (0=HIGH, 1=MID, 2=LOW) |
| `slice` | int  | 이 quantum 진입 시점의 누적 `time_slice_used` (보통 0; demotion 후 0으로 리셋) |

**한 줄 = 한 번의 스케줄링 결정.** 같은 pid 가 연속해서 여러 tick 동안 도는
경우에도 라인은 quantum 시작마다 한 번씩만 찍힙니다.

예:
```
TRACE tick=42 pid=4 pri=0 slice=0
TRACE tick=44 pid=4 pri=0 slice=0
TRACE tick=46 pid=5 pri=1 slice=0
```

## `EXIT` 라인

```
EXIT pid=<PID> name=<NAME> arrival=<A> completion=<C> run=<R> ioblock=<I> final_pri=<L>
```

| 필드 | 타입 | 의미 |
|---|---|---|
| `pid`         | int | 종료한 프로세스의 pid |
| `name`        | str | `p->name` (16 byte). 공백·제어문자 없음 — 정규식이 `\S+` 로 받음 |
| `arrival`     | int | `allocproc()` 시점의 tick (생성 시간) |
| `completion`  | int | `kexit()` 진입 시점의 tick |
| `run`         | int | `total_run_ticks` — 누적 CPU tick |
| `ioblock`     | int | `total_io_blocks` — 진짜 I/O sleep 횟수 (kwait·`&ticks` sleep 제외) |
| `final_pri`   | 0\|1\|2 | 종료 시점 큐 레벨 |

예:
```
EXIT pid=4 name=cpu_burner arrival=8 completion=42 run=34 ioblock=0 final_pri=2
EXIT pid=5 name=io_burner  arrival=10 completion=51 run=6  ioblock=20 final_pri=0
```

## 노이즈 허용

`parse_trace.py` 는 각 줄에 위 정규식을 `re.search` 하므로, **앞뒤로 어떤 텍스트가
섞여 있어도 한 줄에 형식만 들어있으면 인식됩니다**. xv6 부트 메시지, 다른 printf,
ANSI escape 등이 같은 줄에 끼어도 무해.

## 호환성 규칙 (다른 슬라이스가 라인 emit 시)

다른 팀원이 자기 슬라이스에서 추가 라인을 찍을 때는:
- 위 두 prefix(`TRACE `, `EXIT `)는 **건드리지 말 것** — 파서가 가로채갑니다.
- 새 종류를 추가하고 싶으면 새 prefix(예: `TRC_THREAD`, `STATS`)를 사용하고
  `parse_trace.py` 에 정규식을 등록.
- 필드 순서·이름을 바꾸려면 본 문서와 `parse_trace.py` 의 정규식·`evaluator.py`
  의 사용처를 같은 PR에서 일괄 갱신.

## JSON 스키마

`parse_trace.py` 가 출력하는 JSON:

```json
{
  "source_log": "qemu_baseline.log",
  "n_traces": 1208,
  "n_exits": 7,
  "events": [
    {"kind": "trace", "tick": 42, "pid": 4, "pri": 0, "slice": 0},
    ...
    {"kind": "exit", "pid": 4, "name": "cpu_burner",
     "arrival": 8, "completion": 42, "run": 34, "ioblock": 0, "final_pri": 2}
  ]
}
```
