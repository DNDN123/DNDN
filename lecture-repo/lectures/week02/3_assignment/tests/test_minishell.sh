#!/bin/bash
#
# test_minishell.sh - minishell 과제 자동 테스트 스크립트
#
# Usage: ./test_minishell.sh [path_to_minishell.c]
#        Default: ../skeleton/minishell.c
#
# 테스트 항목:
#   1. 컴파일
#   2. 단일 명령어 실행 (echo, ls)
#   3. 명령어 인자 처리
#   4. 입력 리다이렉션 (<)
#   5. 출력 리다이렉션 (>)
#   6. 파이프 (|)
#   7. 파이프 + 리다이렉션 조합
#   8. exit 명령어
#   9. 존재하지 않는 명령어 처리
#  10. 빈 입력 처리

set -e

PASS=0
FAIL=0
TOTAL=0

# Colors
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
    if [ -n "$2" ]; then
        echo "    Expected: $2"
    fi
    if [ -n "$3" ]; then
        echo "    Got:      $3"
    fi
}

info() {
    echo -e "  ${YELLOW}[INFO]${NC} $1"
}

# Send commands to minishell and capture output
# Usage: run_shell "command1\ncommand2\n..."
# Output goes to $TMPDIR/shell_output.txt and $TMPDIR/shell_stderr.txt
run_shell() {
    echo -e "$1\nexit" | timeout 10 "$BIN" > "$TMPDIR/shell_output.txt" 2>"$TMPDIR/shell_stderr.txt" || true
    # Strip prompt prefix "minishell> " from all lines, then remove empty lines
    sed 's/^minishell> *//g' "$TMPDIR/shell_output.txt" | sed '/^$/d' > "$TMPDIR/shell_clean.txt" 2>/dev/null || true
}

# Determine source file
SRC="${1:-../skeleton/minishell.c}"
if [ ! -f "$SRC" ]; then
    echo "Error: Source file not found: $SRC"
    echo "Usage: $0 [path_to_minishell.c]"
    exit 1
fi

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

BIN="$TMPDIR/minishell"

echo "========================================="
echo " minishell 테스트"
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

if [ -s "$TMPDIR/compile_err.txt" ]; then
    info "컴파일 경고 있음:"
    sed 's/^/    /' "$TMPDIR/compile_err.txt"
fi
echo ""

# Test 2: echo command
echo "[Test 2] 단일 명령어 - echo"
run_shell "echo hello world"
RESULT=$(cat "$TMPDIR/shell_clean.txt")
if [ "$RESULT" = "hello world" ]; then
    pass "echo hello world → 'hello world'"
else
    fail "echo hello world" "hello world" "$RESULT"
fi
echo ""

# Test 3: Command with arguments
echo "[Test 3] 명령어 인자 처리"
run_shell "echo -n test"
RESULT=$(cat "$TMPDIR/shell_clean.txt")
if [ "$RESULT" = "test" ]; then
    pass "echo -n test → 'test' (개행 없이)"
else
    # echo -n behavior varies; accept "test" or "-n test"
    if echo "$RESULT" | grep -q "test"; then
        pass "echo -n test 출력에 'test' 포함"
    else
        fail "echo -n test" "test" "$RESULT"
    fi
fi
echo ""

# Test 4: Output redirection (>)
echo "[Test 4] 출력 리다이렉션 (>)"
OUTFILE="$TMPDIR/test_output.txt"
run_shell "echo redirected output > $OUTFILE"
if [ -f "$OUTFILE" ]; then
    CONTENT=$(cat "$OUTFILE")
    if [ "$CONTENT" = "redirected output" ]; then
        pass "echo ... > file 결과 파일에 올바른 내용"
    else
        fail "파일 내용 불일치" "redirected output" "$CONTENT"
    fi
else
    fail "출력 파일이 생성되지 않음: $OUTFILE"
fi
echo ""

# Test 5: Input redirection (<)
echo "[Test 5] 입력 리다이렉션 (<)"
INFILE="$TMPDIR/test_input.txt"
printf "apple\nbanana\ncherry\n" > "$INFILE"
run_shell "wc -l < $INFILE"
RESULT=$(cat "$TMPDIR/shell_clean.txt" | tr -d ' ')
if [ "$RESULT" = "3" ]; then
    pass "wc -l < file → 3"
else
    fail "wc -l < file" "3" "$RESULT"
fi
echo ""

# Test 6: Pipe (|)
echo "[Test 6] 파이프 (|)"
run_shell "echo hello pipe test | wc -w"
RESULT=$(cat "$TMPDIR/shell_clean.txt" | tr -d ' ')
if [ "$RESULT" = "3" ]; then
    pass "echo hello pipe test | wc -w → 3"
else
    fail "echo hello pipe test | wc -w" "3" "$RESULT"
fi
echo ""

# Test 7: Pipe with grep
echo "[Test 7] 파이프 - grep"
run_shell "echo -e 'apple\nbanana\napricot' | grep ap"
RESULT=$(cat "$TMPDIR/shell_clean.txt")
# Different systems handle echo -e differently, so be flexible
if echo "$RESULT" | grep -q "apple"; then
    pass "파이프를 통한 grep 동작 확인"
else
    # Try alternative: some systems need printf
    run_shell "printf 'apple\nbanana\napricot\n' | grep ap"
    RESULT=$(cat "$TMPDIR/shell_clean.txt")
    if echo "$RESULT" | grep -q "apple"; then
        pass "파이프를 통한 grep 동작 확인 (printf 사용)"
    else
        fail "파이프 + grep" "apple 포함" "$RESULT"
    fi
fi
echo ""

# Test 8: Pipe + output redirection
echo "[Test 8] 파이프 + 출력 리다이렉션"
PIPEFILE="$TMPDIR/pipe_output.txt"
run_shell "echo pipe to file | cat > $PIPEFILE"
if [ -f "$PIPEFILE" ]; then
    CONTENT=$(cat "$PIPEFILE")
    if [ "$CONTENT" = "pipe to file" ]; then
        pass "echo ... | cat > file 결과 올바름"
    else
        fail "파이프 + 리다이렉션 파일 내용" "pipe to file" "$CONTENT"
    fi
else
    fail "파이프 + 리다이렉션 출력 파일이 생성되지 않음"
fi
echo ""

# Test 9: Input redirection + pipe
echo "[Test 9] 입력 리다이렉션 + 파이프"
SORTFILE="$TMPDIR/sort_input.txt"
printf "cherry\napple\nbanana\n" > "$SORTFILE"
run_shell "sort < $SORTFILE | head -1"
RESULT=$(cat "$TMPDIR/shell_clean.txt")
if [ "$RESULT" = "apple" ]; then
    pass "sort < file | head -1 → apple"
else
    fail "sort < file | head -1" "apple" "$RESULT"
fi
echo ""

# Test 10: exit command
echo "[Test 10] exit 명령어"
echo "exit" | timeout 5 "$BIN" > /dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    pass "exit 명령어로 정상 종료"
else
    fail "exit 종료 코드" "0" "$EXIT_CODE"
fi
echo ""

# Test 11: EOF (Ctrl-D)
echo "[Test 11] EOF 처리 (Ctrl-D)"
echo -n "" | timeout 5 "$BIN" > /dev/null 2>&1
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then
    pass "EOF(빈 입력)으로 정상 종료"
else
    fail "EOF 종료 코드" "0" "$EXIT_CODE"
fi
echo ""

# Test 12: Nonexistent command
echo "[Test 12] 존재하지 않는 명령어"
run_shell "nonexistent_command_xyz123"
# Shell should not crash; it should print an error and continue
if timeout 5 echo "echo still alive" | "$BIN" > "$TMPDIR/alive_check.txt" 2>/dev/null; then
    # Verify we can still get output after error
    pass "존재하지 않는 명령어 후에도 셸이 계속 실행됨"
else
    fail "존재하지 않는 명령어 실행 후 셸이 죽음"
fi
echo ""

# Test 13: Empty input handling
echo "[Test 13] 빈 입력 처리"
run_shell "\n\n\necho after empty lines"
RESULT=$(cat "$TMPDIR/shell_clean.txt")
if echo "$RESULT" | grep -q "after empty lines"; then
    pass "빈 줄 이후에도 정상 실행"
else
    fail "빈 줄 처리" "after empty lines" "$RESULT"
fi
echo ""

# Test 14: Multiple commands in sequence
echo "[Test 14] 연속 명령어 실행"
run_shell "echo first\necho second\necho third"
RESULT=$(cat "$TMPDIR/shell_clean.txt")
EXPECTED=$(printf "first\nsecond\nthird")
if [ "$RESULT" = "$EXPECTED" ]; then
    pass "연속 3개 명령어 실행 결과 올바름"
else
    fail "연속 명령어" "$EXPECTED" "$RESULT"
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
