# SmartShell — Claude Code 컨텍스트 (xv6-riscv 기반)

이 파일은 Claude Code가 프로젝트를 시작할 때 자동으로 읽는 컨텍스트야.
**질문하기 전에 항상 이 파일을 먼저 읽고 시작해.**

---

## 📌 프로젝트 한 줄 요약

**SmartShell** — xv6-riscv(RISC-V 64-bit) 위에 LLM 자연어 인터페이스를 얹어, 자연어로 OS 프로세스를 다루는 시스템.
- **방향 B**: LLM for OS (LLM을 OS 안으로 통합)
- 진짜 **OS 컴포넌트**(xv6 커널의 스케줄러, syscall, proc table)를 직접 수정/확장
- LLM(Upstage Solar Pro 3)은 **호스트 측 브리지**로 분리 — xv6는 네트워크/HTTPS 불가
- 학부 운영체제 학기말 팀 프로젝트, 4인팀, 14주차, 성적 30%

---

## 📂 디렉토리 구조 (중요 — 두 곳에 분산되어 있음)

```
/Users/jeonhaneol/Downloads/LLM for OS/   ← 프로젝트 루트 (단일 트리)
├── CLAUDE.md                         (이 파일)
├── README.md                         (사용자/평가자용 문서)
├── host/                             ← 호스트 NL 브리지 (Python)
│   ├── nl_bridge.py
│   ├── README.md
│   ├── .env                          (gitignored — UPSTAGE_API_KEY)
│   └── .gitignore
└── xv6-riscv/                        ← xv6 커널 소스 (멤버 B 등과 공유)
    ├── kernel/                       ← C 커널 소스
    │   ├── proc.c, proc.h            ← 스케줄러, proc table (내가 수정 중)
    │   ├── sysproc.c                 ← 프로세스 syscall (내가 sys_ps 추가)
    │   ├── procinfo.h                ← 내가 신규로 만듦
    │   ├── syscall.c, syscall.h      ← syscall 디스패치 (내가 SYS_ps 등록)
    │   └── ...
    ├── user/                         ← 사용자 공간 프로그램
    │   ├── ps.c                      ← 내가 신규로 만듦
    │   ├── priority_test.c           ← 기존
    │   ├── usys.pl, user.h           ← syscall stub (내가 ps 추가)
    │   └── ...
    ├── Makefile                      ← UPROGS에 _ps 추가
    └── test-xv6.py                   ← QEMU 자동화 테스트 러너
```

`host/nl_bridge.py`는 `XV6_DIR` 기본값을 `os.path.dirname(__file__)/../xv6-riscv`로 잡아서, 어디로 옮겨도 자동 탐색됨.

---

## ⚠️ 가장 중요한 원칙

> **"LLM API 단순 래퍼"는 인정 안 됨.**
> **OS 개념(스케줄러, syscall, proc table)을 xv6 커널에서 직접 구현해야 함.**

- LLM의 역할: 자연어 → 구조화된 의도(Intent), 결과 요약 (호스트 측)
- 우리가 구현: **xv6 커널의 MLFQ 스케줄러, sys_ps/sys_setpriority 등 syscall, 호스트↔xv6 콘솔 브리지**
- 코드 비중: xv6 C 코드 > Python 호스트 브리지 (LLM 호출 코드는 전체의 5% 이하)

---

## 🏗️ 시스템 아키텍처

```
[사용자 자연어 입력]
        │
        ▼
┌───────────────────────────────────┐
│  Python NL Bridge (호스트)         │  ← 멤버 A 일부 + C 담당
│  host/nl_bridge.py                │
│  - Solar API: NL → Intent         │
│  - 화이트리스트 가드               │
│  - QEMU stdio 입출력               │
└───────────┬───────────────────────┘
            │ stdin/stdout (subprocess)
            ▼
┌───────────────────────────────────┐
│  xv6-riscv (QEMU)                 │
│  ┌──────────────────────────┐     │
│  │  user/sh, ps, ...        │     │  ← 멤버 A: ps.c, priority_test
│  ├──────────────────────────┤     │
│  │  syscalls                │     │  ← 멤버 A: sys_ps, sys_setpriority
│  ├──────────────────────────┤     │
│  │  kernel                  │     │
│  │   - proc.c (priority→MLFQ│     │  ← 멤버 A ⭐ (W12 확장 예정)
│  │   - trap.c (quantum)     │     │  ← 멤버 A ⭐ (W12)
│  │   - signal/syscall       │     │  ← 멤버 B
│  │   - fs/log (ireclaim)    │     │  ← 멤버 B (이미 있음)
│  │   - vm.c (lazy alloc)    │     │  ← 멤버 B (이미 있음)
│  └──────────────────────────┘     │
└───────────────────────────────────┘
```

---

## 👤 내 역할: 멤버 A — 프로세스/스케줄러

**xv6 커널의 프로세스 관리 + 호스트 NL 브리지의 프로세스 관련 부분을 담당.**

### 진행 상황 (W10 시점)

| 항목 | 상태 | 파일 |
|---|---|---|
| `sys_ps` syscall | ✅ 구현 완료 | `kernel/sysproc.c`, `kernel/procinfo.h` (신규) |
| `ps` 사용자 프로그램 | ✅ 구현 완료 | `user/ps.c` (신규) |
| 빌드 통합 (Makefile, syscall.h/c, usys.pl, user.h) | ✅ 완료 | 6곳 수정 |
| 호스트 NL 브리지 스켈레톤 | ✅ 구현 완료 | `host/nl_bridge.py` (신규) |
| `[sched]/[sleep]/[wakeup]` 디버그 print 정리 | ⏸️ 보류 (멤버 B 영역, 합의 필요) | `kernel/proc.c` |
| MLFQ 확장 | 🚧 W12 목표 | `kernel/proc.c`, `kernel/trap.c` |

### 기존에 들어가 있던 것 (내가 만들지 않음)

- `proc.h`에 `int priority` 필드 (0=highest, 20=lowest, default 10)
- `proc.h`에 `int trace_mask` 필드
- `kernel/sysproc.c`: `sys_setpriority`, `sys_getpriority`, `sys_trace`, `sys_sysinfo`
- `kernel/proc.c`: scheduler()가 단일 우선순위 기반 (가장 낮은 priority 숫자 선택) — MLFQ 아님
- `user/priority_test.c`, `trace_test.c`, `sysinfo_test.c`
- `kernel/vm.c`: lazy page allocation 확장 (멤버 B 영역)
- `kernel/fs.c`: `ireclaim()` orphaned inode 회수 (멤버 B 영역)

---

## 🔑 sys_ps 인터페이스 (이미 구현됨)

```c
// kernel/procinfo.h
struct procinfo {
  int pid;
  int ppid;
  int state;        // procstate enum 값 (UNUSED..ZOMBIE)
  int priority;     // 0=highest .. 20=lowest
  uint64 sz;
  char name[16];
};

// 사용자에서:  int ps(struct procinfo *buf, int max);
//             반환: 채운 entry 수, 실패 -1
```

`user/ps.c` 출력 포맷 (호스트 브리지가 정규식으로 파싱):
```
PID  PPID  STATE    PRIO  SZ        NAME
1  0  sleep  10  16384  init
2  1  sleep  10  20480  sh
TOTAL 3
```

주의: `sys_ps`는 `parent` 접근에 `wait_lock`을 잡지 않음 (xv6 `procdump()`와 동일한 race-tolerant 패턴). 디버깅 출력용이라 OK.

---

## 🔑 MLFQ 설계 (W12 목표 — 아직 미구현)

### 큐 레벨
- L0 (HIGH): priority 0–6
- L1 (NORMAL): priority 7–13
- L2 (LOW): priority 14–20

### 동작
1. 새 프로세스 시작 시 priority=10 (L1)
2. timer tick마다 RUNNING proc의 `run_ticks++`
3. `run_ticks >= QUANTUM[level]` 도달 시: priority += 1 (강등), run_ticks = 0
4. 매 BOOST_INTERVAL(예: 100 tick)마다 모든 priority를 0으로 reset (starvation 방지)

### 새 필드 (proc.h에 추가 예정)
```c
int run_ticks;      // 현재 quantum에서 사용한 tick
```

기존 `priority` 필드는 그대로 유지 → `setpriority`/`getpriority` 인터페이스 호환 유지.

---

## 🔑 호스트 NL 브리지 (`host/nl_bridge.py`, 이미 구현됨)

### Intent 종류
- `PS` — 프로세스 목록
- `SETPRIO {pid, prio}` — 우선순위 변경
- `SPAWN {cmd}` — 사용자 프로그램 실행
- `KILL {pid}` — 프로세스 종료
- `EXPLAIN {about}` — LLM이 그냥 답변
- `REJECT` — 안전하지 않거나 범위 외

### Solar API (있을 때) / 오프라인 파서 (없을 때) 폴백
- `UPSTAGE_API_KEY` (또는 `SOLAR_API_KEY`) 환경변수로 활성화
- 키 없으면 정규식 기반 오프라인 파서로 폴백 → 로컬 개발 가능

### 안전 가드
- `KILL`은 PID=0, 1 (init) 거부
- `SPAWN`은 화이트리스트 (`SPAWN_WHITELIST` in `nl_bridge.py`)
- `SETPRIO`는 `0 <= prio <= 20` 검증

### QEMUDriver
- `subprocess.Popen(['make', 'qemu'], cwd=XV6_DIR, ...)` 로 부팅
- `proc.stdin`에 명령 송신, `proc.stdout` 읽어서 `[sched]/[sleep]/[wakeup]` 노이즈 정규식으로 필터

---

## 🛠️ 빌드 / 실행 명령어 (자주 씀)

### xv6 커널만 빌드
```bash
cd /Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv
make kernel/kernel
```

### 사용자 프로그램까지 + fs.img 재생성
```bash
make fs.img
```

### QEMU 직접 실행 (인터랙티브)
```bash
make qemu
# 종료: Ctrl-A 다음 X
```

### 자동 테스트 (xv6 트리에 이미 있음)
```bash
./test-xv6.py usertests          # 전체
./test-xv6.py -q usertests       # 빠른 버전
./test-xv6.py crash               # log/forphan/dorphan
```

### 호스트 브리지 실행
```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/host"
export UPSTAGE_API_KEY=sk-...     # 선택 (없으면 오프라인 파서)
python3 nl_bridge.py
```

---

## 🛠️ 기술 스택 / 환경

- **xv6**: RISC-V 64-bit, QEMU 7.2+
- **빌드 도구**: `riscv64-elf-gcc` (Mac brew: `riscv64-elf-binutils`, `riscv64-elf-gcc`)
- **호스트**: Python 3.11+, 표준 라이브러리만 (Solar API 호출 시 `requests` 추가)
- **LLM**: Upstage Solar Pro 3 (OpenAI 호환), `solar-pro2` 모델

---

## 📅 주차별 로드맵

| 주차 | 마일스톤 | 내(A) 작업 |
|---|---|---|
| W10 ✅ | 아키텍처 확정, MVP | 새 CLAUDE.md, sys_ps/ps, host/nl_bridge.py 스켈레톤 |
| W11 | NL→ps 1개 명령 end-to-end | 실 Solar API 연동, QEMU pty 안정화 |
| W12 | MLFQ + 평가 지표 | proc.c MLFQ 확장, run_ticks/boost, 평가 자동화 |
| W13 | 평가 + 발표 리허설 | 처리량/공정성 그래프 |
| W14 | 최종 발표 (영어) | |

---

## 📊 평가 지표

1. 자연어 명령 성공률 (목표 ≥85%)
2. **MLFQ 공정성**: HIGH/NORMAL/LOW 평균 대기 tick 비율 ← 내 모듈
3. **동시 작업 처리량**: N개 fork 동시 제출 시 평균 완료 tick ← 내 모듈
4. xv6 usertests 통과 유지 (회귀 없음)
5. 안전 가드 false positive (악성 명령 차단율)

---

## 🧑‍💻 코딩 규칙

### xv6 (C)
- 기존 xv6 스타일 (2-space indent, K&R brace)
- `-Wall -Werror`, 경고 0
- syscall은 `argint`/`argaddr`/`argstr`로 인자 추출
- proc 테이블 접근은 반드시 `p->lock` 안에서
- 머지 전 디버그 `printf` 제거

### Python (호스트)
- PEP 8
- Type hint
- 표준 라이브러리 우선; 외부 패키지는 lazy import (`requests`)

### 브랜치
- `main` 보호, PR로만 머지
- `feature/<모듈>-<작업>`: 예) `feature/proc-mlfq`, `feature/host-bridge`

---

## ⚠️ 함정 (xv6 + LLM 특화)

### 1. xv6는 네트워크 X
- xv6 안에서 직접 Solar API 호출 불가 → 반드시 호스트 브리지 구조

### 2. proc 테이블 락 순서
- xv6는 `p->lock`, `wait_lock`, `tickslock` 등 락이 여럿
- 정확히 하려면 `parent` 접근에 `wait_lock`도 잡아야 함
- 디버깅용 `ps`/`procdump`는 race를 허용하는 게 일반적

### 3. MLFQ + timer tick (W12 작업 시)
- timer 인터럽트는 `kerneltrap`/`usertrap` 양쪽에서 옴
- run_ticks++는 user mode 진입 시점이 안전 (`yield()` 직전)
- BOOST_INTERVAL는 `tickslock` 안에서만 비교

### 4. priority_test 호환성
- 기존 `priority_test.c`는 default=10, range [0,20] 가정
- MLFQ로 바꿔도 인터페이스(setpriority/getpriority)는 유지

### 5. `[sched]` printf — 출력 도배
- 현재 scheduler가 매 스위치마다 printf
- 호스트 브리지는 `_NOISE_RE`로 필터링 중이지만 임시방편
- 멤버 B와 합의 후 `#ifdef SCHED_DEBUG` 가드 권장

### 6. Makefile UPROGS — 새 user 프로그램 추가 시
- syscall 새로 추가하면 6곳 수정: `kernel/syscall.h` (번호), `kernel/syscall.c` (extern + 배열 + 이름 배열), `kernel/sysproc.c` (함수), `user/usys.pl` (stub), `user/user.h` (선언)
- 새 user 프로그램은 `Makefile`의 `UPROGS`에 `$U/_xxx` 추가

---

## 🤖 Claude Code 활용 가이드

### 좋은 패턴
- ✅ "kernel/sysproc.c에 sys_ps 함수만 짜줘. 인터페이스는 (struct procinfo *, int max) → int."
- ✅ "kernel/proc.c scheduler() 안의 priority 비교 부분만 MLFQ 3-큐로 바꿔줘."
- ✅ "host/nl_bridge.py에 KILL intent 처리 흐름 추가."

### 나쁜 패턴
- ❌ "xv6에 LLM 통합해줘" (너무 추상)
- ❌ "스케줄러 다시 짜줘" (기존 코드 컨텍스트 빠짐)

### 반드시 검증할 것
- syscall 추가는 6곳을 다 손대야 함 (위 함정 #6 참조)
- MLFQ 변경 후 `usertests`, `priority_test`가 통과하는지 확인
- 락 순서 위반 시 panic 발생 — 작은 변경마다 `make qemu` 돌려보기

---

## 🎤 "단순 래퍼"가 아닌 증거 (발표용)

1. ✅ xv6 커널 `proc.c`의 priority 스케줄러 → MLFQ 확장 (C 코드, LLM과 무관)
2. ✅ `sys_ps`, `sys_setpriority` 직접 구현 (proc table → user space copyout)
3. 🚧 timer tick 기반 quantum/boost 메커니즘 (trap.c 수정, W12)
4. ✅ ireclaim/lazy alloc (멤버 B) — 커널 자료구조 직접 수정
5. ✅ LLM은 호스트 브리지로만 사용 (전체 코드의 5% 이하)

---

## ✏️ 현재 진행 상황

- [x] W9: 팀 구성, 방향 결정
- [x] **W10**: 아키텍처 확정, sys_ps/ps MVP, host bridge 스켈레톤 — **완료**
- [ ] W11: NL → xv6 명령 end-to-end (실 Solar API 연동)
- [ ] W12: MLFQ 완성
- [ ] W13: 평가
- [ ] W14: 최종 발표
