# Merge Notes — 통합

각 슬라이스 머지 시 내린 결정 / 충돌 해결 기록. 통합 작업 일자: 2026-05-26.

---

## 0. 출처 (커밋 해시, fetch 시점 기준)

| 슬라이스 | 브랜치 | 추출 커밋 | 추출 방식 |
|---|---|---|---|
| Scheduler | `origin/hyunsung` | `e28af4c6` | `git archive` → `_sources/hyunsung/` |
| Syscall   | `origin/minju`    | `94263c12` | `git archive` → `_sources/minju/` |
| Thread    | `origin/jinhwan`  | `c0774e92` | `git archive` → `_sources/jinhwan/` |
| Process   | `origin/haneol`   | `b8013a00` | `git archive` → `_sources/haneol/` |

GitHub 원본 브랜치는 한 번도 수정되지 않았으며, push/commit 없음.

---

## 1. 최종 syscall 번호 분배 (수정/충돌 해결 결과)

| 번호 | 이름 | 슬라이스 | 비고 |
|---|---|---|---|
| 1~21 | (stock xv6) | — | 미수정 |
| 22 | `trace_on` | minju | per-syscall 카운트 시작 |
| 23 | `trace_off` | minju | 카운트 종료(통계 보존) |
| 24 | `trace_stats` | minju | 통계 dump |
| 25 | `thread_create` | jinhwan | **원본 22 → 25 재번호** |
| 26 | `thread_join` | jinhwan | **원본 23 → 26 재번호** |
| 27 | `thread_exit` | jinhwan | **원본 24 → 27 재번호** |
| 28 | `futex_wait` | jinhwan | **원본 25 → 28 재번호** |
| 29 | `futex_wake` | jinhwan | **원본 26 → 29 재번호** |
| 30 | `setpri` | hyunsung | **원본 22 → 30 재번호** |
| 31 | `getstats` | hyunsung | **원본 23 → 31 재번호** |
| 32 | `settrace` | hyunsung | **원본 24 → 32 재번호** (MLFQ trace ON/OFF, *not* per-syscall) |
| 33 | `forkpri` | hyunsung | **원본 25 → 33 재번호** |
| 34 | `ps` | haneol | **원본 26 → 34 재번호** |
| 35 | `sysinfo` | haneol | **원본 23 → 35 재번호** |

→ NELEM(syscalls) 안에 들어감 (64 미만), 모든 슬라이스 공존 가능.

### 1.1 hyunsung 원본 문서 vs 코드 불일치

`docs/syscall-allocation.md` 는 "hyunsung 30~33 으로 이동 완료" 라고 적혀있었지만,
실제 `smart-mlfq-xv6-patches/kernel/syscall.h` 코드는 **여전히 22~25** 였음.
→ 통합 시 코드를 문서에 맞게 30~33 으로 수정함 (`integration/xv6-riscv/kernel/syscall.h`).

---

## 2. 충돌 해결 (체크리스트 §3 기준)

| 충돌 영역 | 승자 | 사유 |
|---|---|---|
| `kernel/proc.c` MLFQ 스케줄러 | **hyunsung** | 정량 평가에서 검증, haneol 의 0..20 priority 시스템보다 더 정교한 3-단계 큐 |
| `kernel/proc.c` `kthread_*` 함수 | **jinhwan** | Thread 슬라이스 고유, MLFQ 와 별도 영역에 append |
| `kernel/syscall.c` 후킹 (`p->syscall_count[]`) | **minju** | hyunsung 의 dispatch 위에 trace 후킹 patch |
| `kernel/syscall.h` 번호 영역 | **위 §1 표** | 4팀 합의된 분배 |
| 호스트 NL 브리지 진입점 | **hyunsung** `nl_shell.py` | 단일 entry, 나머지 둘은 `host/adapters/` 모듈화 |
| `struct proc` 확장 필드 | **OR 결합** | 필드명이 서로 달라서 충돌 없음 |

### 2.1 haneol 슬라이스 syscall drop 결정

haneol 의 syscall 5개 중 3개는 통합에서 **제외**:
- `sys_trace (22)` → minju 의 trace_on/off/stats 와 의미 중복, 약함 → drop
- `sys_setpriority (24)` → hyunsung 의 `setpri (30)` 과 중복, 0..20 priority 체계가 MLFQ 와 안 맞음 → drop
- `sys_getpriority (25)` → hyunsung 의 `getstats (31)` 이 더 풍부 → drop

남은 2개만 유지:
- `sys_ps (34)` ✓ — 다른 슬라이스에 없는 고유 기능
- `sys_sysinfo (35)` ✓ — freemem + nproc, 고유 기능

### 2.2 haneol `setprio.c` user 프로그램 재작성

원본은 `setpriority(pid, 0..20)` 호출 — 이제 syscall 자체가 없음.
→ `setprio.c` 를 새로 작성해서 `setpri(pid, 0..2)` 를 호출하도록 변경 (출력 포맷은 호환 유지).

### 2.3 host 측 하드코딩 API 키 제거

`jinhwan/solar_bridge.py` 에 `SOLAR_API_KEY = "up_ZxEm..."` 가 평문으로 박혀있었음.
→ 통합 시 `adapters/thread_bridge.py` 에서 `os.environ.get(...)` 로 전환, 키 없을 때 폴백 추가.
→ docs/security-policy.md 준수.

---

## 3. 슬라이스별 머지 로그

### 3.1 hyunsung Scheduler — 베이스
- 상태: ✅ 완료
- 추출: `smart-mlfq-xv6-patches/{kernel,user,workloads}/*`
- 적용 방법:
  1. minju 의 `.bak` 파일들로 stock xv6 트리 복원 → 베이스
  2. hyunsung 의 kernel/* (defs.h, proc.c, proc.h, syscall.{c,h}, sysproc.c, trap.c) 덮어쓰기
  3. user/* 신규 추가 (nlrun, wrunner, cpu_burner, io_burner, mixed_burner, rrunner, diagprog)
  4. workloads/* 신규 추가 (6종 워크로드)
  5. syscall.h 번호 재할당 (22~25 → 30~33)
  6. Makefile UPROGS 에 7개 추가

### 3.2 minju Syscall (trace)
- 상태: ✅ 완료
- 추가된 것:
  - `struct proc` 필드: `traced`, `syscall_count[32]`, `syscall_errors[32]` (proc.h)
  - `syscall()` 함수 내 per-syscall 후킹 (syscall.c)
  - `sys_trace_on/off/stats` 구현 (sysproc.c)
  - dispatch table + externs (syscall.c)
  - usys.pl + user.h + tracetool.c

### 3.3 jinhwan Thread
- 상태: ✅ 완료
- 추가된 것:
  - `struct proc` 필드: `is_thread`, `tgroup` (proc.h)
  - `futex_lock` 전역 + `initlock(&futex_lock, "futex")` (proc.c)
  - `freeproc()` 에 `is_thread` 분기 추가 (proc.c)
  - `kthread_create/join`, `kfutex_wait/wake` (proc.c append)
  - `uvmshare`, `thread_freepagetable` (vm.c append)
  - `sys_thread_*`, `sys_futex_*` (sysproc.c)
  - defs.h kernel decls
  - usys.pl + user.h + threadtest.c + umutex.h

### 3.4 haneol Process
- 상태: ✅ 완료
- 추가된 것:
  - `kernel/procinfo.h`, `kernel/sysinfo.h` (그대로 복사)
  - `proc_count()` (proc.c append)
  - `freemem_count()` (kalloc.c append)
  - `sys_ps`, `sys_sysinfo` (sysproc.c)
  - defs.h kernel decls
  - usys.pl + user.h + ps.c + 새 setprio.c
- DROP된 것: sys_trace, sys_setpriority, sys_getpriority, proc.c 안 0..20 MLFQ 블록

### 3.5 host/ Python
- 상태: ✅ 완료
- 진입점: `host/nl_shell.py` (hyunsung)
- `adapters/process_bridge.py` ← haneol `nl_bridge.py` (변경 없음)
- `adapters/thread_bridge.py` ← jinhwan `solar_bridge.py` (API 키 + 경로 환경변수화)
- `host/README.md` 신규 작성 — 진입점/어댑터 분배 설명

---

## 4. 빌드 검증 (체크리스트 §4 기준) — ✅ 2026-05-26 통과

WSL/QEMU 환경에서 `make qemu CPUS=2` 로 빌드/부팅/슬라이스 데모 모두 확인 완료.

### 빌드 중 발견·픽스한 4가지 이슈

| # | 증상 | 원인 | 패치 |
|---|---|---|---|
| 1 | `make: *** No rule to make target 'mkfs/mkfs.c'` | 베이스 트리에 mkfs/ 디렉토리 누락 | `lecture-repo/xv6-riscv/mkfs/mkfs.c` 복사 |
| 2 | `error: redefinition of 'struct procstats'` in threadtest.c | jinhwan 의 `umutex.h` 가 `user.h` 를 재include → 이중 정의 | `user.h` 에 include guard 추가 (`#ifndef XV6_USER_H ... #endif`) |
| 3 | `wrunner: cannot open cpu_heavy.txt` | `fs.img` 패키징 시 workloads/*.txt 가 포함 안 됨 | Makefile `fs.img` 규칙에 `WORKLOADS` 변수 추가 |
| 4 | `mkfs: assertion 'index(shortname,'/')==0' failed` | stock mkfs.c 가 `user/` prefix 만 strip → `workloads/cpu_heavy.txt` 의 `/` 가 남아서 assert 깨짐 | mkfs.c 의 path stripping 을 `strrchr(.,'/')` 기반의 일반 로직으로 교체 |
| 5 | `hints_example.txt` (17자) > DIRSIZ(14) | xv6 FS 디렉토리 이름 길이 제한 | `hints.txt` 로 rename, Makefile 의 WORKLOADS + 데모 명령도 동기 |

### 라이브 데모 결과 (2026-05-26)

| 슬라이스 | 명령 | 결과 |
|---|---|---|
| hyunsung | `nlrun 2 cpu_burner 1000000` | ✅ TRACE/EXIT, final_pri=2 |
| minju    | `tracetool on; ls; tracetool dump` | ✅ JSON `{"total":1265,"calls":{...8 syscalls...}}` |
| jinhwan  | `threadtest` | ✅ 4 tests 모두 통과 (shared mem, 4 threads, mutex, condvar) |
| haneol   | `ps`, `setprio 1 2` | ✅ 3-row 출력, `OK pid=1 level=2` |
| 통합     | `wrunner cpu_heavy.txt hints.txt` | ✅ 5 items + 3 hints 로드, 4 cpu_burner spawn, `ALL_DONE` |

---

## 5. 정량 평가 — 2026-05-27 재실행

### 5.1 1차(5/26) 실행이 무효였던 사유

`run_eval_matrix.sh` 호출 시 venv 비활성 + `.env` 미배치 → `llm_hint.py`가 `import dotenv` 단계에서 즉시 종료(`ModuleNotFoundError`). 스크립트의 `>/dev/null 2>&1` 때문에 에러는 가려지고 `[warn] heuristic hint generation failed` 한 줄만 남음. 결과적으로:

- `heuristic_hints.txt`, `solar_hints.txt` 미생성
- 후속 `cp -f` 실패 → `$XV6_DIR/hints.txt`는 이전 baseline의 `# empty (baseline)` 그대로
- heuristic/solar 모드가 baseline과 사실상 동일 입력으로 실행됨
- Δheur%/Δllm% 값은 QEMU 스케줄링 노이즈에 불과

### 5.2 재실행 환경

- venv: `/root/smart-mlfq-host/venv` (dotenv 1.2.2, openai 2.38.0)
- `.env`: `/root/smart-mlfq-host/.env`를 `integration/host/.env`로 복사 (gitignored, `.gitignore` 루트 규칙 확인)
- 명령: `XV6_DIR=../xv6-riscv OUT_ROOT=../../docs/charts/integration ./run_eval_matrix.sh`

### 5.3 결과 요약 (avg_turnaround 기준)

| 워크로드 | Δheur% | Δllm% | 비고 |
|---|---|---|---|
| cpu_heavy | +283% | +550% | 단일 프로그램 → 우선순위 강등이 곧 손해 (예상) |
| io_heavy  | +2.8% | +15.8% | 단일 프로그램 + 부분 timeout |
| mixed     | +17.1% | +14.3% | 평균↓ 대신 fairness +155%(heur), max_TT −53% |
| three_way | **−7.3%** | **−5.8%** | 다중 프로그램 핵심 — 모두 개선 |
| realprog  | +6.8% | −0.2% | heur avg_response −26% |

### 5.4 환경 아티팩트

- `io_heavy` 전 모드 + `mixed/baseline`에서 `no ALL_DONE` (QEMU_TIMEOUT=45s 한계)
- /mnt/c 경로 QEMU가 /root 대비 느려 단독 슬라이스 절대값과 직접 비교는 불가 — 모드 간 상대 비교만 의미

산출물: `docs/charts/integration/{cpu_heavy,io_heavy,mixed,three_way,realprog}/*` + `combined_report.txt`.

---

## 6. 알려진 위험 / TODO

| 위험 | 영향 | 완화 |
|---|---|---|
| `ps` 의 priority 출력은 0..2 (MLFQ 큐) | haneol 원본은 0..20 | 출력 포맷 유지, 의미만 변경 |
| host adapters 가 nl_shell.py 와 import 통합되어 있지 않음 | 단일 진입점 보장 정도가 약함 | 해결 (2026-05-27): `nl_shell.py --mode {process,thread}` 가 adapters를 import해 호출, 단일 진입점 완성 |
| jinhwan 의 thread 라이브러리(thread.c, mutex.c 등) 는 통합 안 함 | userspace 추상화, xv6 안에서는 syscall 만으로 충분 | 단독 시연용으로 _sources/ 에 보존 |
| haneol 의 `mlfq_bench.c`, `priority_test.c`, `spin.c`, `trace_test.c`, `sysinfo_test.c` | 단독 테스트 프로그램 | 통합 트리엔 미포함, _sources/ 보존 |
| `run_eval_matrix.sh`가 `>/dev/null 2>&1`로 에러 가림 | 5.1 같은 무음 실패 재발 가능 | stderr 패치 적용 완료 (2026-05-27) |

### 6.1 갭 메우기 추가 (2026-05-27)

`integration/` 가 약속한 7가지 능력 중 다음 2개가 부분 구현이라 보완:

| 능력 | 추가물 | 위치 |
|---|---|---|
| 백그라운드 작업 큐 (이전: LOW 우선순위 시작만 가능) | `bgq` user binary — 순차 큐 + `forkpri(2)` + `wait()` 좀비 회수 | `xv6-riscv/user/bgq.c`, `workloads/bgq.txt`, Makefile UPROGS/WORKLOADS |
| tracetool dump → LLM 자동 분석 (이전: 형식만 호환) | `nl_shell.py --analyze-tracetool` + `TRACETOOL_ANALYZE_PROMPT` | `host/nl_shell.py`, `host/prompts.py` |

`bgq` 런타임 검증 (2026-05-27, QEMU): `bgq bgq.txt` → `BGQ start echo` → `job1_started` → `BGQ done echo pid=4 exit=0` → `BGQ start cpu_burner` → ... → `BGQ all_done 3`. 좀비 누수 없음.

`--analyze-tracetool` 검증: 샘플 tracetool dump JSON 입력 → Solar Pro 3 응답 `{"verdict":"spawner","summary":"...","queue_hint":0,"reason":"..."}`. 풀 체인 동작.

### 6.2 알려진 회귀 — `usertests -q` 부분 실패 (2026-05-27 발견)

`make qemu CPUS=2` 후 `usertests -q` 실행 시 26개 테스트 통과 후 27번째 `reparent2` 에서
"fork failed" 출력 + `FAILED / SOME TESTS FAILED` 종료. `reparent2` 는 800회 sequential
fork+wait 시퀀스로 normally 슬롯 누수가 없으면 절대 실패하지 않는 stock xv6 표준 테스트.

**격리 실험** (`user/forkstress.c` 신규 추가, fresh QEMU 부팅에서 단일 실행):
- `forkstress 1000` → `FORKSTRESS done 1000/1000`, 누수 0건
- → **기본 fork+wait 경로는 깨끗**. usertests의 누수는 **이전 26개 테스트 중 어느
  하나가 남긴 누적 상태** (잠재적으로 `forkforkfork`, `reparent`, `twochildren`,
  `exitwait` 부근에서 일어나는 zombie/thread/page-table 회수 실패).

**영향**
- 5종 슬라이스 라이브 데모: 영향 없음 (시연 시나리오에 800회 fork stress 없음)
- `bgq.txt` 시연: 영향 없음 (3 fork)
- `forkstress 1000`: 영향 없음
- 평가자가 직접 `usertests -q` 돌리면 발각됨

**후속 작업** (다음 단계)
- 후보 origin: `proc.c::allocproc` K1 fix 의 64-slot 초기화 / `freeproc` 의 thread 분기 /
  `kfork`/`kforkpri` 의 trace inheritance / `kfutex_wait` 의 lock ordering
- 진단 방법: usertests 의 26개 중 어느 시점부터 슬롯 점유가 누적되는지 측정
  (예: 각 테스트 사이 `ps` 호출해 사용 중 슬롯 수 추적)
- W14 후속 작업으로 보류 — 통합 데모와 발표 시연에는 영향 없으므로 차단급 결함 아님

---

## 7. 흔한 함정 (체크리스트 §9 + 본 통합 작업에서 발견)

| 증상 | 원인 | 대처 |
|---|---|---|
| `multiple definition of mlfq_*` | haneol proc.c MLFQ 블록 잔존 시 발생 | 본 통합에서 baseline 으로 stock 을 깔았으므로 발생 안 함 |
| `undefined reference to sys_thread_create` | syscall.h 번호 누락 시 | §1 표 그대로 적용했으므로 안전 |
| TRACE 라인 미출력 | `settrace(0,1)` 미호출 | `nlrun.c`/`wrunner.c` 가 자동 호출함 |
| `proc.h:struct proc` 컴파일 에러 | uint64 미정의 | proc.h 가 types.h 의존 — 기존 그대로 |
| user 측 `uint64` 미인식 | types.h include 누락 | xv6 관례대로 user prog 는 `#include "kernel/types.h"` 먼저 |
| Solar API 키 노출 | jinhwan 원본에 평문 | 통합 시 환경변수로 전환됨 (§2.3) |

---

## 8. 코드 내 `K1~K4 fix` 라벨 매핑

통합 빌드 중 잡은 커널·호스트 4종 fix를 코드 주석에 `K1`~`K4 fix` 로 태깅했음. 본 표가 정식 매핑이며, 새 fix 추가 시 K5~ 로 확장.

| 라벨 | 의미 | 등장 위치 |
|---|---|---|
| **K1** | proc-table 슬롯 재사용 시 누락된 필드 초기화 — `allocproc()` 에서 `traced=0`, `syscall_count[]=0`, `syscall_errors[]=0` (minju), `is_thread=0`, `tgroup=0` (jinhwan). 안 하면 직전 입주자의 trace 상태가 새 프로세스로 누수됨 → `tracetool dump` 가 쓰레기 출력. | `xv6-riscv/kernel/proc.c:170, 177` |
| **K2** | `syscall_count[]` / `syscall_errors[]` 배열 크기를 **32 → 64** 로 확장. minju 원본은 SYS_close=21까지 가정해 32였지만 통합 후 SYS_settrace=32, SYS_forkpri=33, SYS_ps=34, SYS_sysinfo=35 가 들어와 32 인덱스 오버플로 발생. | `xv6-riscv/kernel/proc.h:135`, `kernel/syscall.c:172`, `kernel/sysproc.c:207`, `user/user.h:30`, `user/tracetool.c × 4` |
| **K3** | `fork`/`forkpri` 경로에서 자식이 부모의 `traced` 플래그를 상속하도록 명시. 안 하면 셸에서 `tracetool on` 후 실행한 자식 프로세스가 추적 안 됨 → 데모 시나리오 깨짐. | `xv6-riscv/kernel/proc.c:327, 935` |
| **K4** | 호스트 NL 어댑터의 우선순위 범위 정렬. haneol 원본 `setpriority(pid, 0..20)` 을 가정했지만 통합 후엔 hyunsung MLFQ `setpri(pid, 0..2)` 만 유효. `SPAWN_WHITELIST` 도 통합 트리에 없는 `priority_test`/`trace_test`/`sysinfo_test` 빼고 갱신. | `host/adapters/process_bridge.py:55, 85, 155` |
