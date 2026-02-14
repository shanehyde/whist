#!/bin/bash
#
# Test runner for w0 compiler
#

# Colors
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
CYAN='\033[1;36m'
GRAY='\033[90m'
BOLD='\033[1m'
RESET='\033[0m'

# Counters
run_passed=0
run_failed=0
run_skipped=0
error_passed=0
error_failed=0

# Options
run_valid=false
run_errors=false
verbose=false

# Find w0 binary and lib path
W0="${W0:-bin/w0}"
LIB_PATH="${LIB_PATH:-../lib}"

usage() {
    echo -e "${BLUE}W0 Test Suite${RESET}"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --run       Run only executable program tests (test/run/**)"
    echo "  --valid     Alias for --run"
    echo "  --errors    Run only error case tests (test/errors/**)"
    echo "  --verbose   Show detailed output"
    echo "  --help      Show this help"
    echo ""
    echo "With no options, runs all tests."
}

file_has_main() {
    local file="$1"
    grep -Eq '^[[:space:]]*func[[:space:]]+main[[:space:]]*\(' "$file"
}

file_has_test_blocks() {
    local file="$1"
    grep -Eq '^[[:space:]]*test[[:space:]]+"' "$file"
}

print_test_header() {
    local file="$1"
    local display
    display=$(echo "$file" | sed 's#^test/##')
    if $verbose; then
        echo -e "${YELLOW}--- Testing $display ---${RESET}"
    else
        printf "${BOLD}%-45s${RESET}" "${display}:"
    fi
}

read_expected_lines() {
    local prefix="$1"
    local file="$2"
    sed -n "s#^// ${prefix}: *##p" "$file"
}

check_expected_lines() {
    local label="$1"
    local expected_lines="$2"
    local actual_file="$3"

    if [ -z "$expected_lines" ]; then
        return 0
    fi

    while IFS= read -r expected; do
        [ -z "$expected" ] && continue
        if ! grep -Fq -- "$expected" "$actual_file"; then
            return 1
        fi
    done <<< "$expected_lines"

    return 0
}

check_rc_free_order() {
    local expected_free_order="$1"
    local stderr_file="$2"

    [ -z "$expected_free_order" ] && return 0

    local left_idx right_idx
    left_idx=$(echo "$expected_free_order" | awk -F" before " '{print $1}' | tr -d ' ')
    right_idx=$(echo "$expected_free_order" | awk -F" before " '{print $2}' | tr -d ' ')

    local alloc_ptrs left_ptr right_ptr
    alloc_ptrs=$(awk '/RC_ALLOC:/ {print $2}' "$stderr_file")
    left_ptr=$(echo "$alloc_ptrs" | awk -v idx="$left_idx" 'NR==idx {print $1}')
    right_ptr=$(echo "$alloc_ptrs" | awk -v idx="$right_idx" 'NR==idx {print $1}')

    if [ -z "$left_ptr" ] || [ -z "$right_ptr" ]; then
        return 1
    fi

    local left_pos right_pos
    left_pos=$(awk -v ptr="$left_ptr" '$0 ~ ("RC_FREE: " ptr) {print NR; exit}' "$stderr_file")
    right_pos=$(awk -v ptr="$right_ptr" '$0 ~ ("RC_FREE: " ptr) {print NR; exit}' "$stderr_file")

    if [ -z "$left_pos" ] || [ -z "$right_pos" ] || [ "$left_pos" -gt "$right_pos" ]; then
        return 1
    fi

    return 0
}

run_program_test() {
    local file="$1"
    local expected_stdout expected_stderr expected_free_order expected_exit
    local extra_flags=()

    expected_stdout=$(read_expected_lines "Expected" "$file")
    expected_stderr=$(read_expected_lines "Expected stderr" "$file")
    expected_free_order=$(sed -n 's#^// Expected rc free order: *##p' "$file" | head -1)
    expected_exit=$(sed -n 's#^// Expected exit: *##p' "$file" | head -1)
    if [ -z "$expected_exit" ]; then
        expected_exit="0"
    fi

    if [ -n "$expected_free_order" ]; then
        extra_flags+=("--rc-debug")
    fi

    print_test_header "$file"

    local tmp_bin tmp_stdout tmp_stderr
    tmp_bin=$(mktemp /tmp/w0_run_test_XXXXXX)
    tmp_stdout=$(mktemp /tmp/w0_run_stdout_XXXXXX)
    tmp_stderr=$(mktemp /tmp/w0_run_stderr_XXXXXX)

    local compile_output
    compile_output=$("$W0" --lib-path "$LIB_PATH" "${extra_flags[@]}" "$file" | cc -x c -I"$LIB_PATH/include" -o "$tmp_bin" - "$LIB_PATH/whist_runtime.c" 2>&1)
    if [ $? -ne 0 ]; then
        if $verbose; then
            echo -e "${RED}✗ FAIL (compile)${RESET}"
            echo "$compile_output"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (compile)${RESET}"
        fi
        rm -f "$tmp_bin" "$tmp_stdout" "$tmp_stderr"
        ((run_failed++))
        return
    fi

    "$tmp_bin" >"$tmp_stdout" 2>"$tmp_stderr"
    local run_status=$?

    if [ "$run_status" -ne "$expected_exit" ]; then
        if $verbose; then
            echo -e "${RED}✗ FAIL (runtime exit $run_status, expected $expected_exit)${RESET}"
            echo -e "${GRAY}stdout:${RESET}"
            cat "$tmp_stdout"
            echo -e "${GRAY}stderr:${RESET}"
            cat "$tmp_stderr"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (exit $run_status, expected $expected_exit)${RESET}"
        fi
        rm -f "$tmp_bin" "$tmp_stdout" "$tmp_stderr"
        ((run_failed++))
        return
    fi

    if ! check_expected_lines "stdout" "$expected_stdout" "$tmp_stdout"; then
        if $verbose; then
            echo -e "${RED}✗ FAIL (stdout mismatch)${RESET}"
            echo -e "${GRAY}expected lines:${RESET}"
            echo "$expected_stdout"
            echo -e "${GRAY}actual stdout:${RESET}"
            cat "$tmp_stdout"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (stdout)${RESET}"
        fi
        rm -f "$tmp_bin" "$tmp_stdout" "$tmp_stderr"
        ((run_failed++))
        return
    fi

    if ! check_expected_lines "stderr" "$expected_stderr" "$tmp_stderr"; then
        if $verbose; then
            echo -e "${RED}✗ FAIL (stderr mismatch)${RESET}"
            echo -e "${GRAY}expected lines:${RESET}"
            echo "$expected_stderr"
            echo -e "${GRAY}actual stderr:${RESET}"
            cat "$tmp_stderr"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (stderr)${RESET}"
        fi
        rm -f "$tmp_bin" "$tmp_stdout" "$tmp_stderr"
        ((run_failed++))
        return
    fi

    if ! check_rc_free_order "$expected_free_order" "$tmp_stderr"; then
        if $verbose; then
            echo -e "${RED}✗ FAIL (rc free order)${RESET}"
            echo -e "${GRAY}expected:${RESET} $expected_free_order"
            echo -e "${GRAY}actual stderr:${RESET}"
            cat "$tmp_stderr"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (rc free order)${RESET}"
        fi
        rm -f "$tmp_bin" "$tmp_stdout" "$tmp_stderr"
        ((run_failed++))
        return
    fi

    if $verbose; then
        echo -e "${GREEN}✓ PASS${RESET}"
        echo ""
    else
        echo -e " ${GREEN}✓ PASS${RESET}"
    fi

    rm -f "$tmp_bin" "$tmp_stdout" "$tmp_stderr"
    ((run_passed++))
}

run_w0_test_file() {
    local file="$1"
    local expected_lines
    expected_lines=$(read_expected_lines "Expected" "$file")

    print_test_header "$file"

    local output run_status
    output=$("$W0" --lib-path "$LIB_PATH" test "$file" 2>&1)
    run_status=$?

    if [ "$run_status" -ne 0 ] && [ -z "$expected_lines" ]; then
        if $verbose; then
            echo -e "${RED}✗ FAIL (w0 test runtime exit $run_status)${RESET}"
            echo "$output"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (w0 test runtime)${RESET}"
        fi
        ((run_failed++))
        return
    fi

    if [ -n "$expected_lines" ]; then
        while IFS= read -r expected; do
            [ -z "$expected" ] && continue
            if ! printf '%s\n' "$output" | grep -Fq -- "$expected"; then
                if $verbose; then
                    echo -e "${RED}✗ FAIL (missing expected output: $expected)${RESET}"
                    echo "$output"
                    echo ""
                else
                    echo -e " ${RED}✗ FAIL (output)${RESET}"
                fi
                ((run_failed++))
                return
            fi
        done <<< "$expected_lines"
    fi

    if $verbose; then
        echo -e "${GREEN}✓ PASS${RESET}"
        echo ""
    else
        echo -e " ${GREEN}✓ PASS${RESET}"
    fi
    ((run_passed++))
}

run_error_test() {
    local file="$1"
    local expected actual output
    expected=$(sed -n 's#^// Expected error: *##p' "$file" | head -1)

    print_test_header "$file"
    if $verbose; then
        echo -e "${GRAY}Expected to fail with: $expected${RESET}"
    fi

    if output=$("$W0" --lib-path "$LIB_PATH" --check "$file" 2>&1); then
        if $verbose; then
            echo -e "${RED}✗ FAIL (should have failed)${RESET}"
            echo -e "  ${YELLOW}Expected: $expected${RESET}"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (should have failed)${RESET}"
            echo -e "  ${YELLOW}Expected: $expected${RESET}"
        fi
        ((error_failed++))
        return
    fi

    if ! printf '%s\n' "$output" | grep -q 'Error:'; then
        if $verbose; then
            echo -e "${RED}✗ FAIL (no error message)${RESET}"
            echo "$output"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (no error message)${RESET}"
        fi
        ((error_failed++))
        return
    fi

    actual=$(printf '%s\n' "$output" | grep 'Error:' | head -1 | sed 's|.*Error: *||')
    if printf '%s\n' "$actual" | grep -Fq -- "$expected"; then
        if $verbose; then
            echo "$output"
            echo -e "${GREEN}✓ PASS (correct error)${RESET}"
            echo ""
        else
            echo -e " ${GREEN}✓ PASS (correct error)${RESET}"
            echo -e "  ${GRAY}$actual${RESET}"
        fi
        ((error_passed++))
    else
        if $verbose; then
            echo "$output"
            echo -e "${RED}✗ FAIL (wrong error message)${RESET}"
            echo -e "  ${YELLOW}Expected: $expected${RESET}"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (wrong error message)${RESET}"
            echo -e "  ${YELLOW}Expected: $expected${RESET}"
            echo -e "  ${RED}Actual:   $actual${RESET}"
        fi
        ((error_failed++))
    fi
}

print_summary() {
    echo ""
    echo -e "${BLUE}=== Test Summary ===${RESET}"

    if $run_valid; then
        local run_total=$((run_passed + run_failed))
        echo -e "${GREEN}Run Tests:      $run_passed/$run_total passed${RESET}"
        if [ "$run_skipped" -gt 0 ]; then
            echo -e "${GRAY}Run Skipped:    $run_skipped helper module(s)${RESET}"
        fi
    fi

    if $run_errors; then
        local error_total=$((error_passed + error_failed))
        echo -e "${GREEN}Error Cases:    $error_passed/$error_total passed${RESET}"
    fi

    local total_passed=$((run_passed + error_passed))
    local total_failed=$((run_failed + error_failed))
    local total=$((total_passed + total_failed))

    if $run_valid && $run_errors; then
        echo -e "${CYAN}Total:          $total_passed/$total tests passed${RESET}"
    fi

    if [ $total_failed -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${RESET}"
        exit 0
    else
        echo -e "${RED}$total_failed test(s) failed${RESET}"
        exit 1
    fi
}

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --run|--valid) run_valid=true ;;
        --errors)      run_errors=true ;;
        --verbose)     verbose=true ;;
        --help)        usage; exit 0 ;;
        *)             echo "Unknown option: $1"; usage; exit 1 ;;
    esac
    shift
done

# Default to running all tests
if ! $run_valid && ! $run_errors; then
    run_valid=true
    run_errors=true
fi

# Check that w0 exists
if [ ! -x "$W0" ]; then
    echo -e "${RED}Error: $W0 not found or not executable${RESET}"
    echo "Run 'make' first to build the compiler."
    exit 1
fi

# Run tests
echo -e "${BLUE}=== Running W0 Test Suite ===${RESET}"
echo ""

if $run_valid; then
    echo -e "${CYAN}=== Run Tests (test/run/**) ===${RESET}"

    while IFS= read -r f; do
        [ -e "$f" ] || continue

        if file_has_test_blocks "$f"; then
            run_w0_test_file "$f"
            continue
        fi

        if file_has_main "$f"; then
            run_program_test "$f"
        else
            if $verbose; then
                echo -e "${GRAY}--- Skipping helper module $f ---${RESET}"
                echo ""
            fi
            ((run_skipped++))
        fi
    done < <(find test/run -type f -name '*.w' | sort)

    echo ""
fi

if $run_errors; then
    echo -e "${CYAN}=== Error Cases (test/errors/**) ===${RESET}"

    while IFS= read -r f; do
        [ -e "$f" ] || continue
        run_error_test "$f"
    done < <(find test/errors -type f -name '*.w' | sort)

    echo ""
fi

print_summary
