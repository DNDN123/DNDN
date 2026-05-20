# 데모 녹화 가이드 (1~2분)

QuickTime Player로 화면 녹화하는 시연 스크립트. 그대로 따라 치면 됨.

---

## 사전 준비 (녹화 시작 전)

1. **터미널 한 칸 띄워놓기**
   ```bash
   pkill -f qemu-system-riscv64    # 이전 QEMU 정리
   cd "/Users/jeonhaneol/Downloads/LLM for OS/xv6-riscv"
   make                             # 미리 빌드해서 녹화 시간 절약
   ```

2. **호스트 디렉토리로 이동**
   ```bash
   cd "/Users/jeonhaneol/Downloads/LLM for OS/host"
   ```

3. **터미널 폰트 크게** (16~18pt 권장 — 영상에서 글자 잘 보이게)

4. **QuickTime → New Screen Recording → 터미널 창만 선택**

---

## 녹화 시작 — 따라 치기 (예상 1분 40초)

### 0:00 — 시작
```bash
python3 nl_bridge.py
```
(부팅 메시지 약 4초 → `smart>` 프롬프트 나옴)

### 0:08 — 한국어 PS
```
프로세스 보여줘
```
**기대 출력**:
```
3 active process(es):
  pid=  1 sleep   prio=10 name=init
  pid=  2 sleep   prio=10 name=sh
  pid=  3 run     prio=10 name=ps
```

### 0:20 — 영어 PS (같은 의도 다르게)
```
show me the running processes
```
(다시 PS 결과)

### 0:32 — SETPRIO 시연
```
set pid 2 priority to 3
```
**기대 출력**:
```
  -> intent: SETPRIO {'pid': 2, 'prio': 3}
  -> xv6  : setprio 2 3
OK pid=2 prio=3
```

### 0:45 — 변경 확인
```
프로세스 다시 보여줘
```
(pid 2의 priority가 변경된 거 보임)

### 0:58 — MLFQ 벤치마크 시연
```
launch mlfq_bench
```
(약 20초 걸림 — 결과:
```
BENCH name=HIGH prio=1 ticks=4
BENCH name=NORMAL prio=10 ticks=8
BENCH name=LOW prio=19 ticks=11
```
HIGH/NORMAL/LOW 비율 확실히 보임)

### 1:25 — 안전 가드 #1
```
kill 1
```
**기대 출력**:
```
⚠️  rejected: refusing to kill pid 1 (init)
```

### 1:32 — 안전 가드 #2
```
rm -rf the system
```
**기대 출력**:
```
⚠️  rejected: program not in whitelist: rm
```
(또는 Solar가 REJECT intent로 분류)

### 1:38 — 종료
```
Ctrl-D
```

---

## 녹화 후처리

1. QuickTime → File → Save As → `demo.mov`

2. **GIF로 변환** (README에 임베드용):
   ```bash
   # macOS 기본 도구로는 안 됨, ffmpeg 설치 필요
   brew install ffmpeg
   ffmpeg -i demo.mov -vf "fps=10,scale=720:-1:flags=lanczos" -loop 0 demo.gif
   ```

3. **README에 임베드**:
   ```markdown
   ## Demo
   ![SmartShell demo](demo.gif)
   ```

   파일이 너무 크면 (GitHub 25MB 한도) ffmpeg로 fps/scale 더 줄이거나
   YouTube에 올리고 링크.

---

## 시연 중 망쳤을 때 빠른 복구

| 증상 | 복구 |
|---|---|
| QEMU가 한 명령 후 멈춤 | Ctrl-C → `pkill -f qemu` → 다시 `python3 nl_bridge.py` |
| Solar API가 502/429 반환 | `unset UPSTAGE_API_KEY` → 오프라인 파서로 폴백 (한국어 일부만 됨) |
| `smart>` 프롬프트가 안 나옴 | 4~6초 더 기다림. 그래도 안 나오면 부팅 실패 — 빌드 다시 |
| 화면이 너무 빠르게 지나감 | 녹화 후 QuickTime으로 구간 trim |

---

## 발표 시 다른 옵션 (영상 대신)

영상 녹화 실패해도 발표는 가능:
- **라이브 데모** — 발표 자리에서 직접 위 명령들 실행 (단점: 네트워크/API 의존)
- **스크린샷 6장** — 각 단계 캡처해서 슬라이드 4번에 차례로 보여주기
- **로그 텍스트** — 시연 결과 텍스트만 슬라이드에 박아두기

GIF는 가장 편한 옵션이지만 백업 항상 준비.
