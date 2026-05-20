#!/bin/bash
# =============================================================
# test_kalloc.sh
# xv6에서 per-CPU kalloc 과제를 테스트하는 가이드
#
# 이 스크립트는 xv6 내부에서 직접 실행하는 것이 아니라,
# 테스트 절차를 안내하는 스크립트입니다.
# =============================================================

set -e

echo "============================================"
echo " xv6 Per-CPU kalloc 테스트 가이드"
echo "============================================"
echo ""

# xv6 소스 디렉터리 경로 (환경에 맞게 수정)
XV6_DIR="${XV6_DIR:-../../xv6-riscv}"

echo "[1단계] 파일 교체"
echo "  수정한 kalloc_percpu.c를 xv6 소스에 복사합니다:"
echo ""
echo "    cp skeleton/kalloc_percpu.c ${XV6_DIR}/kernel/kalloc.c"
echo ""

echo "[2단계] 빌드"
echo "  xv6를 빌드합니다:"
echo ""
echo "    cd ${XV6_DIR}"
echo "    make clean"
echo "    make qemu"
echo ""

echo "[3단계] 테스트 실행"
echo "  xv6 셸에서 다음 명령을 실행합니다:"
echo ""
echo "  (a) usertests - 기본 시스템 테스트:"
echo "      $ usertests"
echo "      모든 테스트가 PASS되어야 합니다."
echo ""
echo "  (b) forktest - fork를 통한 메모리 할당 스트레스 테스트:"
echo "      $ forktest"
echo "      fork: OK 가 출력되어야 합니다."
echo ""

echo "[4단계] 확인사항"
echo "  - 빌드 시 에러/경고가 없어야 합니다."
echo "  - usertests가 모든 테스트를 통과해야 합니다."
echo "  - 커널 패닉이 발생하지 않아야 합니다."
echo ""

echo "[참고] QEMU 종료 방법:"
echo "  Ctrl-a 를 누른 뒤 x 를 누릅니다."
echo ""

echo "============================================"
echo " 선택 테스트: per-CPU 효과 확인"
echo "============================================"
echo ""
echo "per-CPU free list가 올바르게 동작하는지 확인하려면:"
echo ""
echo "  1. kinit()에서 각 CPU의 freelist 크기를 출력하는 디버그 코드를 추가"
echo "  2. kalloc()에서 steal이 발생할 때 로그를 출력"
echo "  3. 예시 디버그 코드:"
echo ""
echo '     // kalloc() 내부, steal 성공 시:'
echo '     printf("CPU %d: stole page from CPU %d\n", id, i);'
echo ""
echo "============================================"
