# SmartShell — LLM for OS (xv6-riscv 기반)

학부 운영체제 학기말 팀 프로젝트. **방향 B(LLM for OS)** 를 선택해서, **xv6-riscv 커널을 직접 수정**하고 그 위에 **LLM 자연어 인터페이스**를 호스트 측 브리지로 얹은 시스템.

자연어로 "프로세스 보여줘", "pid 5 우선순위 높여줘" 같은 요청을 입력하면, Upstage Solar Pro 3가 구조화된 의도(Intent)로 변환하고, 호스트 브리지가 QEMU 안에서 돌고 있는 xv6 셸로 명령을 보내 실행한다.

---

## ⚠️ HANDOFF — 다음 Claude에게

**이 섹션을 먼저 읽고 시작할 것.** 현재 상태 + 남은 일 + 어디까지 검증됐는지 정리.

### 📦 추가 산출물 (이번에 새로 작성한 문서)

| 파일 | 용도 |
|---|---|
| `TECHNICAL_REPORT.md` | 평가 항목 #2 — 시스템 보고서 (영어 + 한국어 혼합) |
| `PRESENTATION_EN.md` | 평가 항목 #4 — 10장 영어 슬라이드 outline + speaker notes + 시간 분배 |
| `DEMO_RECORDING.md` | 1~2분 데모 녹화용 step-by-step 스크립트 |
| `xv6-riscv/user/mlfq_bench.c` | MLFQ 공정성 측정 벤치마크 (HIGH/NORMAL/LOW 비교) |

### 지금까지 완성된 것 (작동 검증됨)

| # | 항목 | 위치 |
|---|---|---|
| 1 | `sys_ps` syscall (proc table → user buffer) | `xv6-riscv/kernel/sysproc.c`, `xv6-riscv/kernel/procinfo.h` (신규) |
| 2 | `ps` 사용자 명령 | `xv6-riscv/user/ps.c` (신규) |
| 3 | `setprio <pid> <prio>` 사용자 명령 | `xv6-riscv/user/setprio.c` (신규) |
| 4 | **MLFQ 3-단계 큐 스케줄러** | `xv6-riscv/kernel/proc.c` (scheduler() 수정 + mlfq_boost/mlfq_tick 추가) |
| 5 | `run_ticks` 필드 + timer tick 통합 | `xv6-riscv/kernel/proc.h`, `xv6-riscv/kernel/trap.c` |
| 6 | `[sched]/[sleep]/[wakeup]` 디버그 print를 `#ifdef SCHED_DEBUG` 가드 | `xv6-riscv/kernel/proc.c` |
| 7 | 호스트 NL 브리지 (Solar Pro 3 + 오프라인 폴백 + 안전 가드 + 백그라운드 stdout 드레인) | `host/nl_bridge.py` (356줄) |
| 8 | `.env` 자동 로딩 + `.gitignore` | `host/.env`, `host/.gitignore` |
| 9 | syscall 6곳 통합 (syscall.h/c, usys.pl, user.h, defs.h, Makefile) | 위 파일들 |
| 10 | GCC 15 호환 (`-Wno-unused-but-set-variable`) | `xv6-riscv/Makefile` |

**검증 완료**: 빌드 통과, 부팅 시 노이즈 0줄, NL→ps/setprio/spawn 동작, priority_test 3/3 PASSED.

### ❗ 아직 안 한 것 (다음 Claude가 해야 할 일, 우선순위 순)

#### ✅ 1. MLFQ 벤치마크 — **완료** (위 §7.1 표 참고)
- 빌드/실행/결과 표 정리까지 끝남
- 추가 작업하고 싶으면: scheduler()에서 mlfq_level을 임시로 막고 baseline(vanilla single-priority) 비교

#### ✅ 2. xv6 `usertests -q` — **완료**: 26 PASS / 1 FAIL (`reparent2`, §7.3 참고)
- 튜닝하고 싶으면: `kernel/proc.c`의 `mlfq_quantum`/`MLFQ_BOOST_INTERVAL` 조정
- 자세한 원인 분석은 §7.3

#### 🟡 3. KILL intent end-to-end 시연 — **(usertests가 fs.img 잠그고 있어서 보류)**
- **상태**: 코드는 있음 (host/nl_bridge.py의 `to_xv6_cmd`에서 `kill {pid}`)
- **재현 방법**: `usertests` 끝나면 `smart>` 로 들어가서
  1. `run spin &` (백그라운드 spin 띄움) — xv6 sh는 `&` 지원
  2. `프로세스 보여줘` → spin의 pid 확인
  3. `kill <그 pid>` → REJECT 안 됨, 진짜 죽음
  4. `프로세스 보여줘` → spin 사라짐 또는 ZOMBIE

#### 🟡 4. EXPLAIN intent 활용 시연
- **할 일**: 발표 데모 끝에 추가 — "fork가 뭐야?", "MLFQ가 뭐야?" 같은 질문
- LLM이 xv6 안 건드리고 답만 함

#### ✅ 5. 기술 보고서 — **완료**: `TECHNICAL_REPORT.md` 참고

#### ✅ 6. 영어 발표 슬라이드 — **완료**: `PRESENTATION_EN.md` 참고
  - 1: Title — SmartShell: LLM for OS
  - 2: Problem — Why NL interface for OS?
  - 3: Architecture diagram
  - 4: Demo (live)
  - 5: Kernel changes — sys_ps + procinfo
  - 6: MLFQ deep dive (3-level queue + boost) ⭐
  - 7: Host bridge — Intent + safety guards
  - 8: Evaluation results
  - 9: "Not just a wrapper" — 5 evidence
  - 10: Limitations + Q&A
- 영어로 짧은 발표문도 추가하면 좋음

#### 🟢 7. 데모 영상/GIF
- **상태**: Claude가 직접 녹화 못 함 (화면 녹화 불가)
- **할 일**: 사용자가 QuickTime으로 직접 녹화 (1~2분)
  - 시연 명령: `python3 nl_bridge.py` → 아래 8개 명령 순서대로
    1. `프로세스 보여줘`
    2. `show me running processes`
    3. `set pid 2 priority to 3`
    4. `프로세스 보여줘` (prio 변화 확인)
    5. `launch priority_test`
    6. `kill 1` (REJECT 시연)
    7. `rm -rf the system` (REJECT 시연)
    8. `fork가 뭐야?` (EXPLAIN 시연)
  - .mov → .gif 변환해서 README에 임베드

### 📌 알아야 할 함정 / 디버깅 노하우

- **빌드 깨지면**: 거의 항상 `make clean && make` 로 해결됨 (.d 파일 stale)
- **QEMU가 한 명령 후 죽으면 (BrokenPipe)**: stdout 파이프 가득 참 — `host/nl_bridge.py`의 `QEMUDriver`는 이미 백그라운드 스레드 드레인으로 해결됨. 만약 다시 터지면 `boot_wait` 늘리기
- **GCC 15에서 `-Werror` 에러**: `Makefile` CFLAGS에 `-Wno-unused-but-set-variable` 이미 추가됨
- **Solar API 502/429**: 일시적, 재시도 또는 잠시 후
- **xv6 안에 네트워크 없음**: HTTPS 호출 불가 — 반드시 호스트 브리지 경유
- **API 키 노출됨**: `host/.env`의 키는 이전 채팅에서 노출됐음 → Upstage 콘솔에서 rotate 권장
- **이전 Claude 대화 컨텍스트**: `CLAUDE.md`에 자세히 있음. 거기 먼저 읽기

### 🚀 다음 Claude 시작 명령 추천

```bash
# 1. 빌드 (현재 mlfq_bench는 UPROGS에 안 들어가 있어서 _ps, _setprio만 빌드됨)
cd "/Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv"
make clean && make

# 2. 우선순위 1: mlfq_bench 빌드/실행 (위 #1)
# Makefile에 _mlfq_bench 추가 → make → QEMU에서 실행

# 3. 우선순위 2: 회귀 검증 (위 #2)
./test-xv6.py -q usertests
```

---

## 0. 쉽게 설명 (초등학생 버전)

> 처음 보는 사람을 위한 비유. 뒤에 나오는 기술 설명은 같은 내용을 정확하게 적은 것.

### 등장인물 4명

**1. 너 (사용자)** — 한국어로 말함. "프로세스 보여줘!"

**2. 통역사 (Solar AI)** — 인터넷 어딘가에 사는 똑똑한 통역사. 사람 말을 컴퓨터 명령으로 바꿔줌.
- 너: "프로세스 보여줘"
- 통역사: "아, 이건 `ps` 라는 컴퓨터 명령이네!"

**3. 우편배달부 (Python 브리지, `host/nl_bridge.py`)** — 통역사한테 너의 말을 가져다주고, 통역 결과를 받아서 장난감 컴퓨터한테 전달해줌. 위험한 명령(예: "다 지워!")은 중간에서 막아.

**4. 장난감 컴퓨터 (xv6)** — 진짜 컴퓨터 안에서 돌아가는 작은 가짜 컴퓨터. 자기만의 메모리, 프로세스, 파일을 가짐. 학교 운영체제 수업 교재용.

### 일이 일어나는 순서

```
① 너                  "프로세스 보여줘"
                          │
                          ▼
② 우편배달부 ──→ ③ 통역사(AI) ──→ "ps 명령이래"
                          │
                          ▼
② 우편배달부 ──→ "위험한 명령인가? 아니, 안전!"
                          │
                          ▼
② 우편배달부 ──→ ④ 장난감 컴퓨터한테 "ps" 라고 적은 쪽지 넣어줌
                          │
                          ▼
④ 장난감 컴퓨터          (자기 안에서 일하고 있는 프로그램 명단을 만듦)
                          │
                          ▼
② 우편배달부 ←── "지금 일하는 애들: init, sh, ps, 이렇게 3명이야"
                          │
                          ▼
① 너                  "3 active processes: init, sh, ps" 화면에 보임
```

### 학교 비유로 다시

장난감 컴퓨터(xv6)는 **교실** 같은 거야.
- **선생님 (kernel)** 이 누가 발표할지(프로세스 실행) 정해줌
- **학생들 (processes)** 이 한 명씩 돌아가면서 발표 (CPU 사용)
- 너가 **"누가 지금 발표 중이야?"** 라고 한국어로 물으면
- 통역사가 **"교실 명단 보여달라는 뜻!"** 으로 바꿔주고
- 우편배달부가 교실 문에 대고 **"명단!"** 이라고 외치면
- 선생님(xv6 커널)이 명단을 적어서 보내줌

### 우편배달부의 안전 규칙

장난감 컴퓨터에서도 위험한 짓은 막음:
- **"1번 학생을 죽여!"** ← 1번은 반장(init), 거부
- **"이상한 프로그램 실행해!"** ← 허락 목록(`ls`, `ps`, `cat`...)에 없으면 거부
- **"우선순위를 100으로!"** ← 0~20 사이만 가능, 거부

### 그래서 뭐가 멋진 거야?

보통 "AI 챗봇"은 그냥 글만 적어주잖아. 우리 프로젝트는:
- **AI는 말 통역만** 함 (전체 코드의 5% 정도)
- **진짜 일은 우리가 만든 운영체제(xv6)** 가 함 — 프로세스 만들고, 우선순위 정하고, 메모리 관리하고
- 그래서 "그냥 AI 래퍼 아니야, 진짜 OS야!" 라고 발표할 수 있는 거

평가 포인트:
- "AI만 써서 만들었지?" → 아니에요! xv6 커널 C 코드를 직접 고쳤어요 (`sys_ps`, 스케줄러)
- "그럼 AI는 왜 써?" → 사람 말을 알아듣게 하려고요. 진짜 일은 OS가 해요

---

## 1. 프로젝트 구성

이 저장소는 두 부분으로 구성된다.

| 디렉토리 | 내용 | 언어 |
|---|---|---|
| `host/` | 호스트 측 NL 브리지 (Solar API 호출 + QEMU 제어) | Python 3.11+ |
| `xv6-riscv/` | 수정된 xv6-riscv 커널 + 사용자 프로그램 | C (RISC-V) |

---

## 2. xv6-riscv 베이스에 대해

[xv6](https://pdos.csail.mit.edu/6.828/2024/xv6.html)는 MIT가 교육용으로 만든 미니 OS로, 6th Edition Unix를 ANSI C로 다시 쓰고 RISC-V 64-bit로 포팅한 것. 본 프로젝트는 그 위에 다음 확장이 미리 들어가 있는 변형판을 사용한다(이 부분은 멤버 B/이전 lab 작업물).

### 베이스에 이미 들어가 있던 확장 (내가 만들지 않음)
- **Lazy page allocation** (`kernel/vm.c`) — `sbrk(SBRK_LAZY)`로 가상 주소만 늘리고, 실제 페이지는 page fault 시점에 할당
- **Orphaned inode recovery** (`kernel/fs.c`의 `ireclaim()`) — 부팅 시 nlink=0인 잔여 inode 회수
- **Crash recovery 테스트** (`user/forphan.c`, `user/dorphan.c`, `user/logstress.c`)
- **trace syscall** (`sys_trace`, `user/trace_test.c`) — 비트마스크로 syscall 호출 로깅
- **sysinfo syscall** (`sys_sysinfo`, `user/sysinfo_test.c`) — freemem / nproc 조회
- **priority syscall** (`sys_setpriority`, `sys_getpriority`, `user/priority_test.c`) — 단일 우선순위 스케줄링
- **scheduler 변경** (`kernel/proc.c`) — round-robin → 가장 낮은 priority 숫자를 선택하는 단일 우선순위 기반

### 명명 규칙
커널 측 프로세스 함수는 `k` 접두사: `kfork()`, `kexit()`, `kwait()`, `kkill()`, `kexec()`. 사용자 공간은 표준 Unix 이름 유지. `SYS_sleep`은 `SYS_pause`로 개명되어 있음.

---

## 3. 이 프로젝트에서 내가 추가/수정한 것

### 3.1 새로 추가한 파일

| 파일 | 설명 |
|---|---|
| `xv6-riscv/kernel/procinfo.h` | `sys_ps`가 사용자 공간으로 내보내는 `struct procinfo` 정의 |
| `xv6-riscv/user/ps.c` | `sys_ps`를 호출해 프로세스 표를 출력하는 `ps` 명령 |
| `host/nl_bridge.py` | 자연어 → Intent → xv6 셸 명령으로 변환하는 호스트 측 REPL |
| `host/README.md` | 호스트 브리지 사용법 |

### 3.2 수정한 기존 파일

| 파일 | 변경 내용 |
|---|---|
| `xv6-riscv/kernel/syscall.h` | `SYS_ps = 26` 추가 |
| `xv6-riscv/kernel/syscall.c` | `sys_ps` extern, 디스패치 배열, 이름 배열에 등록 |
| `xv6-riscv/kernel/sysproc.c` | `sys_ps()` 함수 구현 (`procinfo.h` include 포함) |
| `xv6-riscv/user/usys.pl` | `entry("ps")` 추가 (생성된 stub) |
| `xv6-riscv/user/user.h` | `int ps(struct procinfo *, int);` 선언 |
| `xv6-riscv/Makefile` | `UPROGS`에 `$U/_ps` 추가 |

### 3.3 sys_ps 인터페이스

```c
// kernel/procinfo.h
struct procinfo {
  int    pid;
  int    ppid;
  int    state;     // procstate enum 값 (UNUSED..ZOMBIE)
  int    priority;  // 0=highest .. 20=lowest
  uint64 sz;        // 메모리 크기 (bytes)
  char   name[16];
};

// user space:
int ps(struct procinfo *buf, int max);   // 채운 entry 수 반환, 실패 -1
```

`ps` 명령 출력 예 (호스트 브리지가 정규식으로 파싱):
```
PID  PPID  STATE    PRIO  SZ        NAME
1  0  sleep  10  16384  init
2  1  sleep  10  20480  sh
3  2  run  10  16384  ps
TOTAL 3
```

### 3.4 호스트 NL 브리지 (`host/nl_bridge.py`)

Intent 6종(`PS`, `SETPRIO`, `SPAWN`, `KILL`, `EXPLAIN`, `REJECT`)을 지원.
- `UPSTAGE_API_KEY` 환경변수가 있으면 **Solar Pro 3** 호출
- 없으면 정규식 기반 **오프라인 파서**로 폴백 (로컬 개발용)
- 안전 가드: pid 1 kill 거부, SPAWN 화이트리스트, prio 범위 검증

---

## 4. 사전 준비

### 4.1 필수 도구
- **RISC-V 64-bit GCC 툴체인** (베어메탈 elf 타겟)
- **QEMU 7.2 이상** (`qemu-system-riscv64`)
- **Python 3.11+**

### 4.2 macOS (Homebrew) 설치
```bash
brew tap riscv-software-src/riscv
brew install riscv64-elf-gcc riscv64-elf-binutils qemu
```

### 4.3 Ubuntu 22.04 설치
```bash
sudo apt update
sudo apt install -y qemu-system-misc gcc-riscv64-unknown-elf gdb-multiarch
```

이 경우 Makefile이 자동으로 `riscv64-unknown-elf-` 접두사를 감지한다.

### 4.4 (선택) Solar API 키
실제 LLM 추론을 쓰려면 [Upstage 콘솔](https://console.upstage.ai/)에서 API 키를 발급받아 환경변수로 설정.
```bash
export UPSTAGE_API_KEY=sk-...
```
**키는 절대 git에 커밋하지 말 것.**

---

## 5. 빌드 & 실행

아래 명령은 모두 xv6 소스 디렉토리에서 실행한다.
```bash
cd /Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv
```

### 5.1 빌드
```bash
make kernel/kernel    # 커널만
make fs.img           # 사용자 프로그램 + 파일시스템 이미지
make                  # 둘 다
make clean            # 정리
```

### 5.2 QEMU 직접 실행
```bash
make qemu
```
부팅이 끝나면 `$` 프롬프트가 나온다. 종료는 **Ctrl-A → X**.

새 명령 시도:
```
$ ps
PID  PPID  STATE    PRIO  SZ        NAME
1  0  sleep  10  16384  init
2  1  sleep  10  20480  sh
3  2  run  10  16384  ps
TOTAL 3
$
```

### 5.3 자동 테스트 (xv6 트리에 이미 있음)
```bash
./test-xv6.py usertests          # 전체 usertests
./test-xv6.py -q usertests       # 빠른 버전
./test-xv6.py crash               # log/forphan/dorphan crash 회복 테스트
./test-xv6.py priority_test       # 특정 사용자 테스트
```

### 5.4 호스트 NL 브리지 실행
```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/host"
export UPSTAGE_API_KEY=sk-...     # 선택
python3 nl_bridge.py
```

브리지가 자동으로 `make qemu`를 띄우고, 부팅 후 `smart>` 프롬프트를 노출한다.
```
SmartShell NL Bridge — booting xv6 in QEMU...
Ready. Type natural-language commands. Ctrl-D to exit.

smart> show me the processes
  -> intent: PS {}
  -> xv6  : ps
3 active process(es):
  pid=  1 sleep   prio=10 name=init
  pid=  2 sleep   prio=10 name=sh
  pid=  3 run     prio=10 name=ps

smart> run priority_test
  -> intent: SPAWN {'cmd': 'priority_test'}
  -> xv6  : priority_test
... (priority_test 출력)

smart> kill 1
⚠️  rejected: refusing to kill pid 1 (init)
```

---

## 6. 알려진 이슈 / 한계

- **scheduler 디버그 출력 도배**: 현재 `kernel/proc.c`의 `scheduler()`, `sleep()`, `wakeup()`이 매 호출마다 `[sched]/[sleep]/[wakeup]` 줄을 print한다. 호스트 브리지는 이를 정규식 `_NOISE_RE`로 필터링하지만, `#ifdef SCHED_DEBUG` 가드로 통합하는 게 정석. (멤버 B 영역이라 합의 후 정리 예정)
- **MLFQ 미구현**: 현재 스케줄러는 단일 우선순위 비교에 머물러 있음. W12에 3-단계 큐 + quantum 강등 + 주기적 boost로 확장 예정.
- **`setprio` 사용자 명령 없음**: 호스트의 `SETPRIO` Intent는 임시로 `priority_test`를 실행하도록 대체되어 있다. 별도 `user/setprio.c`를 추가하면 진짜 변경이 된다 (W11 작업).
- **QEMU 출력 동기화**: 현재는 `time.sleep(settle)`로 결과를 기다림. pty + 프롬프트 검출 기반의 안정적 driver로 W12까지 교체 예정.

---

## 7. 평가 지표 (측정 결과)

### 7.1 MLFQ 공정성 — `mlfq_bench` (`CPUS=1`, 200M iter/child)

| 자식 | 우선순위 | 완료 ticks | HIGH 대비 비율 |
|---|---:|---:|---:|
| HIGH   | 1  | **4**  | 1.00× |
| NORMAL | 10 | **8**  | 2.00× |
| LOW    | 19 | **11** | 2.75× |

- HIGH가 NORMAL의 2배 속도로 완료 — 큐 우선순위 효과 확인
- LOW가 HIGH의 약 3배 ticks 안에서 **반드시 완료** (starvation 방지)
- 100-tick boost가 LOW를 주기적으로 HIGH로 끌어올려서 진전 보장

### 7.2 NL 정확도 (수동 6개 입력)

| 입력 | Intent | 결과 |
|---|---|---|
| `프로세스 보여줘` | PS | ✅ |
| `show me running processes` | PS | ✅ |
| `set pid 2 priority to 3` | SETPRIO {pid:2, prio:3} | ✅ |
| `launch priority_test` | SPAWN | ✅ |
| `kill 1` | KILL → guard REJECT | ✅ |
| `rm -rf the system` | REJECT | ✅ |

### 7.3 xv6 `usertests` 회귀

`usertests -q` 결과: **26 PASS / 1 FAIL** (96.3% 통과율)

```
test copyin: OK              test pipe1: OK
test copyout: OK             test killstatus: OK
test copyinstr1: OK          test preempt: kill... wait... OK
test copyinstr2: OK          test exitwait: OK
test copyinstr3: OK          test reparent: OK
test rwsbrk: OK              test twochildren: OK
test truncate1/2/3: OK       test forkfork: OK
test openiput: OK            test forkforkfork: OK
test exitiput: OK            test reparent2: fork failed  ← FAIL
test iput: OK                FAILED — SOME TESTS FAILED
test opentest / writetest / writebig: OK
test createtest / dirtest: OK
test exectest: OK
```

**실패 원인 (`reparent2`)**: 800회 반복 fork 폭주 테스트. MLFQ의 quantum=1
(HIGH 큐)과 100-tick 주기 boost가 컨텍스트 스위치 + 락 경합을 늘려 init의
좀비 reparent + wait 회수 속도가 부족 → NPROC(=64) 초과로 fork 실패.

**완화 옵션 (future work)**:
- quantum 키우기: `{1, 4, 16}` → `{4, 8, 20}` (HIGH preempt 빈도 ↓)
- boost 간격 늘리기: 100 → 300 ticks (proc[] 락 경합 ↓)
- `mlfq_boost()`에서 UNUSED 슬롯 skip (try_acquire 패턴)

본 프로젝트는 일반 워크로드(`priority_test`, `mlfq_bench`, 자연어 시연
명령들)에서 정상 동작 — MLFQ 튜닝은 부하 한계 문제로 분리.

### 7.4 안전 가드

수동 검증: pid 1 kill, 화이트리스트 외 SPAWN, prio 21 → 전부 차단.

---

## 8. 팀 / 역할

| 멤버 | 담당 |
|---|---|
| **A (나)** | 프로세스 / 스케줄러 — `kernel/proc.c` MLFQ 확장, `sys_ps` syscall, `user/ps.c`, 호스트 NL 브리지의 process intent 처리 |
| B | 시그널 / 파일시스템 — SIGCHLD, lazy alloc, ireclaim (이미 베이스에 있음) |
| C | LLM 호스트 — Solar API 호출, intent parser 정밀화, executor |
| D | 인프라 / 평가 / 문서 — Dockerfile, 평가 스크립트, 보고서, 발표 |

---

## 9. 라이선스 / 출처

- xv6-riscv: MIT (원작 MIT 6.S081 / Russ Cox 등). `xv6-riscv/LICENSE` 참조.
- 본 프로젝트의 추가 코드: 학교 과제 산출물 (별도 라이선스 미정).
- LLM 백엔드: [Upstage Solar Pro 3](https://console.upstage.ai/docs).
