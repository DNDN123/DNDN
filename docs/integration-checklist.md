# Integration Checklist

4명의 슬라이스(`hyunsung` Scheduler / `haneol` Process / `minju` Syscall /
`jinhwan` Thread)를 단일 xv6 트리에 머지하는 작업 순서. 통합일 당일 패닉을
피하기 위한 체크리스트.

> **Status (2026-05-27)**: 통합 작업은 2026-05-26~05-27에 완료. 본 체크리스트는
> 그 작업 순서의 reference. 각 항목의 결과는 `integration/MERGE_NOTES.md` 및
> `integration/README.md` §"진행 상황" 에 기록.
>
> 사전 가정: 각 슬라이스가 본인 브랜치에서 단독 빌드·시연 가능 상태.

---

## 0. 통합 전 합의 (회의 1회, ~30분)

- [x] `docs/syscall-allocation.md` 표 확정 (minju 22~24 / jinhwan 25~29 / hyunsung 30~33 / haneol 34~35) — `MERGE_NOTES.md §1`
- [x] 본인 외 슬라이스의 syscall 번호 이동 — 통합 시 일괄 적용 (`integration/xv6-riscv/kernel/syscall.h`)
- [x] MLFQ 단일화 → hyunsung 채택, haneol 0..20 priority DROP (`MERGE_NOTES.md §2.1`)
- [x] 호스트 NL 브리지 단일화 → `nl_shell.py` 기준, `nl_bridge.py`/`solar_bridge.py` 는 `adapters/` 모듈로 분리
- [x] 통합 작업자: hyunsung (Scheduler 담당)

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

- [x] `make` 경고가 stock xv6 대비 늘지 않음 — `LOAD segment RWX` 단일 경고만 (stock 동일)
- [x] `$` 프롬프트 진입 (init 안 죽음) — 5종 데모 정상 부팅 검증 (2026-05-26)
- [-] `usertests -q` 부분 실패 (2026-05-27) — 26/27 통과 후 `reparent2` 에서 fork failed. 격리 실험(`forkstress 1000` 신규 추가) 은 fresh 부팅에서 1000/1000 통과 → 기본 fork+wait 경로는 깨끗, 이전 26개 테스트 중 어딘가에서 슬롯 누수. 5-슬라이스 데모/`bgq.txt` 시연에는 영향 없음. 근본 원인은 `MERGE_NOTES §6.2 알려진 회귀` 에 기록, 후속 작업으로 보류.

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
1    0     sleep    1     12288     init
$ setprio 4 2
OK pid=4 level=2
```

각 라인이 정상 출력되면 통합 성공.

---

## 6. 통합 후 정량 평가 재실행

```bash
cd smart-mlfq-host
XV6_DIR=/path/to/integration/xv6-riscv \
    OUT_ROOT=docs/charts/integration \
    ./run_eval_matrix.sh
```

- [x] 단독 슬라이스와 동등하거나 더 좋은 결과 — `three_way` Δheur −7.3%, Δllm −5.8%; 회귀 없음 (`integration/MERGE_NOTES.md §5.3`)
- [x] `docs/charts/integration/combined_report.txt` commit — 5b810d3

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
(`docs/hello-world.md §5`)을 미리 녹화해두면 통합 실패 시 라이브 데모 대신 영상으로 대체 가능.

---

## 9. 흔한 함정

| 증상 | 원인 | 대처 |
|---|---|---|
| `multiple definition of mlfq_*` | haneol의 MLFQ 블록이 남아있음 | §3 표대로 haneol 쪽 제거 |
| `undefined reference to sys_thread_create` | jinhwan 슬라이스가 syscall 번호 충돌로 빠짐 | §0 합의표 다시 확인, `syscall.h` 재정렬 |
| TRACE 라인이 안 찍힘 | `settrace(0,1)` 호출 누락 | `nlrun.c`/`wrunner.c`가 자동으로 켜 줌 — 다른 진입점은 명시 호출 필요 |
| 부팅 직후 panic | `kfork()` vs `kforkpri()` 충돌 | hyunsung 버전 사용, fork stub은 둘 다 등록 |
| Solar 호출이 두 번 일어남 | 호스트 브리지가 통합 안 됨 | §0 합의대로 `nl_shell.py` 단일화 |
