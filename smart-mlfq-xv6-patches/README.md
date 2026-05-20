# Smart-MLFQ-xv6 — Kernel Patch Bundle

> 사용자가 작성한 xv6 커널 코드(Smart-MLFQ-xv6 프로젝트)에 대한
> 코드 리뷰 결과와 6건의 패치를 담은 번들입니다.

## 📦 번들 내용

```
smart-mlfq-xv6-patches/
├── README.md                    # 이 파일
├── CHANGES.diff                 # 원본 대비 unified diff
├── apply_patches.sh             # 자동 적용 스크립트
├── kernel/
│   ├── proc.c                   # 메인 수정 (스케줄러, 동기화, 새 헬퍼)
│   ├── proc.h                   # struct proc, procstats
│   ├── trap.c                   # 타이머 인터럽트 훅
│   ├── syscall.h                # SYS_setpri/getstats/settrace/forkpri
│   ├── syscall.c                # syscall 디스패치 테이블
│   ├── sysproc.c                # syscall 핸들러 4개
│   └── defs.h                   # 새 함수 프로토타입
└── user/
    ├── user.h                   # 유저 측 syscall 선언
    └── usys.pl                  # 어셈블리 stub 생성기
```

## 🚀 빠른 시작 (3단계)

### 1) 패치 적용

```bash
chmod +x apply_patches.sh
./apply_patches.sh /path/to/lecture-repo/xv6-riscv
```

스크립트는 적용 전 원본을 `xv6-riscv/.backup-pre-mlfq-<timestamp>/` 에
자동 백업합니다.

### 2) 빌드

```bash
cd /path/to/lecture-repo/xv6-riscv
make clean
make
```

### 3) 부팅 테스트

```bash
make qemu
# `$` 프롬프트가 뜨면 성공. 종료: Ctrl-A x
```

## 🔧 무엇을 수정했나 (요약)

원본 코드를 강의용 xv6와 비교 검토한 결과, 1건의 **빌드 실패 버그**와
5건의 **품질 이슈**를 발견하여 패치했습니다:

| # | 종류 | 한 줄 설명 |
|---|---|---|
| 🚨 | **Compat fix** | `forkret()`이 강의용 xv6에 없는 `usertrapret()` 호출 → 링크 실패 |
| C1 | Critical | `last_boost_tick` 멀티 CPU race → boost_lock 추가 |
| C2 | Critical | `total_io_blocks` 가 kwait/pause 도 카운트 → 필터링 |
| C3 | Critical | scheduler가 락 잡은 채로 printf → swtch 후로 이동 |
| I2 | Important | 무조건 trace ON → settrace() syscall 추가, 기본 off |
| I4 | Important | fork→setpri 사이 race → forkpri() syscall 추가 |

## 🆕 추가된 Syscall

```c
int settrace(int pid, int on);  // pid=0 means self. trace 토글
int forkpri(int level);          // level=0/1/2. priority를 갖고 fork
```

기존 syscall(`setpri`, `getstats`)는 번호도 시그니처도 그대로입니다.
호스트 Python 측 변경은 불필요합니다.

## ✅ 검증 상태

이 번들에서 (제한된 샌드박스 환경에서) 가능했던 검증:

- ✅ `gcc -fsyntax-only -Wall -Wextra -Werror` 통과
- ✅ Brace balance OK
- ✅ Syscall 번호 일관성 (syscall.h ↔ syscall.c ↔ sysproc.c ↔ user.h ↔ usys.pl)
- ✅ `struct procstats` 정의가 kernel/proc.h와 user/user.h에서 동일
- ✅ 함수별 lock acquire/release 패턴이 원본 xv6와 동일
- ✅ 패치 외 함수들이 강의용 xv6 원본과 비교 시 의도된 차이만 존재

본인 환경에서 빠뜨리지 않고 검증해야 할 것 (RISC-V 툴체인 필요):

- [ ] `make` 빌드 통과 (경고는 원본과 동일한 것만)
- [ ] `make qemu` 부팅 + `$` 진입
- [ ] 새 syscall 4개 (setpri/getstats/settrace/forkpri) 동작 확인
- [ ] 멀티 인스턴스 panic 없음
- [ ] baseline trace에서 `workload_runner.total_io_blocks ≈ 0` 확인

## 📚 다음 단계

1. **`workload_runner.c` 업데이트** — 새 syscall 활용:
   ```c
   settrace(0, 1);                    // 트레이스 켜기
   int pid = forkpri(hint_level);     // 우선순위와 함께 fork
   ```

2. **아키텍처 다이어그램 정정** — 다이어그램의 *"priority=2 LOW
   by default"* → `priority=0 HIGH` (코드 및 proposal.md와 일치)

3. **첫 실제 실험** — baseline 실행 후 `total_io_blocks` 분포 확인
