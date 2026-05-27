# W12 정량 평가 결과 — Smart-MLFQ 3-way 비교

실험 일자: 2026-05-20
실험 환경: WSL Ubuntu, QEMU RISC-V, xv6-riscv (본인 MLFQ 패치 적용)

## 비교 대상 (3-way)

| 모드 | 설명 |
|---|---|
| **baseline** | 본인 MLFQ만 적용 (LLM 힌트 없음) |
| **heuristic** | 폴백 — Solar API 없이 키워드/통계 기반 힌트 |
| **solar (LLM)** | Solar Pro 3에게 baseline 통계 → 힌트 받음 |

## 워크로드 5종

- `cpu_heavy` — CPU-bound 프로세스 5개
- `io_heavy` — IO-bound 프로세스 4개
- `mixed` — CPU/IO/Mixed 혼합 7개
- `three_way` — 각 큐(HIGH/MID/LOW)에 1개씩
- `realprog` — 실제 xv6 프로그램 (cat, wc, ls)

## 핵심 발견

| 워크로드 | 결과 요약 |
|---|---|
| `cpu_heavy` | 모두 비슷 (avg_tt 1.2~1.6). 모두 fairness ≈ 0.9 |
| `io_heavy` | baseline 최고 — sleep retention이 이미 잘 작동 |
| **`mixed`** | **LLM이 heuristic보다 명확히 우수** (TT +24% vs +55%) |
| **`three_way`** | **LLM이 모든 모드 중 최고** (avg_tt 80.67) |
| `realprog` | LLM이 heuristic보다 약간 좋음, baseline에 근접 |

## 가장 강력한 발견 — `mixed` 워크로드 cpu_burner

```
baseline cpu_burner TT:  1.33
heuristic cpu_burner TT: 21.00  (+1478%)
LLM cpu_burner TT:        6.67  (+401%)
```

→ Heuristic은 cpu_burner를 무조건 LOW로 보내서 굶주리게 함.
   **LLM은 더 똑똑한 판단으로 cpu_burner를 덜 굶주리게** 하면서도 fairness 개선.

## 결론 (발표 포인트)

> *"LLM이 모든 워크로드에서 좋은 건 아니지만, 혼합/복잡한 워크로드에서는 도메인 지식 기반 휴리스틱을 명확히 능가한다. 비싼 LLM 호출이 실제로 가치 있는 지점을 정량적으로 확인."*

## 파일 구조

```
w12/
├── README.md                 (이 파일)
├── combined_report.txt       (5개 워크로드 통합 리포트)
└── <workload>/
    ├── metrics.json          (모든 metric, 머신 가독)
    ├── report.txt            (인쇄용 표)
    ├── avg_turnaround.png    (3-way 막대 비교)
    ├── per_pid_tt.png        (프로세스별 TT)
    ├── gantt_baseline.png    (baseline 스케줄링 시각화)
    └── gantt_llm.png         (LLM 스케줄링 시각화)
```

## 재현 방법

```bash
cd /root/smart-mlfq-host
source venv/bin/activate
OUT_ROOT=/root/mlfq-experiments/w12_full ./run_w12_matrix.sh
```

총 소요 시간: 약 7~8분 (15 QEMU runs × ~30초 + Solar API 5회)
