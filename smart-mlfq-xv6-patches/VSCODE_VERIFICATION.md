# VS Code + xv6-riscv 검증 체크리스트

> 이 샌드박스 환경에서는 RISC-V 크로스컴파일러와 QEMU를 설치할 수 없어
> 실제 빌드/실행은 본인의 VS Code 환경에서 진행해야 합니다. 아래는
> 단계별 체크리스트입니다.

## 0. 사전 준비

VS Code에서:
- **확장**: C/C++ (ms-vscode.cpptools), Remote - SSH (만약 리눅스 서버 빌드라면)
- **터미널 접근 가능한 환경**: 우분투/맥/WSL

호스트(또는 VM)에:
```bash
# RISC-V 크로스컴파일러 + QEMU
sudo apt-get install -y \
    gcc-riscv64-unknown-elf \
    qemu-system-misc \
    gdb-multiarch \
    make python3 python3-pip
```
> 우분투 24.04 기준. 다른 배포판은 패키지명이 다를 수 있어요
> (예: Arch는 `riscv64-elf-gcc`).

## 1. 패치 적용

`smart-mlfq-xv6-patches/` 폴더의 파일들을 강의용 xv6 트리에 덮어쓰기:

```bash
cd <your-project-root>

# 강의용 xv6를 원본 위치로 가져오기 (서브모듈이거나 이미 있다면 스킵)
git clone <lecture-repo>/xv6-riscv  # 또는 기존 위치 사용

# 패치 적용
cp smart-mlfq-xv6-patches/kernel/*  lecture-repo/xv6-riscv/kernel/
cp smart-mlfq-xv6-patches/user/*    lecture-repo/xv6-riscv/user/
```

## 2. 빌드 검증

```bash
cd lecture-repo/xv6-riscv
make clean
make
```

**기대 결과**: 경고 없이 또는 원본과 동일한 경고만 떠서 `kernel/kernel`
바이너리가 생성. 다음과 같은 메시지가 보이면 OK:
```
riscv64-unknown-elf-ld -z max-page-size=4096 -T kernel/kernel.ld -o kernel/kernel \
    kernel/entry.o kernel/start.o kernel/console.o ...
```

**만약 `undefined reference to 'usertrapret'` 가 나온다면**:
패치 적용이 잘못된 것. `kernel/proc.c::forkret`이 `prepare_return()`
+ trampoline 점프 시퀀스로 끝나는지 확인.

## 3. 부팅 테스트 (워크로드 없이)

```bash
make qemu
```

**기대 결과**:
- `xv6 kernel is booting` 메시지
- `init: starting sh` 메시지
- `$` 프롬프트
- TRACE 라인이 **콘솔에 안 보여야 함** (I2 패치로 default off 했으니까)

**문제 신호**:
- `kernel panic: <something>` → 락 관련이면 C1 또는 C3 패치 점검
- 부팅 멈춤 (hang) → 스케줄러 무한루프 의심, `Ctrl-A x`로 빠져나와서
  `make qemu-gdb` 로 디버깅
- TRACE 라인이 막 보임 → `allocproc()`의 `trace_enabled = 0` 적용 확인

## 4. Syscall 동작 확인 (간단 테스트)

`$` 프롬프트에서:

```sh
$ uptime          # 기존 syscall — 정상 동작 확인
1234
$ ls              # 일상 명령 — sleep/wakeup 정상 확인
```

추가로 user 측 테스트 프로그램을 만들면 좋습니다 (workload_runner.c
업데이트 시 이미 들어가 있을 수도):

```c
// user/test_mlfq.c
#include "kernel/types.h"
#include "user/user.h"

int main(void) {
    int pid = getpid();

    // 1) trace 켜기
    if (settrace(pid, 1) < 0) {
        printf("settrace failed\n");
        exit(1);
    }
    printf("trace enabled for pid %d\n", pid);

    // 2) priority 변경
    if (setpri(pid, 2) < 0) {
        printf("setpri failed\n");
        exit(1);
    }
    printf("set priority to LOW\n");

    // 3) stats 조회
    struct procstats st;
    if (getstats(pid, &st) < 0) {
        printf("getstats failed\n");
        exit(1);
    }
    printf("pid=%d pri=%d run=%d io=%d\n",
           st.pid, st.priority, st.total_run_ticks, st.total_io_blocks);

    // 4) forkpri로 자식 만들기
    int cpid = forkpri(0);  // HIGH
    if (cpid == 0) {
        // 자식: 짧게 일하고 종료
        for (volatile int i = 0; i < 100000; i++);
        exit(0);
    }
    wait(0);

    // 5) 자식의 final stats 출력
    getstats(cpid, &st);
    printf("child final: pri=%d run=%d\n", st.priority, st.total_run_ticks);

    exit(0);
}
```

**Makefile**에 추가:
```makefile
UPROGS=\
    $U/_cat\
    $U/_echo\
    ...
    $U/_test_mlfq\      <-- 추가
```

QEMU에서 실행:
```sh
$ test_mlfq
```

**기대 결과**:
- `trace enabled for pid N` 출력
- TRACE 라인이 콘솔에 흐름 시작
- `set priority to LOW` 출력
- stats가 합리적 값
- 자식 종료 후 EXIT trace 1줄 + final stats

## 5. 멀티 CPU race 검증 (C1)

`Makefile`을 보면 기본 `CPUS = 3`. 그대로 두고 빌드:

```bash
make qemu
```

부팅 후 같은 명령 여러 개 백그라운드로:
```sh
$ test_mlfq &
$ test_mlfq &
$ test_mlfq &
```

**기대**: 세 개 다 정상 종료. boost가 100 tick마다 일어나는 동안
panic 없음.

**boost_lock이 없던 원본 코드**에서는 *이론적으로* race가 가능했지만
실제로 깨지진 않을 가능성도 큼 (idempotent). 따라서 panic이 안 떴다고
패치가 의미 없는 건 아니고, contention만 줄어든 거예요.

## 6. 호스트 Python 파이프라인 통합 테스트

`smart-mlfq-xv6/host/run_experiment.py` 가 이미 있으니:

```bash
cd smart-mlfq-xv6
cp -r ../lecture-repo/xv6-riscv .   # 패치된 트리 복사
pip install -r requirements.txt
echo "UPSTAGE_API_KEY=your_key" > .env

python3 host/run_experiment.py --workload workloads/mixed.txt --baseline
# → results/baseline_trace.json 생성

python3 host/run_experiment.py --workload workloads/mixed.txt --hints results/hints.json
# → results/llm_trace.json 생성
```

**중요 검증 포인트**:
- `baseline_trace.json` 의 `workload_runner` 엔트리에서 `total_io_blocks`
  가 **0 또는 매우 작은 수**여야 함 (C2 패치 효과). 만약 이게 자식
  개수만큼 카운트되어 있다면 C2 패치가 안 먹은 것.
- TRACE 라인의 `tick` 분포가 시간순으로 합리적이어야 함 (C3가 잘
  작동하면 timing이 거의 균일).

## 7. VS Code-specific 디버깅 팁

`.vscode/launch.json` 설정 예시 (xv6 GDB):

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "xv6 kernel debug",
            "type": "cppdbg",
            "request": "launch",
            "miDebuggerPath": "/usr/bin/gdb-multiarch",
            "miDebuggerServerAddress": "localhost:26000",
            "program": "${workspaceFolder}/lecture-repo/xv6-riscv/kernel/kernel",
            "cwd": "${workspaceFolder}/lecture-repo/xv6-riscv",
            "MIMode": "gdb",
            "setupCommands": [
                { "text": "set architecture riscv:rv64" },
                { "text": "symbol-file kernel/kernel" }
            ]
        }
    ]
}
```

별도 터미널에서:
```bash
make qemu-gdb   # GDB 서버를 26000 포트에 열고 대기
```

VS Code에서 F5 → 브레이크포인트는 `proc.c` 의 `scheduler()`, `sleep()`,
`proc_on_timer_tick()` 같은 곳에 걸어보세요.

## 8. 트러블슈팅 빠른 참조

| 증상 | 의심 위치 | 점검 |
|---|---|---|
| `undefined reference to usertrapret` | `forkret` | `prepare_return()` + trampoline 점프로 끝나는지 |
| 부팅 중 panic on `sched p->lock` | `scheduler` | swtch 전후 락 해제 순서 |
| 부팅 중 panic on `sched locks` | trap.c 또는 scheduler | 다른 락 들고 swtch한 경우 |
| 자식 fork 직후 멈춤 | `kforkpri` | wait_lock 해제 빠뜨림 |
| getstats 항상 -1 | proc_getstats | `state != UNUSED` 체크 |
| TRACE 한 줄도 안 나옴 | `proc_settrace` | `settrace(0,1)` 호출했는지 |
| 너무 많은 TRACE | trace 상속 | 부모가 trace=1인 채 fork |
| Solar API 호출 실패 | host/llm_hint.py | `.env`의 `UPSTAGE_API_KEY` |

## 9. 최소 동작 확인 결과 보고 양식

다음 항목을 본인 환경에서 확인 후 결과를 알려주세요:

- [ ] `make` 성공 (경고는 원본과 동일한 것만)
- [ ] `make qemu` 부팅 성공, `$` 프롬프트 진입
- [ ] `ls`, `uptime` 등 기본 명령 동작
- [ ] `test_mlfq` 프로그램 정상 종료
- [ ] `settrace`, `setpri`, `getstats`, `forkpri` 4개 모두 0 또는 양수 반환
- [ ] 멀티 인스턴스(3개 동시) panic 없음
- [ ] baseline trace.json의 workload_runner total_io_blocks가 0 또는 1

여기서 막히는 게 있으면 출력 로그 첨부해주세요. 추가 디버깅 도와드립니다.
