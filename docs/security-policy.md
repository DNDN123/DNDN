# Security Policy

본 저장소는 공개 GitHub 저장소이므로 **푸시되는 모든 파일이 인터넷에 노출**된다는
전제 위에서 작업합니다. 본 문서는 그 전제에서 비밀(특히 Upstage Solar API 키)을
어떻게 다룰지 정합니다.

## 사고 이력

| 일자 | 위치 | 내용 | 상태 |
|---|---|---|---|
| ~W11 | `jinhwan` 브랜치 `solar_bridge.py` L5 | Upstage 키 평문 하드코딩 후 push | ✅ 해결 (2026-05-27) |
| 2026-05-21 | `hyunsung` 브랜치 `smart-mlfq-host/.env.example` L5 | 실제 키 `up_lq0WSlcP...` 가 placeholder처럼 들어가 push | ✅ 해결 (2026-05-27) |

### 2026-05-27 해결 조치 요약

1. **revoke**: 노출된 키 `up_lq0WSlcP...` 를 Upstage 콘솔에서 삭제 — 해당 키로 401 반환 확인
2. **재발급**: 새 키 발급, `/root/smart-mlfq-host/.env` + `integration/host/.env` 두 곳에만 저장 (둘 다 `.gitignore` 적용)
3. **forward leak 차단**: `smart-mlfq-host/.env.example` 의 평문 키를 `your_api_key_here` placeholder 로 대체 후 commit (`89656c9 security: sanitize .env.example placeholder`)
4. **검증**: `hello_solar.py` 양쪽 .env 위치에서 모두 `response: 'ok'` 확인
5. **(미수행)** git 히스토리 재작성 — 키 자체는 무효화됐으므로 필수 아님. 추가 정화가 필요하면 `git filter-repo --replace-text` 절차 별도 적용.

이전 commit들의 히스토리에는 여전히 옛 키 문자열이 남아있지만 Upstage에서 revoke됐으므로 실질 위험은 0. 단, 외부 자동 스캐너(GitHub secret scanning 등)가 옛 키를 발견하고 알림을 띄울 수 있으니 그때 §해결 조치를 인용해 응답.

## 키 보관 규칙

### `.env` 파일에만 실제 키
- 위치: `smart-mlfq-host/.env`
- `.gitignore`의 `.env` 패턴으로 절대 commit되지 않음
- 형식:
  ```
  UPSTAGE_API_KEY=up_<실제 키>
  UPSTAGE_BASE_URL=https://api.upstage.ai/v1
  UPSTAGE_MODEL=solar-pro3
  ```

### `.env.example`은 placeholder만
- `.gitignore`의 `!.env.example` 예외로 commit됨 (다른 팀원이 환경 셋업 시 참고)
- `UPSTAGE_API_KEY` 값은 **반드시 `your_api_key_here`** (실제 키 형태 `up_...` 금지)
- 호스트 코드(`nl_shell.py`, `hello_solar.py`, `llm_hint.py`)는 키가 비어있거나
  `your_api_key_here` 인 경우를 자동으로 폴백 처리

### 소스코드에 키 하드코딩 금지
- 어떤 Python·C 파일에도 `up_...` 패턴을 적지 말 것
- 키가 필요하면 `os.getenv("UPSTAGE_API_KEY")` 로만 읽음

## 사고 발생 시 대응 절차

1. **즉시 키 revoke** — Upstage 콘솔에서 노출된 키 비활성화
2. **새 키 발급** — 본인 `.env`에만 저장
3. **소스에서 평문 키 제거** — placeholder로 교체 후 commit
4. **히스토리 정리(선택)** — `git filter-repo --replace-text` 로 과거 commit에서도
   키 문자열을 제거. 단 다른 팀원과 force-push 합의 필요
5. **사고 이력 표 갱신** — 본 문서 §"사고 이력" 표에 추가

## Pre-commit 자가 점검

본인이 commit 전에 한 번씩 확인:
```bash
# 스테이지된 변경에 키 패턴이 들어있는지 검사
git diff --cached | grep -nE 'up_[A-Za-z0-9]{20,}' && echo "STOP: API key in stage"
```

이 한 줄을 `~/.bashrc` 의 alias 또는 git pre-commit hook 으로 등록하면 자동화.

## .gitignore 표준 (4팀 공통)

본 저장소의 `.gitignore` 는 다음을 포함하고 있어야 함 — 다른 슬라이스 브랜치도
동일하게 적용:
```
.env
.env.local
*.env
!.env.example

.DS_Store
__pycache__/
*.py[cod]

*.o
*.d
*.asm
*.sym
*.bak
```

`minju` 브랜치는 현재 `.gitignore` 가 없어 `.bak`/`.DS_Store` 가 commit돼 있고,
`jinhwan` 브랜치는 `.o`/빌드 산출물이 commit돼 있음. 본 표준을 받아 적용 권장.

## 책임 분담

- 본 정책의 시행 책임은 **각 슬라이스 담당자**.
- 키 노출 사고를 발견한 사람은 즉시 팀 채널에 공유 + 해당 담당자에게 revoke 요청.
- 신규 외부 비밀(예: 다른 API 키, DB 패스워드)을 도입하려면 본 문서를 먼저 갱신.
