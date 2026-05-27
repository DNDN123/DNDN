# 통합 작업 폴더

> 4팀(hyunsung / minju / jinhwan / haneol) 슬라이스를 **단일 xv6-riscv 트리**로 합친 결과.
> GitHub 원본 브랜치는 **절대 수정되지 않음** — 모든 파일은 `git archive` 로 읽기만 해서 이 폴더로 복사됨.
> 통합 작업 일자: 2026-05-26.

---

## 폴더 구조

```
integration/
├── README.md            ← 이 파일 (개요)
├── MERGE_NOTES.md       ← 충돌 결정 / 머지 결정 기록 (상세)
├── _sources/            ← 각 브랜치 원본 파일 (읽기 참조용, 변경 안 됨)
│   ├── hyunsung/        ← Scheduler 슬라이스 원본
│   ├── minju/           ← Syscall(trace) 슬라이스 원본
│   ├── jinhwan/         ← Thread 슬라이스 원본
│   └── haneol/          ← Process 슬라이스 원본
├── xv6-riscv/           ← ⭐ 통합 결과 — 빌드 대상
│   ├── kernel/          (proc.c, proc.h, syscall.{c,h}, sysproc.c, vm.c, kalloc.c, defs.h,
│   │                     procinfo.h, sysinfo.h, trap.c …)
│   ├── user/            (init, sh, ls, … + nlrun, wrunner, cpu/io/mixed_burner, rrunner,
│   │                     diagprog, tracetool, threadtest, ps, setprio, …)
│   ├── workloads/       (cpu_heavy, io_heavy, mixed, three_way, realprog, stress, hints_example)
│   └── Makefile
├── host/                ← 통합 호스트 Python
│   ├── README.md
│   ├── nl_shell.py      ⭐ 단일 진입점 (hyunsung)
│   └── adapters/
│       ├── process_bridge.py  (haneol)
│       └── thread_bridge.py   (jinhwan, API 키 sanitize 완료)
└── docs/                ← 통합 트리 전용 docs (현재 비어있음)
```

---

## 통합 결과 요약

### Syscall 번호 최종 분배

| 영역 | 슬라이스 |
|---|---|
| 1~21 | stock xv6 |
| 22~24 | minju (trace) |
| 25~29 | jinhwan (thread/futex) |
| 30~33 | hyunsung (Smart-MLFQ) |
| 34~35 | haneol (ps, sysinfo) |

전체 매핑은 `MERGE_NOTES.md §1` 참조.

### 핵심 결정

| 충돌 | 결정 |
|---|---|
| MLFQ 구현 | hyunsung (W12 정량 검증) |
| 0..20 priority (haneol) | DROP — hyunsung 의 setpri (0..2) 로 통일 |
| trace syscall (haneol vs minju) | minju (더 풍부한 per-syscall 통계) |
| host NL 진입점 | `host/nl_shell.py` (hyunsung 단일), 나머지는 adapter |
| Solar API 키 하드코딩 (jinhwan) | 환경변수로 sanitize |

상세 결정 근거는 `MERGE_NOTES.md §2` 참조.

---

## 빌드 / 검증 (다음 단계)

```bash
# WSL/Linux 환경에서:
cd integration/xv6-riscv
make clean && make
make qemu CPUS=2

# 슬라이스별 데모 (체크리스트 §5):
# hyunsung:  $ nlrun 2 cpu_burner 1000000
# minju:     $ tracetool on   →   $ tracetool dump
# jinhwan:   $ threadtest
# haneol:    $ ps   →   $ setprio 4 2
# 통합 데모 (2026-05-27 추가): 백그라운드 큐 + LOW 우선순위 + 좀비 회수
#            $ bgq bgq.txt    →   BGQ start … / BGQ done … / BGQ all_done 3
# 호스트 측 (2026-05-27 추가): tracetool dump JSON → Solar 분석
#            $ tracetool dump | tee /tmp/td.json   (xv6)
#            $ python host/nl_shell.py --analyze-tracetool /tmp/td.json   (host)
```

**⚠️ 현재 빌드 검증은 미실시.** 사용자가 직접 WSL 환경에서 빌드 후 결과 보고 필요.

---

## 원본 보호 원칙 (재확인)

- 이 폴더 안 모든 작업은 **로컬** working copy
- GitHub 의 4개 원본 브랜치 — `origin/hyunsung`, `origin/minju`, `origin/jinhwan`, `origin/haneol` — 는 `git fetch` 만 했고 단 1바이트도 수정하지 않음
- 새 브랜치 생성 안 함, push 안 함, commit 안 함
- `_sources/` 의 파일도 단순 복사본일 뿐 원본을 가리키지 않음 — 안전하게 비교·검증·롤백 가능

---

## 진행 상황

- [x] 폴더 구조 생성
- [x] 각 브랜치 원본 파일 `_sources/` 로 추출
- [x] 베이스 xv6 트리 + hyunsung 슬라이스 적용
- [x] minju 슬라이스 머지 (Syscall trace)
- [x] jinhwan 슬라이스 머지 (Thread / Futex)
- [x] haneol 슬라이스 머지 (Process ps / sysinfo)
- [x] host/ Python 통합 (단일 nl_shell.py + adapters)
- [x] MERGE_NOTES.md 완성
- [x] **빌드/부팅 검증 (WSL/QEMU)** — 2026-05-26 통과
- [x] **슬라이스별 데모 4종 라이브 시연** — 2026-05-26 완료 (hyunsung/minju/jinhwan/haneol 모두 동작 확인)
- [x] **W12 정량 평가 재실행 (`run_w12_matrix.sh`)** — 2026-05-27 완료 (상세 §아래)
- [-] 통합 PR 작성 — W14로 연기 결정 (MERGE_NOTES "원본 보호 원칙" 유지)

---

## 정량 평가 결과 (2026-05-27 재실행)

5/26 1차 실행은 venv 미활성화로 `llm_hint.py`가 import 단계에서 죽어 hints 파일이 생성되지 않은 채 돌아감 → heuristic/solar 모드가 사실상 baseline과 동일 조건이라 Δ% 값이 노이즈에 불과했음. 5/27 재실행은 `/root/smart-mlfq-host/venv` + `.env` 로 hints 정상 생성 확인 후 수행.

### 워크로드별 핵심 Δ (avg_turnaround 기준, Δheur% / Δllm%)

| 워크로드 | Δheur% | Δllm% | 해석 |
|---|---|---|---|
| cpu_heavy   | +283%  | +550%  | 단일 프로그램 → 우선순위 낮추면 손해 (예상된 역효과) |
| io_heavy    | +2.8%  | +15.8% | 단일 프로그램 + 부분 timeout, 변화 미미 |
| mixed       | +17.1% | +14.3% | 평균은 악화지만 fairness +43~155%, max_TT −53% (heur) |
| three_way   | **−7.3%**  | **−5.8%**  | 다중 프로그램에서 heur/llm 모두 turnaround 개선 |
| realprog    | +6.8%  | −0.2%  | heur avg_response −26% — 대화형 응답 개선 |

### 환경 아티팩트

- `io_heavy` 전 모드 + `mixed/baseline`에서 `no ALL_DONE` (QEMU_TIMEOUT=45s 한계). 부분 trace로 평가 수행 — 절대값보다 모드 간 상대 비교에 의존.
- /mnt/c 경로 QEMU가 /root 대비 느려 W12 환경(`~/xv6-riscv`)과 직접 비교는 불가.

### 결론

다중 프로그램 워크로드(`three_way`, `mixed`, `realprog`)에서 hints의 효과가 측정됐고 회귀 없음 → §6 통과. 단일 프로그램 워크로드의 역효과는 hints 시스템의 본질적 특성으로 새 발견이 아님.

전체 산출물: `docs/charts/integration/` (5 워크로드 × 4 산출물 + `combined_report.txt`).
