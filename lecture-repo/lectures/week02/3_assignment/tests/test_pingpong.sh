#!/bin/bash
#
# test_pingpong.sh - pingpong 과제 자동 테스트 스크립트
#
# Usage: ./test_pingpong.sh [path_to_pingpong.c]
#        Default: ../skeleton/pingpong.c
#
# 테스트 항목:
#   1. 컴파일 성공 여부
#   2. 실행 시 정상 종료 여부
#   3. 출력 형식 검증 ("received ping", "received pong")
#   4. PID 형식 확인
#   5. Round-trip time 출력 확인
#   6. 반복 실행 안정성

set -e

PASS=0
FAIL=0
TOTAL=0

# Colors (disable if not a terminal)
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[0;33m'
    NC='\033[0m'
else
    GREEN=''
    RED=''
    YELLOW=''
    NC=''
fi

pass() {
    PASS=$((PASS + 1))
    TOTAL=$((TOTAL + 1))
    echo -e "  ${GREEN}[PASS]${NC} $1"
}

fail() {
    FAIL=$((FAIL + 1))
    TOTAL=$((TOTAL + 1))
    echo -e "  ${RED}[FAIL]${NC} $1"
}

info() {
    echo -e "  ${YELLOW}[INFO]${NC} $1"
}

# Determine source file
SRC="${1:-../skeleton/pingpong.c}"
if [ ! -f "$SRC" ]; then
    echo "Error: Source file not found: $SRC"
    echo "Usage: $0 [path_to_pingpong.c]"
    exit 1
fi

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

BIN="$TMPDIR/pingpong"

echo "========================================="
echo " pingpong 테스트"
echo "========================================="
echo ""
echo "Source: $SRC"
echo ""

# Test 1: Compilation
echo "[Test 1] 컴파일"
if gcc -Wall -o "$BIN" "$SRC" 2>"$TMPDIR/compile_err.txt"; then
    pass "gcc -Wall 컴파일 성공"
else
    fail "컴파일 실패"
    echo "    Compiler output:"
    sed 's/^/    /' "$TMPDIR/compile_err.txt"
    echo ""
    echo "========================================="
    echo " 결과: 컴파일 실패로 나머지 테스트를 건너뜁니다."
    echo "========================================="
    exit 1
fi

# Check for warnings
if [ -s "$TMPDIR/compile_err.txt" ]; then
    info "컴파일 경고 있음:"
    sed 's/^/    /' "$TMPDIR/compile_err.txt"
fi
echo ""

# Test 2: Execution (should exit 0 within 5 seconds)
echo "[Test 2] 실행 및 종료"
if timeout 5 "$BIN" > "$TMPDIR/output.txt" 2>"$TMPDIR/stderr.txt"; then
    pass "정상 종료 (exit code 0)"
else
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        fail "시간 초과 (5초 이상 실행됨 - 무한 대기 가능성)"
    else
        fail "비정상 종료 (exit code $EXIT_CODE)"
    fi
    if [ -s "$TMPDIR/stderr.txt" ]; then
        echo "    stderr:"
        sed 's/^/    /' "$TMPDIR/stderr.txt"
    fi
fi
echo ""

# Test 3: Output format - "received ping"
echo "[Test 3] 출력 형식 - ping"
if grep -q "received ping" "$TMPDIR/output.txt" 2>/dev/null; then
    pass "'received ping' 메시지 있음"
else
    fail "'received ping' 메시지 없음"
    echo "    실제 출력:"
    sed 's/^/    /' "$TMPDIR/output.txt"
fi

# Check PID format for ping line
if grep -qE '^[0-9]+: received ping$' "$TMPDIR/output.txt" 2>/dev/null; then
    pass "ping 행 형식 올바름 (<pid>: received ping)"
else
    fail "ping 행 형식이 '<pid>: received ping'이 아님"
    PING_LINE=$(grep "received ping" "$TMPDIR/output.txt" 2>/dev/null || echo "(없음)")
    echo "    실제: $PING_LINE"
fi
echo ""

# Test 4: Output format - "received pong"
echo "[Test 4] 출력 형식 - pong"
if grep -q "received pong" "$TMPDIR/output.txt" 2>/dev/null; then
    pass "'received pong' 메시지 있음"
else
    fail "'received pong' 메시지 없음"
fi

if grep -qE '^[0-9]+: received pong$' "$TMPDIR/output.txt" 2>/dev/null; then
    pass "pong 행 형식 올바름 (<pid>: received pong)"
else
    fail "pong 행 형식이 '<pid>: received pong'이 아님"
    PONG_LINE=$(grep "received pong" "$TMPDIR/output.txt" 2>/dev/null || echo "(없음)")
    echo "    실제: $PONG_LINE"
fi
echo ""

# Test 5: PID values are different for ping and pong
echo "[Test 5] PID 검증"
PING_PID=$(grep "received ping" "$TMPDIR/output.txt" 2>/dev/null | grep -oE '^[0-9]+' || echo "")
PONG_PID=$(grep "received pong" "$TMPDIR/output.txt" 2>/dev/null | grep -oE '^[0-9]+' || echo "")

if [ -n "$PING_PID" ] && [ -n "$PONG_PID" ] && [ "$PING_PID" != "$PONG_PID" ]; then
    pass "ping PID($PING_PID) != pong PID($PONG_PID) (서로 다른 프로세스)"
else
    if [ "$PING_PID" = "$PONG_PID" ] && [ -n "$PING_PID" ]; then
        fail "ping PID와 pong PID가 동일함 ($PING_PID) - 서로 다른 프로세스여야 합니다"
    else
        fail "PID 추출 실패"
    fi
fi
echo ""

# Test 6: Round-trip time output
echo "[Test 6] Round-trip time 출력"
if grep -qE 'Round-trip time:.*us' "$TMPDIR/output.txt" 2>/dev/null; then
    pass "Round-trip time 출력 있음"
    RTT=$(grep "Round-trip time" "$TMPDIR/output.txt")
    info "$RTT"
else
    fail "Round-trip time 출력 없음"
fi
echo ""

# Test 7: Output order (ping should come before pong)
echo "[Test 7] 출력 순서"
PING_LINE_NUM=$(grep -n "received ping" "$TMPDIR/output.txt" 2>/dev/null | head -1 | cut -d: -f1 || echo "0")
PONG_LINE_NUM=$(grep -n "received pong" "$TMPDIR/output.txt" 2>/dev/null | head -1 | cut -d: -f1 || echo "0")

if [ "$PING_LINE_NUM" -gt 0 ] 2>/dev/null && [ "$PONG_LINE_NUM" -gt 0 ] 2>/dev/null; then
    if [ "$PING_LINE_NUM" -lt "$PONG_LINE_NUM" ]; then
        pass "ping이 pong보다 먼저 출력됨"
    else
        fail "pong이 ping보다 먼저 출력됨 (ping이 먼저여야 합니다)"
    fi
else
    fail "출력 순서 확인 불가"
fi
echo ""

# Test 8: Repeated execution stability
echo "[Test 8] 반복 실행 안정성 (5회)"
STABLE=true
for i in $(seq 1 5); do
    if ! timeout 5 "$BIN" > /dev/null 2>&1; then
        STABLE=false
        fail "반복 실행 $i/5 실패"
        break
    fi
done
if $STABLE; then
    pass "5회 반복 실행 모두 정상"
fi
echo ""

# Summary
echo "========================================="
echo " 결과 요약"
echo "========================================="
echo -e " 통과: ${GREEN}${PASS}${NC} / ${TOTAL}"
echo -e " 실패: ${RED}${FAIL}${NC} / ${TOTAL}"
echo ""

if [ $FAIL -eq 0 ]; then
    echo -e " ${GREEN}모든 테스트 통과!${NC}"
    exit 0
else
    echo -e " ${RED}일부 테스트 실패${NC}"
    exit 1
fi
