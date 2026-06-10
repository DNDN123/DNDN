# Host Python — Integrated Bridge

> 4팀(hyunsung / haneol / jinhwan / minju) 의 호스트 측 Python 코드를 통합한 디렉토리.
> 진입점은 단 하나 — **`nl_shell.py`** (hyunsung 작품).
> 다른 팀원의 호스트 코드는 `adapters/` 아래 모듈로 격리되어 있고, 필요할 때만 import 합니다.

---

## 파일 구조

```
host/
├── README.md               ← 이 파일
├── requirements.txt        ← Python 의존성 (Solar/오프라인 백엔드)
├── requirements-local.txt  ← 온디바이스 백엔드 추가 의존성 (torch/transformers/peft)
├── .env.example            ← API 키 / 경로 설정 템플릿
│
├── nlos.py                 ← ⭐ 라이브 진입점: 백엔드 선택 + 모델서버 + 브리지 한 방에
├── nlbridge.py             ← QEMU 콘솔 감싸 @@NL 포착 → 번역 → 가드 → 명령 자동 주입
├── model_server.py         ← 학습한 LoRA를 OpenAI 호환 /v1 로 서빙 (MPS/CUDA/CPU)
├── test_nlos.py            ← 오프라인 테스트 52종 (모델·QEMU 불필요)
│
├── nl_shell.py             ← 기존 진입점: 자연어 → xv6 명령 (수동 복붙 흐름)
├── executor.py             ← 인텐트 → xv6 명령 + SafetyGuard
├── agent.py                ← 추상요청("정리해줘") → 다단계 분해
├── prompts.py              ← Solar 프롬프트 정의
├── parse_trace.py          ← xv6 콘솔 로그 → JSON
├── llm_hint.py             ← batch: baseline 통계 → Solar → hints.txt
├── evaluator.py            ← turnaround / fairness 메트릭 계산
├── viz.py                  ← Gantt + 막대 차트 생성
├── hello_solar.py          ← Solar API 연결 sanity 체크
├── run_eval_matrix.sh      ← 3-way 평가 자동 실행 스크립트
│
└── adapters/               ← 다른 팀원 슬라이스 고유 Intent (선택적)
    ├── process_bridge.py   ← haneol — Process 슬라이스 NL 브리지 (sys_ps 등)
    └── thread_bridge.py    ← jinhwan — Thread 슬라이스 NL 브리지 (thread/futex)
```

---

## 어떤 걸 언제 쓰는가

**라이브 NL-OS 셸(권장)** — xv6 안에서 `ask <자연어>` → 자동 실행:

| 용도 | 명령 |
|---|---|
| 라이브 셸, 백엔드 자동 선택 | `python3 nlos.py` |
| 온디바이스 학습모델로 강제 | `python3 nlos.py --backend local` |
| Solar Pro 3 로 강제 | `python3 nlos.py --backend solar` |
| LLM 없이 규칙(키·모델 불필요) | `python3 nlos.py --backend offline` |
| 모델만 따로 서빙 | `python3 model_server.py` |
| 오프라인 테스트 52종 | `python3 test_nlos.py` |

**배치/평가 (기존 `nl_shell.py`)** — 도메인은 `--mode` 플래그로 지정:

| 용도 | 명령 |
|---|---|
| 자연어 → xv6 명령 한 줄 (Scheduler/MLFQ 데모, default) | `python nl_shell.py --once "..."` |
| `ps`, `setprio`, `kill` 등 프로세스 관리 자연어 | `python nl_shell.py --mode process --once "..."` |
| `threadtest`, futex 관련 자연어 | `python nl_shell.py --mode thread --once "..."` |
| 시스템 상태 진단 (diagprog 출력) | `python nl_shell.py --diagnose-from FILE` |
| syscall 추적 분석 (tracetool dump) | `python nl_shell.py --analyze-tracetool FILE` |
| baseline → hints.txt (배치 모드) | `python llm_hint.py baseline.json --output hints.txt` |
| 정량 평가 재실행 | `./run_eval_matrix.sh` |

`--mode process/thread` 는 내부적으로 `adapters/process_bridge.py` / `adapters/thread_bridge.py`
를 import해 호출합니다. 두 어댑터는 여전히 standalone (자체 `__main__`) 으로도 실행 가능하지만,
**정식 단일 진입점은 `nl_shell.py` 한 곳**.

---

## API 키 / 환경변수

- `.env` 파일을 만들고 `.env.example` 처럼 채우세요:
  ```
  UPSTAGE_API_KEY=up_xxx...
  UPSTAGE_MODEL=solar-pro3
  XV6_DIR=/path/to/os
  ```
- 모든 어댑터는 `UPSTAGE_API_KEY` (혹은 별칭 `SOLAR_API_KEY`) 를 *환경변수* 로 읽습니다.
- **하드코딩 금지** — jinhwan 의 원본 `solar_bridge.py` 에는 키가 박혀있었는데 통합 시 제거됨.

---

## 통합 시 변경된 점

| 모듈 | 변경 사항 |
|---|---|
| `nl_shell.py` | hyunsung 원본 그대로 — 통합 진입점 |
| `adapters/process_bridge.py` | haneol `nl_bridge.py` 그대로 복사 (이미 env-var 기반) |
| `adapters/thread_bridge.py` | jinhwan `solar_bridge.py` — **하드코딩된 API 키 제거**, `XV6_DIR` 환경변수화, API 키 없을 때 폴백 추가 |

---

## API 키 없이 동작 확인

세 모듈 모두 API 키 없으면 키워드/정적 폴백을 사용하도록 설계되어 있어서,
오프라인 데모도 가능합니다 (네트워크 장애 시연 안전망).
