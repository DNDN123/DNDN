# Syscall Number Allocation

W13 통합 시 syscall 번호 충돌이 빌드 실패의 가장 큰 원인이 됩니다. 4팀 슬라이스가
모두 같은 xv6 트리에 머지되어야 하므로 **각 슬라이스의 syscall 번호 영역을 미리
분배해 두는 게 핵심**입니다.

## 표준 분배표

| 영역 | 슬라이스 | 담당자 (브랜치) | 비고 |
|---|---|---|---|
| 1 ~ 21 | (stock xv6) | — | 강의 원본 그대로 |
| **22 ~ 24** | **Syscall (trace)** | `minju` | `trace_on / trace_off / trace_stats` |
| **25 ~ 29** | **Thread** | `jinhwan` | `thread_create / join / exit / futex_wait / futex_wake` (5칸 = 정확히 필요한 수) |
| **30 ~ 33** | **Scheduler (MLFQ)** | `hyunsung` | `setpri / getstats / settrace / forkpri` |
| 34 ~ | **Process** | `haneol` | `sys_ps` 등 — 시작 번호 합의 필요 |
| 64 이상 | (예약) | — | NELEM(syscalls) 안에 들도록 향후 64 미만 권장 |

## 충돌 이력 (정리 전)

- `hyunsung`: 22 setpri / 23 getstats / 24 settrace / 25 forkpri
- `minju`:    22 trace_on / 23 trace_off / 24 trace_stats
- `jinhwan`:  22 thread_create / 23 thread_join / 24 thread_exit / 25 futex_wait / 26 futex_wake

→ 세 슬라이스가 **22~24를 동시에 사용**. 어느 두 슬라이스든 머지하면 즉시 컴파일
실패 또는 silently wrong dispatch.

## 정리 후 (2026-05-20 기준)

- `hyunsung`: **30~33으로 이동 완료** (`smart-mlfq-xv6-patches/kernel/syscall.h`)
- `minju`:    22~24 유지 권장 (가장 먼저 commit, 트레이싱은 LLM 진단의 입구)
- `jinhwan`:  **25~29로 이동 필요** — 두 사람 합의 후 본인이 변경

## 변경 절차

본인 슬라이스 번호를 옮겨야 하는 사람은 해당 슬라이스의 **`kernel/syscall.h` 한 파일만**
바꾸면 됩니다. 나머지(`kernel/syscall.c`, `user/usys.pl`, `user/user.h`)는 모두 매크로
이름(`SYS_xxx`)으로만 참조하므로 자동으로 새 번호를 따라갑니다.

확인 명령:
```bash
grep -nE 'SYS_[a-z_]+' kernel/syscall.h kernel/syscall.c user/usys.pl user/user.h
```

## 잠금 규칙

- 새 syscall을 추가할 사람은 **이 문서를 먼저 갱신**한 뒤 합의받고 코드 push.
- 임시로 다른 영역의 번호를 빌려 쓰지 말 것 — 통합 시 두 번 일하게 됨.
