# Smart-MLFQ-xv6 — 처음부터 빌드 가이드 (Panic 해결판)

> 이전 빌드에서 `panic: release` 가 발생한 원인은 **스케줄러 임계영역
> (p->lock 보유 중) 안에서 printf 를 호출** 한 것입니다. 새 패치 번들
> (`smart-mlfq-xv6-patches/`) 은 이 문제를 포함한 6개 이슈를 모두 고친
> 버전이에요. 이 가이드대로 처음부터 다시 빌드하시면 됩니다.

---

## 0. 사전 확인 (1분)

WSL 안에서 (`/mnt/c/` 가 아닌 진짜 Linux 파일시스템):

```bash
# 필수 도구 확인
which riscv64-unknown-elf-gcc   # /usr/bin/... 출력
which qemu-system-riscv64        # /usr/bin/... 출력
which make perl                  # 둘 다 출력
```

없으면:
```bash
sudo apt update
sudo apt install -y qemu-system-misc gcc-riscv64-unknown-elf gdb-multiarch
```

---

## Step 1 — 이전 빌드 완전 제거 (30초)

```bash
cd ~
rm -rf xv6-riscv          # 망가진 빌드 완전 삭제
```

---

## Step 2 — 강의용 xv6 깨끗하게 복사 (1분)

```bash
cp -r "/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/lecture-repo/xv6-riscv" ~/xv6-riscv
cd ~/xv6-riscv

# 확인: kernel/, user/, Makefile 가 보여야 함
ls
```

---

## Step 3 — 패치 적용 (10초)

새 패치 번들에 `apply_patches.sh` 자동 적용 스크립트가 있어요.

```bash
PATCH="/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/smart-mlfq-xv6-patches"

# 자동 적용 (백업 폴더도 자동 생성)
bash "$PATCH/apply_patches.sh" ~/xv6-riscv
```

스크립트가 끝에 `==> Done.` 출력하면 성공.

---

## Step 4 — 사용자 프로그램 + 워크로드 복사 (30초)

패치 스크립트는 kernel/, user/*.h, user/*.pl 만 복사해요. 우리가 추가한
**사용자 프로그램(C)** 과 **워크로드 파일(.txt)** 은 직접 복사:

```bash
PATCH="/mnt/c/Users/Hss/Documents/Claude/Projects/OS Project/smart-mlfq-xv6-patches"

# 사용자 프로그램
cp "$PATCH"/user/cpu_burner.c   ~/xv6-riscv/user/
cp "$PATCH"/user/io_burner.c    ~/xv6-riscv/user/
cp "$PATCH"/user/wrunner.c      ~/xv6-riscv/user/

# 워크로드 파일들 — xv6 fs.img에 굽기 위해 xv6 루트에 두기
cp "$PATCH"/workloads/cpu_heavy.txt   ~/xv6-riscv/
cp "$PATCH"/workloads/io_heavy.txt    ~/xv6-riscv/
cp "$PATCH"/workloads/mixed.txt       ~/xv6-riscv/

# 초기 hints 파일 (빈 파일 — baseline 실행용)
echo "# empty" > ~/xv6-riscv/hints.txt
```

---

## Step 5 — Makefile 수정 (1분)

`nano ~/xv6-riscv/Makefile` 또는 다른 편집기로 열고:

### 수정 ①: UPROGS 블록에 3줄 추가

`UPROGS=\` 블록을 찾아서 마지막 `$U/_forphan\` 다음에 추가:

```makefile
	$U/_forphan\
	$U/_cpu_burner\
	$U/_io_burner\
	$U/_wrunner\
```

### 수정 ②: fs.img 빌드 라인 수정

`fs.img: mkfs/mkfs README $(UPROGS)` 줄을 찾아서 워크로드 파일 추가:

```makefile
# 기존:
fs.img: mkfs/mkfs README $(UPROGS)
	mkfs/mkfs fs.img README $(UPROGS)

# 변경 후:
fs.img: mkfs/mkfs README $(UPROGS) cpu_heavy.txt io_heavy.txt mixed.txt hints.txt
	mkfs/mkfs fs.img README $(UPROGS) cpu_heavy.txt io_heavy.txt mixed.txt hints.txt
```

저장: `Ctrl-O`, `Enter`, `Ctrl-X` (nano 기준)

---

## Step 6 — 빌드 (1~2분)

```bash
cd ~/xv6-riscv
make clean
make qemu
```

### 기대하는 화면

```
xv6 kernel is booting
hart 1 starting
hart 2 starting
init: starting sh
$
```

`$` 프롬프트가 뜨면 **부팅 성공**입니다.

> 만약 panic 이 또 나면 → §"문제 해결" 참고.

---

## Step 7 — 첫 동작 확인 (30초)

xv6 콘솔에서:

```
$ ls
# cpu_burner, io_burner, wrunner, cpu_heavy.txt, io_heavy.txt, mixed.txt 보여야 함

$ cpu_burner 100
# "cpu_burner pid=X done acc=Y" 출력

$ wrunner cpu_heavy.txt
# 출력 예:
# wrunner: loaded 5 items, 0 hints
# wrunner: spawn cpu_burner pid=4 (default priority)
# TRACE tick=12 pid=4 pri=0 slice=0
# ...
# cpu_burner pid=4 done acc=...
# EXIT pid=4 name=cpu_burner arrival=10 completion=152 run=42 ioblock=0 final_pri=2
# ...
# ALL_DONE wrunner pid=3
$ 
```

**TRACE 라인이 흘러나오고, 마지막에 EXIT 가 5개 찍히고, ALL_DONE이 보이면 성공입니다.**

QEMU 종료: `Ctrl-A` 누르고 손 떼고 `X`.

---

## Step 8 — LLM 통합 실험 (나중에)

호스트 Python (Solar API 통합)은 별도 step. 위 7단계가 잘 되면 그 다음
단계로 진행하시면 됩니다.

---

## 문제 해결

### 빌드 에러: `undefined reference to settrace` / `forkpri`

`user/user.h`, `user/usys.pl` 가 새 버전인지 확인:
```bash
grep -E "settrace|forkpri" ~/xv6-riscv/user/usys.pl
# 둘 다 나와야 함
```
없으면 Step 3 의 `apply_patches.sh` 다시 실행.

### 빌드 에러: `mkfs: Assertion DIRSIZ failed`

xv6는 파일명 14자 제한. `_workload_runner` (16자) 가 남아있는지 확인:
```bash
grep "_workload_runner" ~/xv6-riscv/Makefile
# 출력되면 → _wrunner 로 변경
sed -i 's/_workload_runner/_wrunner/g' ~/xv6-riscv/Makefile
```

### 부팅 후 panic: release (이전과 같음)

만약 또 발생하면 `kernel/proc.c` 가 새 버전인지 확인:
```bash
grep "boost_lock" ~/xv6-riscv/kernel/proc.c
# 출력되면 새 버전 ✓
# 안 나오면 → apply_patches.sh 다시 실행
```

### 부팅 직후 멈춤 / 검은 화면

QEMU 자체 문제일 수 있어요. 종료 후 재시도:
```
Ctrl-A  X      (QEMU 종료)
make clean
make qemu
```

### `make qemu` 가 너무 느림

`/mnt/c/` 에서 빌드하면 매우 느립니다. **반드시 `~/xv6-riscv` (WSL 홈)
에서 빌드**하세요. ext4 위에서 약 30초~1분, NTFS 위에서는 10분+.

### Workload 파일을 못 찾음 (cannot open cpu_heavy.txt)

Step 4·5 누락. fs.img 빌드 시 워크로드 파일이 포함되지 않았어요:
```bash
ls ~/xv6-riscv/*.txt   # 워크로드 .txt 들이 보여야 함
grep "cpu_heavy.txt" ~/xv6-riscv/Makefile  # fs.img 라인에 나와야 함
```

---

## Step별 체크리스트

진행하면서 하나씩 ✓ 표시:

- [ ] Step 0: gcc-riscv64, qemu-system-riscv64 설치됨
- [ ] Step 1: 이전 `~/xv6-riscv` 삭제 완료
- [ ] Step 2: lecture-repo xv6 가 `~/xv6-riscv` 에 복사됨
- [ ] Step 3: `apply_patches.sh` 가 `==> Done.` 으로 끝남
- [ ] Step 4: `~/xv6-riscv/user/wrunner.c` 존재
- [ ] Step 4: `~/xv6-riscv/cpu_heavy.txt` 등 존재
- [ ] Step 5: Makefile UPROGS 에 `_cpu_burner`, `_io_burner`, `_wrunner` 있음
- [ ] Step 5: Makefile fs.img 라인에 워크로드 .txt 추가됨
- [ ] Step 6: `make qemu` 가 `$` 프롬프트까지 도달
- [ ] Step 7: `wrunner cpu_heavy.txt` 가 TRACE/EXIT 출력 + ALL_DONE

모든 항목 ✓ 면 **Smart-MLFQ-xv6 첫 실행 성공**입니다.

---

## 핵심 변화 (이전 빌드 vs 이번 빌드)

| 항목 | 이전 빌드 | 이번 빌드 |
|---|---|---|
| scheduler 안 TRACE | p->lock 잡고 printf ⚠️ | swtch 후 락 해제 후 printf ✓ |
| boost 동기화 | 없음 (멀티 CPU race) | `boost_lock` 으로 직렬화 ✓ |
| 기본 trace | 모든 프로세스 on (init 까지 노이즈) | 기본 off, `settrace()` 로 명시 on ✓ |
| fork → setpri | race window 존재 | `forkpri()` 원자적 ✓ |
| 우선순위 적용 | 명시적 호출 필요 | `forkpri(level)` 한 번에 ✓ |

이 5가지가 결합되어 panic 없이 안정 동작합니다.

---

## 다음 단계 (Step 7 성공 후)

1. **다른 워크로드 실험**: `wrunner io_heavy.txt`, `wrunner mixed.txt`
2. **Hints 실험**: `wrunner mixed.txt hints.txt` (hints.txt 채워서)
3. **호스트 Python 통합**: Solar API → 자동 hints 생성 → 비교 실험
4. **결과 차트**: matplotlib으로 baseline vs LLM-guided 비교

호스트 Python 통합은 별도 가이드로 진행할게요.

---

**진행 중 막히면 정확한 에러 메시지와 어느 Step에서 막혔는지 알려주세요.**
