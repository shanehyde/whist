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
valid_passed=0
valid_failed=0
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
    echo "  --valid     Run only valid program tests"
    echo "  --errors    Run only error case tests"
    echo "  --verbose   Show detailed output"
    echo "  --help      Show this help"
    echo ""
    echo "With no options, runs all tests."
}

run_valid_test() {
    local file="$1"
    local name=$(basename "$file")

    if $verbose; then
        echo -e "${YELLOW}--- Testing $file ---${RESET}"
        if "$W0" --lib-path "$LIB_PATH" --check "$file" 2>&1; then
            echo -e "${GREEN}✓ PASS${RESET}"
            ((valid_passed++))
        else
            echo -e "${RED}✗ FAIL${RESET}"
            ((valid_failed++))
        fi
        echo ""
    else
        printf "${BOLD}%-35s${RESET}" "$name:"
        if "$W0" --lib-path "$LIB_PATH" --check "$file" >/dev/null 2>&1; then
            echo -e " ${GREEN}✓ PASS${RESET}"
            ((valid_passed++))
        else
            echo -e " ${RED}✗ FAIL${RESET}"
            "$W0" --lib-path "$LIB_PATH" --check "$file" 2>&1 | sed "s/^/  ${RED}/" | sed "s/$/${RESET}/"
            ((valid_failed++))
        fi
    fi
}

run_error_test() {
    local file="$1"
    local name=$(basename "$file")
    local expected=$(grep "// Expected error:" "$file" | sed 's|.*// Expected error: *||' | head -1)

    if $verbose; then
        echo -e "${YELLOW}--- Testing $file ---${RESET}"
        echo -e "${GRAY}Expected to fail with: $expected${RESET}"
    else
        printf "${BOLD}%-35s${RESET}" "$name:"
    fi

    local output
    if output=$("$W0" --lib-path "$LIB_PATH" --check "$file" 2>&1); then
        # Test passed when it should have failed
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

    local actual=$(echo "$output" | grep "Error:" | head -1 | sed 's|.*Error: *||')

    if ! echo "$output" | grep -q "Error:"; then
        # Failed but no error message
        if $verbose; then
            echo -e "${RED}✗ FAIL (no error message)${RESET}"
            echo "$output"
            echo ""
        else
            echo -e " ${RED}✗ FAIL (no error message)${RESET}"
            echo -e "  ${YELLOW}Expected: $expected${RESET}"
        fi
        ((error_failed++))
    elif echo "$actual" | grep -qF "$expected"; then
        # Correct error
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
        # Wrong error message
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
        local valid_total=$((valid_passed + valid_failed))
        echo -e "${GREEN}Valid Programs: $valid_passed/$valid_total passed${RESET}"
    fi

    if $run_errors; then
        local error_total=$((error_passed + error_failed))
        echo -e "${GREEN}Error Cases:    $error_passed/$error_total passed${RESET}"
    fi

    local total_passed=$((valid_passed + error_passed))
    local total_failed=$((valid_failed + error_failed))
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
        --valid)   run_valid=true ;;
        --errors)  run_errors=true ;;
        --verbose) verbose=true ;;
        --help)    usage; exit 0 ;;
        *)         echo "Unknown option: $1"; usage; exit 1 ;;
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
    echo -e "${CYAN}=== Valid Programs (should pass) ===${RESET}"
    for f in test/*.w; do
        case "$f" in test/error_*) continue;; esac
        run_valid_test "$f"
    done
    echo ""
fi

if $run_errors; then
    echo -e "${CYAN}=== Error Cases (should fail with specific errors) ===${RESET}"
    for f in test/error_*.w; do
        [ -e "$f" ] || continue
        run_error_test "$f"
    done
fi

print_summary
