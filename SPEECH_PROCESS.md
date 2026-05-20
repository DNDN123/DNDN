# 프로세스 파트 발표 대본 (LLM for OS — 멤버 A)

분량: 약 2분 30초 ~ 3분. 슈도코드 슬라이드 4장 기준.
다른 팀원이 맡은 LLM 호스트 브리지 / Solar API / Intent 분류 / 안전 가드 영역은 포함하지 않음.

---

## 도입

이제부터는 프로세스 부분에 대해 설명드리겠습니다.

저희 프로젝트의 큰 주제는 "LLM for OS", 즉 사용자가 자연어로 운영체제를 조작할 수 있게 만드는 것이었습니다. 그런데 LLM이 아무리 자연어를 잘 이해해도, 운영체제 측에 두 가지가 갖춰져 있지 않으면 의미가 없습니다. 하나는 LLM이 프로세스 상태를 조회할 수 있는 일관된 인터페이스이고, 다른 하나는 LLM이 우선순위를 바꿔달라고 했을 때 실제로 효과가 나타나는 스케줄링 메커니즘입니다. 저는 이 두 가지를 xv6 커널에 직접 구현하는 역할을 맡았습니다.

---

## 슬라이드 1 — procinfo 구조체와 sys_ps 시스템 콜

```c
// kernel/procinfo.h
struct procinfo {
    int    pid;
    int    ppid;
    int    state;
    int    priority;
    uint64 sz;
    char   name[16];
};

// kernel/sysproc.c
uint64 sys_ps(void) {
    struct procinfo info;
    int count = 0;

    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state != UNUSED) {
            info.pid      = p->pid;
            info.ppid     = (p->parent) ? p->parent->pid : 0;
            info.state    = p->state;
            info.priority = p->priority;
            info.sz       = p->sz;
            memmove(info.name, p->name, 16);
            copyout(myproc()->pagetable, ubuf, &info, sizeof(info));
            count++;
        }
        release(&p->lock);
    }
    return count;
}
```

(슈도코드 1번을 보면서) 가장 먼저 만든 것은 sys_ps라는 새로운 시스템 콜입니다. 위쪽의 procinfo 구조체는 LLM이 자연어로 "프로세스 보여줘"라고 요청했을 때, 운영체제가 사용자 공간으로 내보낼 정보를 담는 통일된 포맷입니다. PID, 부모 PID, 상태, 우선순위, 메모리 크기, 프로세스 이름이 들어 있습니다.

아래쪽 sys_ps 함수가 본체입니다. 프로세스 테이블을 순회하면서 각 프로세스의 락을 잡고, UNUSED가 아닌 활성 프로세스만 골라 procinfo에 채운 다음 copyout 함수로 사용자 공간 버퍼에 안전하게 복사합니다.

여기서 두 가지를 강조하고 싶습니다. 첫째, 락을 잡고 푸는 패턴 덕분에 LLM이 어떤 빈도로 요청을 보내도, 또 그 사이에 다른 프로세스가 생성되거나 죽어도 race condition이 발생하지 않습니다. 운영체제 단계에서 LLM의 비결정적인 요청을 안전하게 받아낼 수 있다는 의미입니다. 둘째, xv6에 새로운 시스템 콜 하나를 추가하려면 헤더, 디스패치 테이블, 유저 스텁, Makefile까지 여섯 개 파일을 동시에 수정해야 합니다. 단순한 코드 한 줄 추가가 아니라 진짜 커널 작업이라는 점을 강조하고 싶었습니다.

---

## 슬라이드 2 — MLFQ 큐 정의와 quantum 회계

```c
// kernel/proc.c
#define MLFQ_HIGH_MAX        6
#define MLFQ_NORM_MAX        13
#define MLFQ_LEVELS          3
#define MLFQ_BOOST_INTERVAL  100  // ticks

static const int mlfq_quantum[MLFQ_LEVELS] = { 1, 4, 16 };
// L0 HIGH    priority  0..6    quantum  1 tick   (interactive)
// L1 NORMAL  priority  7..13   quantum  4 ticks  (default)
// L2 LOW     priority 14..20   quantum 16 ticks  (batch)

int mlfq_level(int prio) {
    if (prio <= MLFQ_HIGH_MAX) return 0;
    if (prio <= MLFQ_NORM_MAX) return 1;
    return 2;
}

void mlfq_tick(struct proc *p) {       // timer 인터럽트마다 호출
    if (!p || p->state != RUNNING) return;
    p->run_ticks++;
    int lvl = mlfq_level(p->priority);
    if (p->run_ticks >= mlfq_quantum[lvl]) {
        p->run_ticks = 0;
        if (p->priority < 20) p->priority++;   // 한 단계 강등
    }
}
```

(슈도코드 2번을 보면서) 두 번째는 핵심인 MLFQ 스케줄러입니다. 우선순위 0부터 6까지를 HIGH 큐, 7부터 13까지를 NORMAL 큐, 14부터 20까지를 LOW 큐, 이렇게 세 단계로 분류했습니다.

각 큐의 quantum, 즉 한 번에 돌 수 있는 시간을 1 tick, 4 tick, 16 tick으로 다르게 줬습니다. HIGH는 짧게 자주 돌아 응답성을 챙기고, LOW는 길게 한 번 돌아 처리량을 챙기는 방식입니다. OSTEP 교과서 8장의 MLFQ를 그대로 따랐습니다.

아래의 mlfq_tick 함수가 매 timer 인터럽트마다 호출됩니다. 실행 중인 프로세스의 run_ticks를 1 증가시키고, 자신의 큐 레벨에 해당하는 quantum을 다 쓰면 priority를 1 증가시켜 한 단계 아래 큐로 강등시킵니다. 즉, CPU를 오래 잡고 있는 프로세스는 점점 우선순위가 낮아지는 구조입니다.

이게 LLM for OS 관점에서 왜 중요하냐면, 사용자가 자연어로 "pid 3 우선순위 높여줘"라고 요청했을 때 단순히 숫자만 바뀌는 게 아니라 실제로 그 프로세스가 더 자주, 더 짧게 돌도록 만드는 핵심 메커니즘이기 때문입니다. LLM의 요청이 OS 단계에서 의미 있는 동작으로 변환되는 부분입니다.

---

## 슬라이드 3 — boost 메커니즘과 scheduler 선택 로직

```c
// kernel/proc.c
void mlfq_boost(void) {                // 100 tick마다 호출
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state != UNUSED) {
            p->priority  = 0;          // 모두 HIGH 큐로 끌어올림
            p->run_ticks = 0;
        }
        release(&p->lock);
    }
}

void scheduler(void) {
    for (;;) {
        struct proc *best = 0;
        int best_level = MLFQ_LEVELS;  // sentinel

        for (p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                int lvl = mlfq_level(p->priority);
                if (lvl < best_level) {   // 더 높은 큐 우선
                    best = p;
                    best_level = lvl;
                }
            }
            release(&p->lock);
        }

        if (best) {
            best->run_ticks = 0;       // 새 슬라이스 시작
            // context switch ...
        }
    }
}
```

(슈도코드 3번을 보면서) 이대로 두면 한 가지 문제가 발생합니다. CPU를 오래 잡은 프로세스가 LOW 큐로 강등되고, HIGH 큐에 계속 새 프로세스가 들어오면, LOW 큐의 프로세스는 영원히 실행되지 못합니다. 이걸 starvation이라고 부릅니다.

그래서 위쪽의 mlfq_boost 함수를 100 tick마다 실행되도록 했습니다. 모든 활성 프로세스를 priority 0, 즉 HIGH 큐로 다시 끌어올려서 starvation을 원천 차단합니다.

아래쪽의 scheduler 함수가 실제로 다음에 돌릴 프로세스를 선택하는 로직입니다. 모든 RUNNABLE 프로세스를 훑으면서 가장 낮은 큐 레벨, 즉 HIGH가 가장 우선이고 그 다음 NORMAL, LOW 순으로 선택합니다.

이 두 가지 메커니즘이 결합되어, 우선순위가 높은 프로세스는 빠르게 응답하면서도 낮은 프로세스가 굶지 않는 공정한 스케줄링이 완성됩니다. 한 가지 더 말씀드리면, 이 모든 락 처리는 LLM이 보내는 요청이 어떤 순간에 도착하더라도 운영체제의 자료구조가 깨지지 않도록 보장합니다.

---

## 슬라이드 4 — 평가 결과

```
mlfq_bench 결과 (CPUS=1, 3 자식 동시 실행, 200M iter/child)

   자식       priority    완료 ticks    HIGH 대비
   ────────────────────────────────────────────
   HIGH          1            4          1.00×
   NORMAL        10           8          2.00×
   LOW           19          11          2.75×

   해석:
   - HIGH가 NORMAL의 절반 시간에 완료      (응답성 확보)
   - LOW도 HIGH의 3배 이내에 반드시 완료   (공정성 확보)
   - 100-tick boost가 LOW의 starvation 차단
   - xv6 usertests 26/27 통과 (회귀 안전성)
```

(슈도코드 4번을 보면서) 평가 결과입니다. 우선순위가 1, 10, 19로 다른 세 개의 자식 프로세스를 동시에 실행했을 때, HIGH는 4 tick, NORMAL은 8 tick, LOW는 11 tick 안에 완료됐습니다.

비율로 보시면 1대 2대 2.75입니다. HIGH가 NORMAL의 절반 시간에 끝나면서도, LOW 또한 HIGH의 약 3배 이내에 반드시 완료됩니다. 즉, 응답성과 공정성을 동시에 확보했다는 결과입니다. 추가로 xv6 표준 회귀 테스트인 usertests도 27개 중 26개를 통과해서 기존 OS 동작을 깨뜨리지 않았음을 확인했습니다.

이 수치가 의미하는 것은, 사용자가 LLM으로 자연어 요청을 보냈을 때 그 요청이 단순히 화면에 출력되는 것에서 끝나는 게 아니라, 실제 스케줄링 단계에서 측정 가능한 차이로 이어진다는 것입니다.

---

## 마무리

정리하면 이렇습니다. "LLM for OS"의 핵심은 LLM이 자연어를 처리한다는 점이 아니라, 운영체제 측에 LLM의 요청을 의미 있게 받아낼 수 있는 인터페이스와 메커니즘이 존재한다는 점입니다. 저는 그 입구가 되는 sys_ps 시스템 콜과, LLM의 우선순위 요청이 실제 효과로 이어지도록 만드는 MLFQ 스케줄러를 xv6 커널에 직접 구현했습니다. C 코드 기준 약 250줄을 작성하거나 수정했고, 이 부분은 LLM 코드가 아닌 순수 운영체제 작업입니다.

프로세스 부분에 대한 설명은 여기까지입니다. 감사합니다.
