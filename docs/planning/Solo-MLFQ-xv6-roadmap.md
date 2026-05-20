# Solo Smart-MLFQ on xv6 — 1인 6주 로드맵

> **프로젝트**: xv6-riscv 커널의 스케줄러를 MLFQ로 확장하고, 호스트 Python 스크립트가 LLM(Solar Pro 3)을 통해 우선순위 힌트를 제공
> **혼자 진행, 6주, 학기 프로젝트 + 포트폴리오용**
> **언어**: 커널 (C, RISC-V), 호스트 브릿지 (Python)
> **OS**: xv6-riscv (MIT, QEMU 에뮬레이션)

---

## 1. 왜 xv6인가 — 그리고 무엇이 바뀌는가

Python 시뮬레이션 버전과의 차이:

| 항목 | Python 시뮬 | xv6 (이 버전) |
|---|---|---|
| OS 본질 | 시뮬레이터 (가짜 OS) | **진짜 커널 코드 수정** |
| 평가 임팩트 | 보통 | **매우 큼** (*"진짜 OS를 만졌다"*) |
| 강의 정렬 | 부분적 | **xv6 기반 강의의 정점** |
| 디버깅 | 쉬움 | 어려움 (kernel panic, qemu+gdb) |
| LLM 통합 | 직접 호출 | **호스트 Python 브릿지 필요** |
| 솔로 시간 | ~60h | **~80h** (디버깅 오버헤드 +30%) |
| 포트폴리오 가치 | 보통 | **매우 큼** |

**xv6 선택의 결정적 장점**: 강의에서 이미 xv6를 다뤄봤고 (Week 3 trace syscall, Week 6 scheduler 분석), 이걸 확장하는 것이라 *"학기 학습의 정점"* 으로 보임. 면접관에게도 *"OS 수업 마지막 프로젝트로 xv6 커널을 직접 수정해 MLFQ 스케줄러를 만들고 LLM과 결합했다"* 한 줄이 강력함.

---

## 2. 시스템 아키텍처

```
┌────────────────────────────────────────────────────────────┐
│                    QEMU (xv6-riscv)                        │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Modified xv6 Kernel                                 │  │
│  │  ├─ struct proc 확장 (priority, queue_level, stats)  │  │
│  │  ├─ MLFQ scheduler() (kernel/proc.c)                 │  │
│  │  ├─ 타이머 인터럽트 핸들러 (kernel/trap.c)           │  │
│  │  ├─ 새 syscalls: setpri, getstats, trace             │  │
│  │  └─ 트레이스 로거 (printf or 전용 buffer)            │  │
│  ├──────────────────────────────────────────────────────┤  │
│  │  User Programs (workloads)                           │  │
│  │  ├─ cpu_burner.c (CPU-bound)                         │  │
│  │  ├─ io_burner.c (I/O-bound)                          │  │
│  │  └─ workload_runner.c (시나리오 실행)                │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ↑↓                                │
│                  콘솔 입출력 (UART)                        │
└──────────────────────────┬─────────────────────────────────┘
                           │ stdout/stdin via QEMU
                           ▼
┌────────────────────────────────────────────────────────────┐
│                      Host Python                           │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Trace Collector                                     │  │
│  │  - QEMU stdout 캡처 → 파싱 → trace.json              │  │
│  ├──────────────────────────────────────────────────────┤  │
│  │  LLM Hint Module (Solar Pro 3)                       │  │
│  │  - trace.json 분석                                   │  │
│  │  - 프로세스별 우선순위 추천                          │  │
│  │  - hints.json 출력                                   │  │
│  ├──────────────────────────────────────────────────────┤  │
│  │  Priority Injector                                   │  │
│  │  - hints.json → xv6 부팅 시 setpri syscall로 적용    │  │
│  ├──────────────────────────────────────────────────────┤  │
│  │  Evaluator                                           │  │
│  │  - turnaround / response / throughput 계산           │  │
│  │  - matplotlib 차트                                   │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────┘
```

### 두 단계 실험 흐름

**Phase 1 — Baseline 측정**
1. xv6 부팅, MLFQ 스케줄러 활성 (LLM 없이)
2. workload_runner가 5~10개 프로세스 fork
3. 스케줄러가 트레이스를 콘솔로 출력
4. 호스트 Python이 트레이스 캡처 → `baseline_trace.json`

**Phase 2 — LLM 추천 적용**
1. 호스트 Python이 baseline_trace.json을 LLM에 전달
2. LLM이 *"PID 1은 CPU-bound라 LOW 큐, PID 2는 I/O-bound라 HIGH 큐"* 추천
3. `hints.json` 생성
4. xv6 재부팅, workload_runner가 hints를 읽고 setpri syscall로 초기 우선순위 설정
5. 다시 실행, `llm_trace.json` 캡처

**Phase 3 — 비교**
- Python 평가 스크립트가 두 trace 비교
- 차트 출력

---

## 3. xv6 측 구체적 수정 사항

### 3.1 struct proc 확장 (`kernel/proc.h`)

```c
struct proc {
  // ... 기존 필드 ...
  
  // === MLFQ 추가 필드 ===
  int priority;          // 현재 큐 레벨: 0(HIGH) / 1(MID) / 2(LOW)
  int time_slice_used;   // 현재 큐에서 사용한 ticks
  int total_run_ticks;   // 누적 CPU 실행 ticks
  int total_io_blocks;   // I/O로 블록된 횟수
  int arrival_tick;      // 도착 시각
  int completion_tick;   // 종료 시각
};
```

### 3.2 MLFQ 스케줄러 (`kernel/proc.c`)

기존 `scheduler()` 의 round-robin 부분을 교체:

```c
// MLFQ 큐 (간단히 priority별 RUNNABLE 프로세스 순회)
// xv6는 별도 큐 자료구조가 없고 ptable[] 순회 방식이라
// "우선순위 0 RUNNABLE 먼저, 없으면 1, 없으면 2" 순으로 찾기

void scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for (;;) {
    intr_on();
    
    int found_level = -1;
    
    // 가장 높은 우선순위 RUNNABLE 프로세스 찾기
    for (int level = 0; level < 3; level++) {
      for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state == RUNNABLE && p->priority == level) {
          // 실행
          p->state = RUNNING;
          c->proc = p;
          swtch(&c->context, &p->context);
          c->proc = 0;
          found_level = level;
          release(&p->lock);
          break;
        }
        release(&p->lock);
      }
      if (found_level >= 0) break;
    }
    
    // boost: 100 ticks마다 모든 프로세스를 HIGH로
    if (ticks > 0 && ticks % 100 == 0) {
      for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        p->priority = 0;
        p->time_slice_used = 0;
        release(&p->lock);
      }
    }
  }
}
```

### 3.3 타이머 인터럽트에서 demotion (`kernel/trap.c`)

`yield()` 호출 전에 time slice 확인 + demotion:

```c
// kernel/trap.c, usertrap()/kerneltrap() 의 timer interrupt 처리부
if (which_dev == 2) {  // timer interrupt
  if (myproc()) {
    myproc()->time_slice_used++;
    myproc()->total_run_ticks++;
    
    // 큐별 time slice: HIGH=2, MID=4, LOW=8 ticks
    int slice_limits[] = {2, 4, 8};
    int my_pri = myproc()->priority;
    
    if (myproc()->time_slice_used >= slice_limits[my_pri]) {
      // demotion (LOW에서는 더 못 내려감)
      if (my_pri < 2) myproc()->priority++;
      myproc()->time_slice_used = 0;
    }
  }
  yield();
}
```

### 3.4 sleep/wakeup 처리 (I/O 블록 시)

xv6에서 `sleep()` 호출 시 = I/O 블록 발생. 이때:
- `total_io_blocks++`
- `time_slice_used = 0` (큐 유지, 다시 그 큐의 처음부터)

`kernel/proc.c` 의 `sleep()` 함수에 추가:
```c
void sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();
  p->total_io_blocks++;
  p->time_slice_used = 0;  // I/O 블록은 demotion 면제
  // ... 기존 코드 ...
}
```

### 3.5 새 시스템 콜

#### `setpri(int pid, int priority)` — LLM 추천 우선순위 설정

```c
// kernel/sysproc.c
uint64 sys_setpri(void) {
  int pid, priority;
  argint(0, &pid);
  argint(1, &priority);
  
  if (priority < 0 || priority > 2) return -1;
  
  for (struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid) {
      p->priority = priority;
      p->time_slice_used = 0;
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}
```

#### `getstats(int pid, struct procstats *stats)` — 통계 조회

```c
struct procstats {
  int pid;
  int priority;
  int total_run_ticks;
  int total_io_blocks;
  int arrival_tick;
  int completion_tick;
};

// kernel/sysproc.c
uint64 sys_getstats(void) {
  // ...
}
```

각 syscall 추가 시 표준 절차:
1. `kernel/syscall.h` — SYS_setpri 매크로
2. `kernel/syscall.c` — extern 선언 + syscalls 배열
3. `kernel/sysproc.c` — sys_setpri 구현
4. `user/usys.S` — 어셈블리 stub
5. `user/user.h` — 사용자용 선언

(Week 3 trace syscall 과제와 동일한 패턴)

### 3.6 트레이스 출력

매 스케줄링 결정마다 콘솔에 한 줄:
```c
// scheduler() 안, swtch 직전
printf("TRACE tick=%d pid=%d pri=%d slice=%d\n",
       ticks, p->pid, p->priority, p->time_slice_used);
```

종료 시:
```c
// exit() 안
printf("EXIT pid=%d arrival=%d completion=%d run=%d ioblock=%d\n",
       p->pid, p->arrival_tick, p->completion_tick,
       p->total_run_ticks, p->total_io_blocks);
```

---

## 4. 호스트 Python 측 구체적 수정 사항

### 4.1 디렉토리 구조

```
smart-mlfq-xv6/
├── README.md
├── xv6-riscv/                   # xv6 서브모듈 (수정한 커널)
│   ├── kernel/
│   │   ├── proc.c               # MLFQ 구현
│   │   ├── proc.h               # struct proc 확장
│   │   ├── trap.c               # demotion 로직
│   │   ├── sysproc.c            # setpri, getstats
│   │   └── ...
│   └── user/
│       ├── cpu_burner.c
│       ├── io_burner.c
│       ├── workload_runner.c    # 시나리오 실행
│       └── ...
├── host/                        # 호스트 Python
│   ├── trace_collector.py       # QEMU stdout 캡처 → JSON
│   ├── llm_hint.py              # Solar API + 프롬프트
│   ├── prompts.py
│   ├── injector.py              # hints.json → 다음 부팅 적용
│   ├── evaluator.py             # 메트릭 계산
│   └── viz.py                   # 차트
├── workloads/
│   ├── cpu_heavy.json           # 시나리오 정의
│   └── io_heavy.json
├── results/
│   ├── baseline_trace.json
│   ├── llm_trace.json
│   └── charts/
├── scripts/
│   └── run_experiment.sh        # 한 방에 baseline + LLM 비교
└── docs/
    ├── proposal.md
    ├── architecture.md
    └── report.md
```

### 4.2 Trace Collector

```python
# host/trace_collector.py
import subprocess, json, re

def run_xv6_with_workload(workload_path: str, output_trace: str):
    """xv6를 QEMU에서 실행, 콘솔 출력에서 TRACE/EXIT 라인 캡처"""
    # workload는 xv6 안에서 자동 실행 (init이나 sh 스크립트로)
    
    proc = subprocess.Popen(
        ["make", "-C", "xv6-riscv", "qemu", f"WORKLOAD={workload_path}"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    
    traces = []
    exits = []
    for line in proc.stdout:
        if line.startswith("TRACE"):
            traces.append(parse_trace_line(line))
        elif line.startswith("EXIT"):
            exits.append(parse_exit_line(line))
        if "ALL_DONE" in line:
            proc.kill()
            break
    
    with open(output_trace, "w") as f:
        json.dump({"traces": traces, "exits": exits}, f)

def parse_trace_line(line):
    # "TRACE tick=42 pid=3 pri=1 slice=2" → dict
    m = re.match(r"TRACE tick=(\d+) pid=(\d+) pri=(\d+) slice=(\d+)", line)
    return {"tick": int(m.group(1)), "pid": int(m.group(2)),
            "pri": int(m.group(3)), "slice": int(m.group(4))}
```

### 4.3 LLM Hint Module

```python
# host/llm_hint.py
from openai import OpenAI
import json, os
from prompts import PRIORITY_PROMPT

client = OpenAI(api_key=os.getenv("UPSTAGE_API_KEY"),
                base_url="https://api.upstage.ai/v1")

def analyze_traces(baseline_trace: dict) -> dict:
    """trace 분석 후 PID별 추천 priority 반환"""
    # 프로세스별 통계 집계
    process_stats = {}
    for exit_event in baseline_trace["exits"]:
        pid = exit_event["pid"]
        run = exit_event["run"]
        ioblock = exit_event["ioblock"]
        process_stats[pid] = {
            "run_ticks": run,
            "io_blocks": ioblock,
            "io_ratio": ioblock / max(run, 1)
        }
    
    # LLM에 통계 전달
    prompt = PRIORITY_PROMPT.format(stats=json.dumps(process_stats, indent=2))
    response = client.chat.completions.create(
        model="solar-pro2",
        messages=[{"role": "user", "content": prompt}],
        temperature=0.0,
    )
    
    hints = json.loads(response.choices[0].message.content)
    return hints  # {"1": 2, "2": 0, ...}  (pid → priority)
```

### 4.4 워크로드 정의

```json
// workloads/cpu_heavy.json
{
  "scenario": "cpu_heavy",
  "processes": [
    {"name": "cpu_burner", "args": ["1000000"], "spawn_at": 0},
    {"name": "cpu_burner", "args": ["500000"], "spawn_at": 5},
    {"name": "io_burner", "args": ["100"], "spawn_at": 10}
  ]
}
```

xv6 의 `workload_runner.c` 가 이 시나리오를 읽고 fork/exec.

(시나리오를 xv6 파일시스템에 어떻게 넣을지 — Makefile에 추가하거나, 처음에 호스트에서 string 으로 컴파일해 넣음)

---

## 5. 6주 일정 — 솔로 80시간 견적

### Week 1 (8h) — xv6 환경 + 분석

**목표**: xv6 빌드·실행 가능, 스케줄러 코드 깊이 읽기

**작업**
- xv6-riscv 빌드 환경 셋업 (qemu, riscv64-elf-gcc) (3h)
  - macOS: `brew install qemu riscv64-elf-gcc` 또는 cross-compile chain
  - Ubuntu/WSL2: `apt install qemu-system-misc gcc-riscv64-unknown-elf`
  - Week 2 lab 의 setup_xv6_env.md 참고
- xv6 부팅 + 기본 명령 (`ls`, `cat`) 확인 (1h)
- `kernel/proc.c` `scheduler()` 함수 읽기 + 주석 달기 (2h)
- `kernel/trap.c` 타이머 인터럽트 흐름 추적 (1h)
- `docs/architecture.md` 초안 (1h)

**산출물**
- [ ] xv6 빌드·부팅 성공
- [ ] `scheduler()` 흐름 이해 메모 (`docs/notes-scheduler.md`)
- [ ] GitHub repo + xv6-riscv submodule + README + proposal

---

### Week 2 (12h) — struct proc 확장 + 단순 priority 스케줄러

**목표**: priority 필드 추가하고, 단순 *고정 priority* 스케줄러 동작

**작업**
- `struct proc` 에 priority/stats 필드 추가 (1h)
- `allocproc()` 에서 priority=0(HIGH) 기본값 (1h)
- `scheduler()` 를 *"priority 가장 낮은 프로세스 먼저"* 로 단순 변경 (3h)
  - 아직 demotion·boost 없음, 그냥 priority 따라 선택만
- `setpri` syscall 추가 (전체 절차) (3h)
- 테스트: 두 user program (P1 priority=0, P2 priority=2) 동시 실행 → P1 먼저 (2h)
- 디버깅 (2h)

**산출물**
- [ ] xv6 안에서 setpri syscall 호출 → 우선순위 변경 동작
- [ ] 트레이스 출력 (`printf` from scheduler) 확인

---

### Week 3 (16h) — MLFQ 코어 ⚠️ 가장 위험한 주

**목표**: 3-level MLFQ + demotion + boost 완성

**작업**
- 3-level priority 순회 로직 (3h)
- 타이머 인터럽트에서 time_slice_used 증가 + demotion (4h)
- I/O 블록 시 time_slice_used 리셋 (sleep() 수정) (2h)
- 주기적 priority boost (3h)
- 테스트 시나리오: CPU-bound 3개 + I/O-bound 2개 → 콘솔 트레이스 보면서 큐 이동 확인 (3h)
- 디버깅 (1h)

**산출물**
- [ ] **3-level MLFQ 동작**: 트레이스에서 프로세스가 HIGH→MID→LOW 이동 보임
- [ ] I/O 블록 후 같은 큐 유지 확인
- [ ] Boost 동작 확인 (모두 HIGH로 복귀)
- [ ] 단위 테스트용 user program 5개

⚠️ **이번 주가 분기점.** 막히면 즉시 비상:
- 3-level → 2-level 단순화
- Demotion 단순화 (slice 초과 → 무조건 LOW)

---

### Week 4 (14h) — 트레이스 출력 + 호스트 Python 셋업

**목표**: xv6 트레이스를 호스트에서 캡처, 파싱, JSON 저장

**작업**
- 트레이스 출력 포맷 확정 (TRACE / EXIT 라인) (2h)
- xv6 안에 `workload_runner.c` 작성 — JSON 워크로드 읽고 fork/exec (4h)
  - xv6는 JSON 파서가 없으므로 매우 단순한 포맷 사용 (예: `name args spawn_at` 한 줄씩)
- 호스트 Python `trace_collector.py` 작성 (3h)
- `cpu_burner.c`, `io_burner.c` 작성 (2h)
- 첫 baseline 실험: cpu_heavy 시나리오 실행 → 트레이스 JSON 저장 (3h)

**산출물**
- [ ] `python host/trace_collector.py --workload cpu_heavy.json --output baseline.json` 동작
- [ ] baseline.json 에 100+ TRACE 라인, 5+ EXIT 라인 들어 있음

---

### Week 5 (16h) — LLM 통합 + 비교 실험

**목표**: LLM 분석 + 우선순위 추천 + 두 번째 실험 + 비교

**작업**
- `host/llm_hint.py` 작성: Solar API 호출, JSON 응답 파싱 (4h)
- `host/prompts.py` 작성: priority recommendation 프롬프트 (2h)
- workload_runner가 hints.json을 읽어서 setpri 호출하도록 수정 (3h)
- `host/evaluator.py`: turnaround / response / throughput 계산 (3h)
- 두 시나리오 (cpu_heavy, io_heavy) baseline + LLM 실험 (2h)
- `host/viz.py`: 비교 막대그래프 (2h)

**산출물**
- [ ] `scripts/run_experiment.sh` 한 방 실행
- [ ] `results/charts/` 안에 비교 차트 4장 이상
- [ ] LLM 추천이 적용된 두 번째 실행 트레이스 (`llm_trace.json`)

---

### Week 6 (14h) — 마무리 + 문서·데모

**목표**: README 정성껏, 데모 영상, 보고서

**작업**
- 코드 리팩터링 + 주석 (3h)
- README 풀버전 (문제·접근·결과 차트·실행법·시연) (3h)
- xv6 빌드 가이드 (`docs/setup.md`) (2h)
- 데모 영상 녹화 (xv6 부팅 + 트레이스 출력 + 차트) (2h)
- 최종 보고서 `docs/report.md` (3h)
- 블로그 글 1개 (선택) (1h)

**산출물**
- [ ] 포트폴리오 수준 README + 차트 임베드
- [ ] 데모 영상 (3분 내외)
- [ ] 최종 보고서

---

## 6. 핵심 리스크 + 대응 (xv6 특화)

### 리스크 1: xv6 빌드 환경 셋업 실패

**증상**: `make qemu` 가 안 됨 (cross-compiler 없음, qemu 버전 안 맞음 등)

**대응**:
- macOS: Docker 이미지 사용 (`mit-pdos/xv6-riscv-fall19` 같은 Dockerfile)
- Ubuntu/WSL2 가 가장 안정적. 환경 안 맞으면 WSL2 설치
- Week 2 lab의 `setup_xv6_env.md` 참고

### 리스크 2: 커널 panic 디버깅

**증상**: xv6가 부팅 중 panic, 어디서 죽는지 모름

**대응**:
- `qemu-system-riscv64 -s -S` 옵션 + gdb 연결
- xv6 코드에 `printf` 적극 추가 (커널에서도 가능)
- 작은 변경마다 빌드·테스트 (한꺼번에 100줄 수정 X)
- Git commit 자주 (panic 나면 직전 commit으로 rollback)

### 리스크 3: race condition (스케줄러 락)

**증상**: 가끔 hang, 가끔 잘못된 프로세스 실행

**대응**:
- xv6 의 락 패턴 따라하기 (`acquire(&p->lock)` 모든 priority 변경 전후로)
- ⭐ Week 9 sleep/wakeup 강의 내용 적극 활용
- 단순한 동기화 패턴부터 시작 (전역 락 1개로 priority 변경 보호)

### 리스크 4: 트레이스 출력이 너무 많아서 콘솔 폭주

**증상**: TRACE 라인이 너무 많아 호스트 Python이 따라가지 못함

**대응**:
- 매 tick 말고 *priority 변경 시에만* 로그
- 또는 in-kernel trace buffer (배열) 에 저장 후 `dumptrace` syscall 로 한 번에 출력

### 리스크 5: workload_runner 가 xv6 안에서 JSON 파싱

**증상**: xv6 라이브러리에 JSON 없음, 직접 짜기 부담

**대응**:
- 매우 단순한 텍스트 포맷 사용: `cpu_burner 1000000 0\nio_burner 100 5\n`
- 한 줄씩 읽어서 strtok 비슷한 단순 파서로 처리
- 또는 워크로드를 빌드 시 C 배열로 컴파일

### 리스크 6: 호스트 ↔ xv6 데이터 전달

**증상**: hints.json 을 xv6에 어떻게 넣지?

**대응 (가장 단순)**:
- 워크로드 시나리오 .txt 파일을 xv6 빌드 시 `mkfs` 로 같이 굽기 (Makefile에 추가)
- 매 실험마다 `make clean && make qemu WORKLOAD=...` 으로 새로 빌드
- 약간 느리지만 (각 실험 30초~1분) 단순함이 보장됨

---

## 7. 첫 단계 — 오늘부터

### 오늘 (1.5h)
- [ ] xv6-riscv submodule 확인 (이미 lecture-repo에 있음)
- [ ] 자기 디렉토리에 새 git repo로 clone:
  ```
  git clone https://github.com/mit-pdos/xv6-riscv.git
  cd xv6-riscv
  ```
- [ ] 빌드 환경 시도: `make qemu` 한번 돌려보기
- [ ] 안 되면 Week 2 lab `setup_xv6_env.md` 참고

### 내일 (2h)
- [ ] xv6 빌드 성공 → `ls`, `cat README` 동작 확인
- [ ] `Ctrl-A X` 로 qemu 종료 가능 확인
- [ ] `kernel/proc.c` 의 `scheduler()` 함수 정독, 한 줄씩 주석

### 이번 주말까지 (4h)
- [ ] GitHub repo `smart-mlfq-xv6` 생성
- [ ] xv6-riscv 를 submodule로 추가
- [ ] `docs/proposal.md` 작성 (1페이지)
- [ ] `docs/notes-scheduler.md` — `scheduler()` 코드 분석 메모

---

## 8. 강의 진도와 직접 연결

| 강의 주차 | 어떻게 활용 |
|---|---|
| Week 2 (Process, fork/exec) | workload_runner.c 작성 |
| Week 3 (syscall path, trace 과제) | setpri/getstats syscall 추가 |
| Week 4 (mutex, spinlock) | priority 변경 시 락 보호 |
| **Week 6 (xv6 scheduler 분석)** | ⭐ scheduler() 수정 직접 적용 |
| **Week 7 (MLFQ 이론)** | ⭐ MLFQ 알고리즘 구현 |
| Week 9 (sleep/wakeup) | I/O 블록 시 큐 유지 처리 |
| Week 10 (deadlock) | 락 순서 검증 |

이 정렬이 *"강의 학습의 정점"* 이라는 한 마디를 자연스럽게 만들어요.

---

## 9. 포트폴리오 가치 극대화 (xv6 버전)

### README의 첫 문단
```markdown
**Smart-MLFQ** is a Multi-Level Feedback Queue scheduler implemented
directly in the **xv6-riscv kernel**, augmented with priority hints
from Upstage Solar Pro 3 LLM. Process behavior traces from a baseline
run are analyzed by the LLM to recommend initial queue assignments
for the next run.

**Key engineering**: real kernel C code (~400 lines), 2 new system
calls, kernel↔userspace bridge to host Python via QEMU stdout.

**Result**: [한 줄 결과, 예: "20% improvement in turnaround time
for I/O-bound workloads"]
```

면접관이 5초 안에 *"진짜 OS 코드를 짠 사람"* 인지 확인 가능.

### 차별화 포인트
- 다른 OS 수업 프로젝트들은 거의 다 *"Python 시뮬"* 또는 *"LLM 래퍼"*
- xv6 + LLM 결합 = 이 강의에서 그 사람만 한 결과물

### 데모 영상 시나리오
1. (10초) xv6 부팅 화면
2. (30초) Baseline 실험 — 트레이스 흐름, 처음에 모두 HIGH → 점점 demotion
3. (30초) 호스트 Python이 LLM 호출 → priority 추천 출력
4. (30초) LLM 추천 적용한 두 번째 실험
5. (30초) 비교 차트 (turnaround time)
6. (20초) 한 줄 결과·결론

---

## 10. 가장 중요한 한 가지

xv6 버전에서 **Week 3 끝에 3-level MLFQ가 동작** 하는 것이 절대 분기점입니다.

- 됨 → 나머지는 호스트 Python 작업 (상대적으로 쉬움)
- 안 됨 → 즉시 비상:
  1. 3-level → 2-level 단순화
  2. Demotion 단순화 (slice 초과 → LOW로 바로)
  3. 그래도 안 되면 → Python 시뮬 버전으로 후퇴 (아쉽지만 안전)

xv6 커널 디버깅은 진짜 어려워요. 한 줄 잘못 짚으면 panic. *작은 commit, 자주 빌드, 락 신경쓰기* 이 세 가지가 생명선입니다.

---

## 마무리

이 프로젝트는 *"학기 OS 수업 마지막 결정체 + LLM 시대 트렌드 + 시스템 깊이"* 가 한 곳에 모입니다. 솔로로 끝까지 갔을 때 그 가치는 4인 Python 시뮬보다 훨씬 큽니다.

다만 **6주 솔로로 xv6 + LLM은 진짜 도전**이에요. 매주 일요일 자가진단 (*"발표가 다음 주라면 뭘 보여줄까"*) 만 정직하게 하시고, Week 3 분기점만 통과하면 나머지는 흐름대로 갑니다.

**Definition of Done** (책상에 붙이세요):
> *"xv6에서 3-level MLFQ가 돌아가고, 호스트 Python이 트레이스를 캡처해 Solar Pro 3에 전달하고, LLM 추천 priority가 두 번째 실행에 적용되고, baseline vs LLM 비교 차트 4장이 README에 있다. 그게 끝이다."*
