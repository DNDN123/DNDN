# SmartShell — 발표 종합 자료 (한국어)

xv6-riscv 기반 LLM for OS. 발표 직전에 이 한 파일만 보면 되도록 정리.

---

## 0. 발표 전 체크리스트 (T-30분)

- [ ] 노트북 충전 100% + 어댑터 지참
- [ ] 외부 디스플레이 연결 확인 (HDMI/USB-C)
- [ ] 터미널 폰트 16~18pt로 키워두기
- [ ] WiFi 연결 확인 (Solar API 호출용) — 안 되면 오프라인 파서로 폴백
- [ ] `pkill -f qemu-system-riscv64` 로 좀비 QEMU 정리
- [ ] 미리 한 번 빌드 (`cd xv6-riscv && make`)
- [ ] 백업 GIF/스크린샷 준비 (라이브 데모 실패 대비)
- [ ] `UPSTAGE_API_KEY` 환경변수 export 됐는지 확인
- [ ] PRESENTATION_EN.md 슬라이드 파일 PPT/Keynote로 변환 완료

---

## 1. 실행 방법 (Step-by-Step)

### 1.1 환경 준비 (최초 1회)

**macOS (Homebrew)**
```bash
brew tap riscv-software-src/riscv
brew install riscv64-elf-gcc riscv64-elf-binutils qemu
```

**Ubuntu 22.04**
```bash
sudo apt update
sudo apt install -y qemu-system-misc gcc-riscv64-unknown-elf gdb-multiarch
```

**Python 3.11+ 확인**
```bash
python3 --version    # 3.11 이상
```

**Solar API 키 (선택 — 없으면 오프라인 파서로 폴백)**
```bash
export UPSTAGE_API_KEY=sk-...
# 또는 host/.env 파일에 UPSTAGE_API_KEY=... 한 줄 추가
```

### 1.2 빌드

```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv"
make clean        # 이전 .o/.d 정리 (선택)
make              # 커널 + 사용자 프로그램 + fs.img 다 빌드
```

빌드 산출물:
- `kernel/kernel` — 커널 ELF
- `fs.img` — 사용자 프로그램 포함 디스크 이미지
- `user/_ps`, `user/_setprio`, `user/_mlfq_bench` 등

### 1.3 실행 — 3가지 방법

#### A. QEMU 직접 실행 (가장 단순)
```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv"
make qemu
```
부팅 후 `$` 프롬프트가 나옴. 종료는 **Ctrl-A → X**.

xv6 셸 안에서 직접 테스트:
```
$ ps
$ setprio 2 5
$ priority_test
$ mlfq_bench
$ usertests -q
```

#### B. 호스트 NL 브리지로 실행 (발표 데모용)
```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/host"
python3 nl_bridge.py
```
자동으로 QEMU를 띄우고 `smart>` 프롬프트가 나옴. 자연어로 명령:
```
smart> 프로세스 보여줘
smart> set pid 2 priority to 3
smart> launch mlfq_bench
smart> kill 1                   ← REJECT 시연
smart> Ctrl-D                   ← 종료
```

#### C. 자동 회귀 테스트
```bash
cd "/Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv"
./test-xv6.py -q usertests     # xv6 usertests 빠른 버전
./test-xv6.py priority_test    # 우선순위 테스트만
./test-xv6.py crash            # 크래시 회복 테스트
```

### 1.4 문제 발생 시 빠른 복구

| 증상 | 해결 |
|---|---|
| 빌드 깨짐 | `make clean && make` |
| `make qemu` 후 한 명령 후 멈춤 | `pkill -f qemu` → 재실행 |
| `smart>` 프롬프트 안 나옴 | 4~6초 더 기다림, 그래도 안 나오면 빌드 실패 |
| Solar API 502/429 | 일시 오류, 재시도 또는 `unset UPSTAGE_API_KEY`로 오프라인 |
| GCC 15 `-Werror` | `Makefile`에 이미 `-Wno-unused-but-set-variable` 적용됨 |

---

## 2. 발표 개요 (10분 기준)

| # | 슬라이드 | 시간 | 핵심 메시지 |
|---|---|---|---|
| 1 | 표지 | 0:30 | 프로젝트 한 줄 |
| 2 | 문제 | 0:45 | "xv6는 진입장벽이 높다" |
| 3 | 아키텍처 | 1:00 | 호스트(통역) + xv6(실제 일) 분리 |
| 4 | **라이브 데모** | 3:00 | ⭐ 클라이맥스 |
| 5 | 커널 변경 ① `sys_ps` | 0:45 | 6곳 통합 = 진짜 커널 일 |
| 6 | 커널 변경 ② **MLFQ** | 1:30 | ⭐ 가장 기술적 슬라이드 |
| 7 | 호스트 브리지 + 안전 가드 | 1:00 | LLM이 함부로 못 함 |
| 8 | 평가 결과 | 1:00 | MLFQ 공정성 수치 |
| 9 | "단순 래퍼 아님" 증거 | 0:45 | C 250줄 vs LLM 30줄 |
| 10 | 한계 + Q&A | 1:00+ | 정직하게 |

---

## 3. 슬라이드별 스크립트 (발표용)

### 슬라이드 1 — 표지

**보여줄 것**
> SmartShell: A Natural-Language Interface for xv6-riscv
> *LLM for OS — 방향 B*
> 팀명 · 운영체제 학기말 프로젝트 · 2026

**말할 것 (30초)**
> "안녕하세요. 저희 팀 [이름]은 운영체제 학기말 프로젝트로 **SmartShell** 을 만들었습니다.
> 한 줄로 말하면, xv6 운영체제 위에 자연어 인터페이스를 얹은 시스템입니다.
> 단, 핵심 메시지는 — **진짜 작업은 커널에서 일어납니다**. LLM은 통역사일 뿐입니다."

---

### 슬라이드 2 — 문제 정의

**보여줄 것**
- xv6를 **읽을** 수는 있어도 **다루기** 어려움
- `ps`, `kill 5`, `setpriority 5 3`, `priority_test` … 명령 외워야 함
- GUI 없음, 네트워크 없음 → 자동화도 어려움
- 한국어/영어 사용자가 각자 다른 cheat sheet 필요

**목표**: 커널은 순수 xv6 유지하면서, **사용자가 자연어로 조작** 할 수 있게.
단, OS 불변식(invariant)은 절대 약화시키지 않음.

**말할 것 (45초)**
> "xv6는 교육용으로 훌륭하지만, 명령어를 외워야 다룰 수 있습니다.
> `ps`, `kill`, `setpriority` 같은 명령들 말이죠. GUI도 없고 네트워크도 없습니다.
> 저희가 풀고 싶은 문제는 — **xv6 커널은 그대로 두면서, 사용자가 한국어나 영어로 OS를 조작** 할 수 있게 만들자. 단, 안전성을 절대 양보하지 않으면서요."

---

### 슬라이드 3 — 아키텍처

**보여줄 것 (다이어그램)**
```
[사용자 자연어]
      ↓
[Python NL 브리지] ─── Solar Pro 3 (Intent JSON)
      ↓ stdin/stdout
[QEMU + xv6-riscv]
   ├ user: ps / setprio / mlfq_bench
   ├ syscall: SYS_ps, SYS_setpriority
   └ kernel: MLFQ 스케줄러, timer hook
```

**두 부분으로 분리**:
1. **호스트 (Python, ~360줄)** — 번역 + 안전 검사 + I/O
2. **xv6 커널 (C, ~250줄 추가/수정)** — 진짜 OS 작업

**말할 것 (1분)**
> "구조는 두 부분입니다. 왼쪽 호스트 측은 Python으로 짠 NL 브리지입니다.
> Upstage Solar Pro 3를 호출해서 자연어를 구조화된 Intent JSON으로 바꿉니다.
> 오른쪽은 QEMU 안에서 도는 xv6입니다. 둘은 stdin/stdout 파이프로 연결됩니다.
> **중요한 점** — xv6는 네트워크 스택이 없습니다. 그래서 LLM은 **물리적으로 커널 안에 살 수 없고**, 호스트로 분리될 수밖에 없습니다. 이건 핵심 설계 결정이지 hack이 아닙니다."

---

### 슬라이드 4 — 라이브 데모 (3분, 클라이맥스)

**시연 시나리오**
```
smart> 프로세스 보여줘
3 active process(es):
  pid=  1 sleep   prio=10 name=init
  pid=  2 sleep   prio=10 name=sh
  pid=  3 run     prio=10 name=ps

smart> show me the running processes
(같은 결과 — 한국어/영어 둘 다 됨)

smart> set pid 2 priority to 3
  -> intent: SETPRIO {'pid': 2, 'prio': 3}
OK pid=2 prio=3

smart> 프로세스 다시 보여줘
(pid 2의 prio가 3으로 변경된 것 확인)

smart> launch mlfq_bench
BENCH name=HIGH prio=1 ticks=4
BENCH name=NORMAL prio=10 ticks=8
BENCH name=LOW prio=19 ticks=11

smart> kill 1
⚠️  rejected: refusing to kill pid 1 (init)

smart> rm -rf the system
⚠️  rejected: program not in whitelist
```

**말할 것 (3분, 강조 포인트)**
- 한국어 입력 후 → "AI가 한국어를 이해했지만, **실제 kill, priority 변경, 스케줄링은 C 커널이 한 것**"
- mlfq_bench 결과 시 → "HIGH가 NORMAL의 절반 ticks에 끝났습니다. 이건 우리가 직접 짠 MLFQ 스케줄러의 결과입니다."
- kill 1 거부 → "안전 가드가 동작합니다. AI가 시켜도 init은 안 죽입니다."

**라이브 데모 실패 시 백업**
- 미리 녹화한 GIF/MOV 재생
- 또는 스크린샷 6장 슬라이드로 차례차례

---

### 슬라이드 5 — 커널 변경 ① `sys_ps` 시스템 콜

**보여줄 것**
```c
// kernel/procinfo.h
struct procinfo {
  int pid, ppid, state, priority;
  uint64 sz;
  char name[16];
};

// kernel/sysproc.c — 핵심 부분
uint64 sys_ps(void) {
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    // ... fill procinfo ...
    release(&p->lock);
    copyout(pgtbl, ubuf, &info, sizeof(info));
  }
  return count;
}
```

**6곳 통합 (xv6 syscall 추가 비용)**:
| `syscall.h` | `syscall.c` (×3) | `sysproc.c` | `usys.pl` | `user.h` | `Makefile` |

**말할 것 (45초)**
> "첫 번째 커널 변경은 새 시스템 콜 `sys_ps`입니다. 프로세스 테이블을 순회하면서 각 `p->lock`을 잡고, `copyout()`으로 사용자 공간 버퍼에 안전하게 복사합니다.
> 새 syscall 하나를 추가하려면 xv6에서는 **6개 파일을 동시에 수정** 해야 합니다. 이게 '진짜 커널 작업'의 증거입니다 — 단순 스크립트가 아닙니다."

---

### 슬라이드 6 — 커널 변경 ② MLFQ 스케줄러 ⭐ (가장 중요)

**보여줄 것**
```
L0 HIGH    priority  0..6   quantum  1 tick    (interactive)
L1 NORMAL  priority  7..13  quantum  4 ticks   (default)
L2 LOW     priority 14..20  quantum 16 ticks
```

**동작 3가지**:
1. **강등 (demote)** — 매 timer tick `mlfq_tick()`이 `run_ticks++`. quantum 소진 시 `priority++` (최대 20).
2. **부스트 (anti-starvation)** — 매 100 ticks마다 `mlfq_boost()` 가 모든 활성 proc을 priority 0으로 재설정.
3. **dispatch 시 리셋** — 새 슬라이스 시작 시 `run_ticks = 0`.

**기존 vs 새**:
- 기존: 단일 priority 비교 (가장 낮은 숫자 선택) — starvation 가능
- 새: 3단계 큐 + boost — 공정성 + 응답성 모두 확보

**말할 것 (1분 30초 — 가장 천천히)**
> "두 번째 커널 변경이 핵심입니다. xv6 기본 스케줄러를 **3단계 MLFQ로 교체** 했습니다.
> 우선순위 0~6은 HIGH 큐, 7~13은 NORMAL, 14~20은 LOW로 분류합니다.
> 각 큐의 quantum은 1, 4, 16 ticks입니다. — HIGH는 짧게짧게 자주(interactive), LOW는 길게 한 번(batch).
> 매 timer tick마다 `mlfq_tick()`이 동작 중인 프로세스의 슬라이스를 회계합니다. quantum을 다 쓰면 priority가 한 단 강등됩니다.
> 그러면 LOW 큐가 영원히 못 돌 수 있죠 — starvation. 그래서 **100 tick마다 모든 프로세스를 priority 0으로 boost** 합니다. 이건 OSTEP 8장 교과서 MLFQ 그대로입니다.
> 결과는 평가 슬라이드에서 보여드리겠습니다."

---

### 슬라이드 7 — 호스트 브리지 + 안전 가드

**보여줄 것 — 파이프라인**
```
사용자 자연어
   ↓
Solar Pro 3 호출 → Intent JSON
   ↓
guard() 검사 → REJECT or 통과
   ↓
to_xv6_cmd() → "ps", "setprio 2 3", ...
   ↓
QEMU stdin
   ↓
xv6 셸 실행 → stdout
   ↓
parse → summarise → 사용자에게 출력
```

**Intent 6종**: `PS / SETPRIO / SPAWN / KILL / EXPLAIN / REJECT`

**안전 가드**:
- `init` (pid 1) **절대** kill 안 됨
- `SPAWN`은 **화이트리스트** (`ls`, `ps`, `cat`, `wc`, `grep`, `echo`, `priority_test`, `mlfq_bench`, `setprio`만 허용)
- `SETPRIO`는 `0 ≤ prio ≤ 20` 검증
- 모르는 명령 → `REJECT`

**Background reader thread** — QEMU stdout 가득 차서 BrokenPipe 나는 문제 해결.

**말할 것 (1분)**
> "호스트 브리지는 6가지 Intent로 자연어를 분류합니다.
> 핵심은 **안전 가드** 입니다. LLM이 무슨 말을 해도 — 'init 죽여', 'rm -rf', priority 100 — 다 차단합니다.
> SPAWN은 화이트리스트 외엔 안 됩니다. SETPRIO는 0-20 범위 강제입니다.
> 즉, **LLM이 함부로 OS를 조작할 수 없다는 게 디자인의 핵심** 입니다. 이게 없으면 자연어 인터페이스는 위험합니다."

---

### 슬라이드 8 — 평가 결과

**(1) MLFQ 공정성 — `mlfq_bench`, CPUS=1, 3 자식 동시**

| 자식 | priority | 완료 ticks | HIGH 대비 |
|---|---:|---:|---:|
| HIGH   | 1  | 4  | 1.00× |
| NORMAL | 10 | 8  | 2.00× |
| LOW    | 19 | 11 | 2.75× |

- HIGH가 NORMAL의 **2배 빠름** — 큐 우선순위 효과
- LOW도 HIGH의 **3배 이내**로 반드시 완료 — boost가 starvation 차단

**(2) NL 정확도 — 수동 6개 입력**

| 입력 | Intent | 결과 |
|---|---|---|
| `프로세스 보여줘` | PS | ✅ |
| `show me running processes` | PS | ✅ |
| `set pid 2 priority to 3` | SETPRIO | ✅ |
| `launch priority_test` | SPAWN | ✅ |
| `kill 1` | KILL → guard REJECT | ✅ |
| `rm -rf the system` | REJECT | ✅ |

**(3) xv6 회귀** — `usertests -q`: **26 PASS / 1 FAIL** (96.3%)
- 실패 1건: `reparent2` (800회 fork 폭주) — NPROC 한계, MLFQ 튜닝 이슈로 분리

**말할 것 (1분)**
> "왼쪽 표가 헤드라인 수치입니다. CPUS=1 환경에서 우선순위가 다른 3개의 자식 프로세스를 동시에 띄웠을 때, HIGH는 4 ticks, NORMAL은 8 ticks, LOW는 11 ticks에 완료했습니다.
> 비율 1:2:2.75. HIGH가 2배 빠르고, LOW도 3배 안에 끝납니다. 즉, **공정성과 응답성을 동시에 확보** 했습니다.
> 자연어 정확도는 한국어/영어 6개 입력에서 6/6 모두 정확하게 매핑됐습니다.
> xv6 회귀는 usertests 27개 중 26개 통과. 실패한 `reparent2`는 800회 fork 폭주 부하 테스트로, MLFQ 컨텍스트 스위치 비용 증가 때문이며 일반 워크로드에선 영향 없습니다."

---

### 슬라이드 9 — "단순 LLM 래퍼 아님" 증거

| 증거 | 라인 수 | 레이어 |
|---|---:|---|
| MLFQ 스케줄러 + boost | ~70 | C 커널 |
| Timer-tick quantum 회계 | ~10 | C 커널 |
| `sys_ps` 시스템 콜 | ~40 | C 커널 |
| `ps`, `setprio`, `mlfq_bench` 사용자 프로그램 | ~120 | C 사용자 |
| 호스트 브리지 + 안전 가드 | ~360 | Python |

**핵심 비율**:
- **C OS 코드 ~250줄** vs **LLM 호출 코드 ~30줄**
- LLM은 UX 레이어. OS는 진짜 OS.

**말할 것 (45초)**
> "마지막으로 — 'AI 챗봇 래퍼 아니냐'는 질문에 대답합니다.
> C 커널 코드를 약 250줄 직접 작성/수정했습니다. 그중 가장 큰 덩어리가 MLFQ 스케줄러입니다.
> 반면 LLM 호출 코드는 30줄 남짓 — 전체의 5% 이하입니다.
> 즉, **LLM은 UX, OS는 진짜 OS** 입니다. 이게 방향 B 'LLM for OS'의 핵심입니다."

---

### 슬라이드 10 — 한계 + 향후 작업 + Q&A

**한계**
- MLFQ 공정성은 `CPUS=1`에서만 측정 가능 (3 CPU에서는 3 proc이 동시 실행돼 경합 없음)
- 호스트 브리지가 `time.sleep`으로 I/O 동기화 — pty 기반 프롬프트 검출이 더 안정적
- Solar API 일시 장애 시 오프라인 정규식 파서로 폴백 (NL 커버리지 제한)
- `usertests` 중 `reparent2` 1건 실패 (정직하게 언급)

**향후 작업**
- pty driver로 `$` 프롬프트 검출 기반 동기화
- 추가 Intent: 메모리 (`MEMINFO`), 파일시스템 (`LS`, `CAT`)
- Boost interval sweep — 공정성 vs 처리량 trade-off 곡선
- 호스트 브리지 Docker 컨테이너화 (one-command demo)

**말할 것 (1분 + Q&A)**
> "마무리하면서, 정직하게 한계를 짚습니다. MLFQ 공정성은 단일 CPU에서만 의미 있는 수치입니다. 호스트 I/O 동기화도 sleep 기반이라 가끔 흔들립니다. usertests 1건은 실패합니다.
> 향후 작업으로는 pty 기반 동기화, 메모리/FS Intent 추가, Docker화를 생각하고 있습니다.
> **감사합니다. 질문 받겠습니다.**"

---

## 4. 데모 시연 명령어 (그대로 복사)

```
# 1. 한국어 PS
프로세스 보여줘

# 2. 영어 PS (다국어 시연)
show me the running processes

# 3. 우선순위 변경
set pid 2 priority to 3

# 4. 변경 확인
프로세스 다시 보여줘

# 5. MLFQ 벤치 (가장 임팩트 있음 — 약 20초 대기)
launch mlfq_bench

# 6. 안전 가드 ① — init kill 거부
kill 1

# 7. 안전 가드 ② — 화이트리스트 외
rm -rf the system

# 8. (선택) EXPLAIN intent
fork가 뭐야?

# 종료
Ctrl-D
```

---

## 5. 예상 Q&A

### Q1. "그냥 LLM API 래퍼 아닌가요?"
> "아니요. C 커널 코드를 250줄 직접 작성했고, LLM 호출 코드는 30줄 남짓입니다. MLFQ 스케줄러, `sys_ps` syscall, timer tick 회계 — 모두 우리가 직접 짠 OS 코드입니다. LLM은 자연어 → Intent JSON 변환만 합니다. 진짜 작업은 xv6 커널 안에서 일어납니다."

### Q2. "MLFQ가 기존 priority 비교와 뭐가 다른가요?"
> "기존은 가장 낮은 priority 숫자를 가진 proc 1개를 매번 선택했습니다. 이러면 priority가 낮은 (숫자가 큰) 프로세스는 영원히 못 돌 수 있습니다 — starvation. MLFQ는 3단계 큐로 분리해서 각 큐마다 quantum을 다르게 주고(1/4/16 ticks), 100 ticks마다 boost로 모두 priority 0으로 끌어올립니다. 이게 OSTEP 8장 교과서 MLFQ입니다."

### Q3. "왜 LLM을 xv6 안에 안 넣고 호스트에 뒀나요?"
> "xv6에는 네트워크 스택이 없어서 HTTPS 호출 자체가 불가능합니다. 또 LLM 모델은 GB 단위 크기라 xv6 안에서 돌릴 수도 없습니다. 그래서 호스트 측 브리지로 분리하는 게 자연스러운 설계입니다. xv6는 OS의 본질에 집중하고, LLM은 UX 레이어로 분리하는 거죠."

### Q4. "안전 가드는 어떻게 보장하나요? LLM이 우회하면?"
> "안전 가드는 LLM 출력 **이후** 호스트 브리지에서 실행됩니다. LLM이 어떤 Intent를 만들어도, `guard()` 함수가 (1) pid 1 kill 거부, (2) SPAWN 화이트리스트, (3) prio 0-20 범위를 강제합니다. LLM의 답변은 신뢰하지 않고 항상 검증합니다."

### Q5. "MLFQ 공정성 측정이 CPUS=1에서만 된다는데, 의미 있나요?"
> "네, 학술적으로는 단일 CPU에서 공정성을 측정하는 게 표준입니다. 멀티 CPU에서는 N개 proc이 N개 코어에서 병렬로 돌면 경합 자체가 없어서 스케줄러의 효과를 측정할 수 없습니다. 이건 OSTEP에서도 같은 접근입니다. 다만 향후 작업으로 boost interval을 sweep해서 trade-off 곡선을 그려보는 걸 생각하고 있습니다."

### Q6. "usertests reparent2 실패는 큰 문제 아닌가요?"
> "정직하게 한계를 말씀드리면, reparent2는 800회 fork를 연속 폭주시키는 부하 테스트입니다. MLFQ의 잦은 컨텍스트 스위치 + 100 tick boost 시 proc[] 락 경합 때문에 init이 좀비 reparent 처리를 못 따라가서 NPROC=64에 도달하고 fork가 실패합니다. quantum을 {1,4,16} → {4,8,20}으로 늘리거나 boost 간격을 100 → 300 ticks로 늘리면 통과할 가능성이 높지만, 일반 워크로드에선 영향 없어서 분리했습니다."

### Q7. "Solar API가 죽으면 시스템이 멈추나요?"
> "아니요. API 키가 없거나 502/429를 받으면 정규식 기반 오프라인 파서로 폴백합니다. 한국어/영어 기본 패턴은 정규식으로도 잡힙니다. 단, 자유로운 NL 표현은 LLM 없이는 제한적입니다."

### Q8. "왜 Solar Pro 3인가요? GPT나 Claude는 안 썼나요?"
> "Upstage Solar Pro 3는 한국어 지원이 강하고 API 호환이 OpenAI 표준이라 교체가 쉽습니다. 학교 프로젝트 맥락에서 한국어 처리 품질이 중요했고, 한국 회사 모델이라는 점도 고려했습니다. 코드 구조상 base_url과 모델명만 바꾸면 다른 LLM도 즉시 됩니다."

### Q9. "팀에서 본인 기여는 정확히 뭔가요?"
> "저는 **프로세스/스케줄러** 영역을 담당했습니다. 구체적으로는 (1) `sys_ps` 시스템 콜 구현, (2) MLFQ 3단계 스케줄러로 `kernel/proc.c` 교체, (3) timer tick에서 quantum 회계 + boost 메커니즘, (4) 호스트 NL 브리지의 process intent 처리부입니다. 멤버 B는 lazy alloc과 ireclaim (FS/메모리), 멤버 C는 LLM 호스트 인프라, 멤버 D는 평가/문서를 담당했습니다."

### Q10. "이게 실제 OS에 쓰일 수 있나요?"
> "현재 형태로는 교육/연구용입니다. 실제 OS에 쓰려면 (1) 안전 가드를 SELinux/AppArmor 수준으로 강화, (2) LLM을 로컬 모델로 (네트워크 의존 제거), (3) audit log + rollback 메커니즘이 필요합니다. 다만 'Linux + 자연어 셸' 같은 응용은 충분히 가능한 방향이라고 봅니다."

---

## 6. 발표 직후 정리

- [ ] Q&A 받은 질문 메모 (다음 발표/보고서 반영)
- [ ] 데모 실패 / 잘된 부분 기록
- [ ] `pkill -f qemu` 로 좀비 정리
- [ ] (선택) 발표 슬라이드 PDF 저장

---

## 부록 A — 코드 위치 빠른 참조 (Q&A 중 펴볼 수 있게)

| 토픽 | 파일 | 라인 |
|---|---|---|
| `sys_ps` 본체 | `kernel/sysproc.c` | 200~237 |
| `struct procinfo` | `kernel/procinfo.h` | 1~10 |
| MLFQ 큐 정의 | `kernel/proc.c` | 447~452 |
| `mlfq_tick` | `kernel/proc.c` | 483~493 |
| `mlfq_boost` | `kernel/proc.c` | 466~478 |
| MLFQ scheduler 선택 | `kernel/proc.c` | 520~554 |
| timer hook | `kernel/trap.c` | 85, 158, 182 |
| `run_ticks` 필드 | `kernel/proc.h` | 109 |
| Intent 정의 | `host/nl_bridge.py` | 62~90 |
| 안전 가드 | `host/nl_bridge.py` | 141~155 |
| Solar API 호출 | `host/nl_bridge.py` | 92~120 |
