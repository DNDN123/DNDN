# 팀 통합 발표 대본 — LLM for OS (진행상황 데모)

분량: 약 6분. 슬라이드 7장 (아키텍처 + 4인 슬라이스 + 정량 평가 + 향후 계획).
각 슬라이스는 담당자가 본인 슬라이드를 직접 발표.

---

## 도입

안녕하세요. 저희 4명은 *"LLM for OS"* 방향으로, **xv6 운영체제 안에 자연어 인터페이스를 만드는 프로젝트**를 진행하고 있습니다.

OS의 4가지 핵심 개념 — **스케줄러, 프로세스, 시스템콜, 스레드** — 을 한 명씩 나눠 맡았고, 각자 슬라이스를 구현해 *Hello World* 시연을 통과했습니다. 그 뒤 baseline / heuristic / Solar LLM 3-way 정량 비교를 마쳤고, 오늘 그 결과를 함께 보여드립니다.

---

## 슬라이드 1 — 통합 아키텍처

```
┌─────── HOST (Python) ─────────────────────────────────┐
│  User NL  ──▶  Solar Pro 3  ──▶  spec/intent JSON     │
│                                        │              │
│                              ┌─────────┴──────────┐   │
│                              │ 검증 + 안전 가드   │   │
│                              └─────────┬──────────┘   │
│                                        ▼              │
│                              "nlrun 2 cpu_burner ..." │
└────────────────────────────────────────┼──────────────┘
                                         ▼
┌─────── xv6 KERNEL ────────────────────────────────────┐
│  ┌───────────┐  ┌──────────┐  ┌──────────┐ ┌──────┐   │
│  │ 스케줄러  │  │ 프로세스 │  │ 시스템콜 │ │스레드│   │
│  │  3-MLFQ   │  │  ps/sys_ps  │ syscall  │ │worker│   │
│  │ forkpri   │  │  setprio │  │  trace   │ │ pool │   │
│  │ (조현성)  │  │ (haneol) │  │  (minju) │ │(jinhwan)│
│  └───────────┘  └──────────┘  └──────────┘ └──────┘   │
└───────────────────────────────────────────────────────┘
```

### 무엇을 말하는가

프로젝트 초기에 합의한 큰 그림입니다. 사용자의 자연어가 Solar API를 거쳐 JSON으로 구조화되고, 호스트 측에서 검증과 안전 가드를 통과한 뒤에 xv6 안의 4개 슬라이스로 흘러갑니다. **LLM은 절대 커널 안에 들어오지 않습니다** — 시스템콜 경계에서 작은 정수나 짧은 명령으로 축약된 뒤에만 커널이 받아들입니다.

---

## 슬라이드 2 — 스케줄러 (~60초, 본인)

```python
# nl_shell.py — 자연어를 큐 레벨로 번역
def translate(user_text):
    spec = call_solar(user_text, NL_TO_SPEC_PROMPT)
    if spec is None:
        spec = heuristic(user_text)        # ★ Solar 죽어도 결정론적 폴백
    assert spec.queue in {0, 1, 2}         # ★ LLM 격리: 2비트만 커널로
    return f"nlrun {spec.queue} {spec.cmd} {spec.args}"
```

```c
// xv6: user/nlrun.c
int pid = forkpri(level);                  // ★ fork→setpri race 제거
if (pid == 0) exec(prog, args);

// kernel/proc.c — 3-단계 MLFQ
const int slice_limit[3] = {2, 4, 8};      // HIGH=2, MID=4, LOW=8 ticks
// time slice 초과 시 demote, 주기적 boost로 starvation 방지
```

### 무엇을 말하는가

스케줄러 파트는 자연어를 받아서 *어느 큐에서 실행할지* 결정하는 흐름입니다. Solar가 응답을 못 줘도 결정론적 키워드 폴백이 작동하기 때문에 데모가 깨지지 않습니다. 핵심 설계는 **LLM의 출력을 시스템콜 경계에서 0, 1, 2 중 하나의 정수로 축약**한다는 점입니다. 커널은 LLM의 존재를 모릅니다.

xv6 안에는 3-단계 MLFQ를 구현했고, time slice를 다 쓰면 demote, 주기적 boost로 starvation을 방지합니다. **잘못된 힌트가 와도 표준 demotion 규칙이 몇 틱 안에 자동 교정**해 줍니다.

---

## 슬라이드 3 — 프로세스 (~50초, haneol)

```python
# host/nl_bridge.py — Intent 분류 + 안전 가드
def parse_nl(text) -> Intent:
    return Intent(type, args, reason)      # PS|SETPRIO|SPAWN|KILL|EXPLAIN|REJECT

def guard(intent):
    if intent.type == KILL and intent.args.pid in {0, 1}:
        return "refuse kill init"          # ★ init 보호
    if intent.type == SPAWN and cmd not in WHITELIST:
        return "not whitelisted"
```

```c
// kernel/sysproc.c — 프로세스 introspection
sys_ps(user_buf, n) {
    for (p in proc_table where p.state != UNUSED)
        copyout(buf, {p.pid, p.ppid, p.state, p.priority, p.name});
}
```

### 무엇을 말하는가

프로세스 파트는 LLM이 시스템을 *볼 수 있게* 만드는 일입니다. 사용자의 자연어를 6가지 Intent로 분류하고, 위험한 요청은 안전 가드에서 거부합니다. 특히 `pid=1` (init) 종료는 무조건 차단합니다. xv6 안에는 `sys_ps` 시스템콜을 추가해서 프로세스 테이블 정보를 사용자 공간으로 copyout 합니다.

---

## 슬라이드 4 — 시스템콜 (~50초, minju)

```c
// kernel/syscall.c — 한 지점에서 모든 syscall 후킹
void syscall(void) {
    int num = p->trapframe->a7;
    int trackable = (p->traced && num < 32 &&
                     num != SYS_trace_on);     // 관찰자 효과 방지

    if (trackable) p->syscall_count[num]++;
    ret = syscalls[num]();
    if (trackable && ret < 0) p->syscall_errors[num]++;
}

// user/tracetool dump 출력 → LLM 진단으로 흘려보낼 JSON
// {"pid":4,"total":18,"calls":{"fork":1,"exec":2,"read":3,"write":5,...}}
```

### 무엇을 말하는가

시스템콜 파트의 핵심은 *"모든 syscall이 거치는 한 지점"* 에 후킹을 거는 것입니다. 호출/실패 횟수를 자동 누적하기 때문에 새 syscall이 추가돼도 추적 코드는 손볼 필요 없습니다. 추적 제어 syscall 자체는 후킹 조건에서 제외해 **관찰자 효과**를 막았습니다. `tracetool dump` 출력은 *그대로 LLM 진단 모듈로 흘려보낼 수 있는 JSON 형식*입니다.

---

## 슬라이드 5 — 스레드 (~50초, jinhwan)

```c
// thread.c — Linux clone(2) 기반 user thread
t->tid = clone(fn, stack, CLONE_VM | CLONE_FS | CLONE_FILES, arg);

// mutex.c — futex 기반 (Drepper 패턴)
if (CAS(&m->state, 0, 1)) return;          // ★ fast path: syscall 없음
futex_wait(&m->state, 2);                   // 경합 시에만 커널 sleep

// worker_pool.c — 3-level 우선순위 큐
task = dequeue_highest_priority(&p->queue); // HIGH(대화형) → NORMAL → LOW
task.fn(task.arg);                          // 예: Solar API 동시 호출
```

### 무엇을 말하는가

스레드 파트에서는 `pthread` 라이브러리 없이 **Linux의 `clone(2)`과 `futex(2)` 시스템콜을 직접 호출**해서 유저 스레드와 동기화를 만들었습니다. mutex는 경합 없으면 CAS 한 번으로 끝나는 fast path가 있습니다. 이 인프라 위에 **우선순위 워커 풀**을 얹어서 LLM 호출 동시성을 확보했습니다.

---

## 슬라이드 6 — 정량 평가 결과 ⭐ (~70초, 본인)

```
3-way 비교 (baseline = MLFQ만 / heuristic = 폴백 힌트 / LLM = Solar 힌트)

워크로드      | baseline   heuristic   LLM      | 해석
─────────────|──────────────────────────────────|──────────────────────────
cpu_heavy    |   1.2        1.4         1.6     | 모두 비슷
io_heavy     |  41.25      45.5        46.75    | baseline 최고
                                                   (sleep retention 자동)
mixed   ⭐   |  19.71  →  30.57   →    24.43    | LLM이 heuristic 능가
three_way ⭐ |  82.33      82          80.67    | LLM이 모든 모드 중 최고
realprog     |   7.2        7.8         7.4     | LLM이 heuristic 능가

mixed 워크로드 cpu_burner 개별 (avg turnaround):
   baseline 1.33  →  heuristic 21.0 (+1478%)  →  LLM 6.67 (+401%)
   → heuristic은 cpu_burner 굶주림; LLM은 더 똑똑한 trade-off
```

### 무엇을 말하는가

정량 평가에서는 5개 워크로드 × 3 모드 = 15회 QEMU 실험을 돌렸습니다. 결과가 의외로 의미 있어요. **단일 워크로드(cpu_heavy, io_heavy)는 baseline MLFQ가 이미 최적**입니다 — 우리가 구현한 sleep retention이 이미 잘 작동하기 때문이죠.

**진짜 흥미로운 발견은 mixed와 three_way 워크로드입니다.** mixed에서는 도메인 지식 없이 동작하는 heuristic이 cpu_burner를 무조건 LOW로 보내서 굶주리게 한 반면, **LLM은 cpu_burner를 6.67 tick으로 처리** — heuristic의 21 tick보다 명확히 좋습니다. three_way에서는 LLM이 세 모드 중 가장 좋은 80.67 tick을 기록했습니다.

이건 *"비싼 LLM 호출이 실제로 가치 있는 지점"* 을 정량적으로 보여주는 결과입니다. 차트는 `docs/charts/standalone/`에 commit되어 있습니다.

---

## 슬라이드 7 — 향후 계획 (~30초, 본인)

| 주차 | 작업 |
|---|---|
| **통합 주** | 4명 슬라이스 통합 — syscall 번호 재배정 + MLFQ 단일화 + NL 브리지 통합 |
| **마지막 주** | 영문 기술 보고서 + 영문 발표 슬라이드 + 백업 데모 영상 |

### 무엇을 말하는가

통합 주에는 네 명의 코드를 하나의 xv6 트리에 통합합니다. 가장 큰 작업은 syscall 번호 재배정과 두 MLFQ 구현 중 본인 것으로 단일화, 그리고 세 개의 NL 브리지를 하나로 합치는 일입니다. 그 다음 주차에 영문 보고서와 데모 영상을 마무리할 계획입니다.

---

## 마무리

지금까지 구현 + 정량 평가 결과를 보여드렸습니다. *"LLM이 항상 더 좋다"* 가 아니라 *"LLM이 가치 있는 워크로드를 찾았다"* 는 정직한 결과를 얻었습니다. 질문 받겠습니다. 감사합니다.

---

## 📋 발표 운영 메모

- **시간 배분 (총 ~6분):** 도입 20s · 아키텍처 30s · 본인 60s · haneol 50s · minju 50s · jinhwan 50s · 정량평가 70s · 향후 30s · 마무리 20s
- **정량 결과 차트:** `docs/charts/standalone/{cpu_heavy,io_heavy,mixed,three_way,realprog}/avg_turnaround.png` 사용 권장
- **시연 영상:** 시간 여유 시 슬라이드 2 직후에 30초 자연어→실행 데모 GIF 1개 삽입
- **Q&A 대비:** *"왜 LLM이 커널에 안 들어가나"*, *"폴백이 왜 휴리스틱인가"*, *"왜 io_heavy에서 LLM이 더 나쁜가"*, *"통합 시 syscall 번호 충돌은 어떻게 푸나"* — 답변 미리 합의
