# Integration Checklist

4명의 슬라이스(`hyunsung` Scheduler / `haneol` Process / `minju` Syscall /
`jinhwan` Thread)를 단일 xv6 트리에 머지하는 작업 순서. 통합일 당일 패닉을
피하기 위한 체크리스트.

> 사전 가정: 각 슬라이스가 본인 브랜치에서 단독 빌드·시연 가능 상태.

---

## 0. 통합 전 합의 (회의 1회, ~30분)

- [ ] `docs/syscall-allocation.md` 표를 확정 (현재 안: minju 22~24 / jinhwan 25~29 / hyunsung 30~33 / haneol 34~)
- [ ] 본인 외 슬라이스의 syscall 번호 이동이 필요한 사람은 **이 회의 직후** 본인 브랜치에서 `kernel/syscall.h` 한 파일만 수정 후 push
- [ ] MLFQ 단일화 방향 합의 → **Scheduler = hyunsung 단일 채택**, haneol의 `proc.c` 안 MLFQ 블록 제거 합의
- [ ] 호스트 NL 브리지 단일화 방향 합의 → **`smart-mlfq-host/nl_shell.py`를 기준**, `nl_bridge.py`(haneol) + `solar_bridge.py`(jinhwan)는 자기 슬라이스 고유 Intent만 모듈로 분리
- [ ] 통합 작업자 1명 지정 (보통 Scheduler 담당자 = hyunsung)

---

## 1. 통합 브랜치 생성

```bash
# 통합 담당자(hyunsung 기준):
git checkout main
git pull
git checkout -b integration
git merge origin/hyunsung   # 본인 슬라이스 먼저 (가장 큰 변경)
```

---

## 2. 머지 순서 (충돌 적은 순)

> 큰 변경부터 들어가고 작은 변경이 그 위에 얹히는 게 충돌이 적음.

1. [ ] **hyunsung (Scheduler)** — kernel/proc.c, trap.c, sysproc.c, syscall.h, host/* 전부
2. [ ] **minju (Syscall)** — syscall.c의 후킹만 hyunsung 코드에 패치, `user/tracetool.c` 신규 추가, syscall 번호 22~24
3. [ ] **jinhwan (Thread)** — kernel/proc.c에 `kthread_*`/`kfutex_*` 추가, syscall 번호 25~29. **MLFQ 코드는 건드리지 말 것**
4. [ ] **haneol (Process)** — `kernel/procinfo.h`, `sys_ps` syscall (번호 34~), `user/ps.c`, `user/setprio.c`. **`proc.c`의 MLFQ 블록은 모두 제거하고 hyunsung 것으로 통일**

---

## 3. 충돌 해결 우선순위 표

| 충돌 영역 | 승자 |
|---|---|
| `kernel/proc.c` MLFQ 스케줄러 | hyunsung |
| `kernel/proc.c` `kthread_*` | jinhwan |
| `kernel/syscall.c` 후킹 (`p->syscall_count[]`) | minju |
| `kernel/syscall.h` 번호 영역 | `docs/syscall-allocation.md` 표 그대로 |
| 호스트 NL 브리지 진입점 | hyunsung (`nl_shell.py`) |
| `struct proc` 확장 필드 | 모든 슬라이스 필드 OR 결합 (충돌 거의 없음) |

---

## 4. 통합 후 빌드/부팅 검증

```bash
cd integration/xv6-riscv-tree  # 통합 트리 위치
make clean
make                                 # 컴파일 통과해야 다음 단계
make qemu CPUS=2                     # 부팅 + $ 프롬프트 확인
```

- [ ] `make` 경고가 stock xv6 대비 늘지 않음
- [ ] `$` 프롬프트 진입 (init 안 죽음)
- [ ] `usertests -q` 통과 (적어도 hyunsung 단독 브랜치와 동일 수준)

---

## 5. 슬라이스별 동작 시연 (4종 × 1분)

각 슬라이스가 본인 데모 한 줄을 통합 트리에서 그대로 실행:

```text
# hyunsung (Scheduler)
$ nlrun 2 cpu_burner 1000000
TRACE tick=… pri=2 …
EXIT  … final_pri=2

# minju (Syscall)
$ tracetool on
$ wrunner mixed.txt hints.txt
$ tracetool dump
{"pid":4,"total":18,"calls":{"fork":1,"exec":2,…}}

# jinhwan (Thread)
$ threadtest
[thread] created tid=… joined …

# haneol (Process)
$ ps
PID  PPID  STATE    PRIO  SZ        NAME
1    0     sleep    10    12288     init
$ setprio 4 5
OK pid=4 prio=5
```

각 라인이 정상 출력되면 통합 성공.

---

## 6. 통합 후 정량 평가 재실행

```bash
cd smart-mlfq-host
XV6_DIR=/path/to/integration/xv6-riscv \
    OUT_ROOT=docs/charts/integration \
    ./run_w12_matrix.sh
```

- [ ] W12와 동등하거나 더 좋은 결과 (다른 슬라이스가 스케줄러를 망가뜨리지 않았는지 확인)
- [ ] `docs/charts/integration/combined_report.txt` commit

---

## 7. 통합 PR 작성

- 본인이 통합 담당이면 `integration` → `main` PR
- 본문에 위 §5의 4종 시연 콘솔 캡처 첨부
- 4명 모두 리뷰 후 머지

---

## 8. 롤백 플랜

머지가 망가졌으면 빠르게 후퇴:

```bash
git checkout main         # 통합 안 한 상태로
git branch -D integration
# 각자 본인 브랜치로 돌아가 단독 시연 (발표 백업안)
```

본인 브랜치 단독 시연이 항상 가능하도록 유지하는 게 통합 최후 안전망. 시연 영상
(`docs/w11-hello-world.md §5`)을 미리 녹화해두면 통합 실패 시 라이브 데모 대신 영상으로 대체 가능.

---

## 9. 흔한 함정

| 증상 | 원인 | 대처 |
|---|---|---|
| `multiple definition of mlfq_*` | haneol의 MLFQ 블록이 남아있음 | §3 표대로 haneol 쪽 제거 |
| `undefined reference to sys_thread_create` | jinhwan 슬라이스가 syscall 번호 충돌로 빠짐 | §0 합의표 다시 확인, `syscall.h` 재정렬 |
| TRACE 라인이 안 찍힘 | `settrace(0,1)` 호출 누락 | `nlrun.c`/`wrunner.c`가 자동으로 켜 줌 — 다른 진입점은 명시 호출 필요 |
| 부팅 직후 panic | `kfork()` vs `kforkpri()` 충돌 | hyunsung 버전 사용, fork stub은 둘 다 등록 |
| Solar 호출이 두 번 일어남 | 호스트 브리지가 통합 안 됨 | §0 합의대로 `nl_shell.py` 단일화 |
