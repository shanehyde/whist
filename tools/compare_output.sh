#!/usr/bin/env bash
#
# compare_output.sh — Compare w0 and wc compiler output for each test file.
#
# Usage:
#   tools/compare_output.sh [--stage lex] [--verbose] [FILTER]
#
# Stages: lex (default). Future: parse, check, codegen.
# FILTER: optional substring to match against test file paths.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W0="$ROOT/w0/bin/w0"
WC="$ROOT/wc/bin/wc"
TEST_DIR="$ROOT/w0/test"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
RESET='\033[0m'

# Defaults
STAGE="lex"
FILTER=""
VERBOSE=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --stage)
            STAGE="$2"
            shift 2
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --help|-h)
            echo "Usage: tools/compare_output.sh [--stage lex] [--verbose] [FILTER]"
            echo ""
            echo "Stages: lex (default)"
            echo "FILTER: substring to match against test file paths"
            exit 0
            ;;
        *)
            FILTER="$1"
            shift
            ;;
    esac
done

# Build stage-specific flags
case "$STAGE" in
    lex)
        W0_FLAGS="--lex"
        WC_FLAGS="--lex"
        ;;
    *)
        echo -e "${RED}Unknown stage: $STAGE${RESET}"
        echo "Supported stages: lex"
        exit 1
        ;;
esac

# Check binaries exist
if [[ ! -x "$W0" ]]; then
    echo -e "${RED}Error: w0 binary not found at $W0${RESET}"
    echo "Run 'make w0' first."
    exit 1
fi
if [[ ! -x "$WC" ]]; then
    echo -e "${RED}Error: wc binary not found at $WC${RESET}"
    echo "Run 'make wc' first."
    exit 1
fi

# Collect test files into array (compatible with bash 3)
TEST_FILES=()
while IFS= read -r -d '' file; do
    TEST_FILES+=("$file")
done < <(find "$TEST_DIR" -name '*.w' -type f -print0 | sort -z)

PASS=0
FAIL=0
LEAK=0
FAIL_LIST=()
LEAK_LIST=()

# Known RC baseline: net alloc−free count.  With the debug library
# (lib/debug/libwhist.a), all RC operations are traced including those
# in whist_runtime.c and stdlib modules, so the expected balance is 0.
KNOWN_LEAKS=0

echo -e "${BOLD}Comparing w0 vs wc output (stage: $STAGE)${RESET}"
echo "---"

for FILE in "${TEST_FILES[@]}"; do
    # Apply filter
    if [[ -n "$FILTER" && "$FILE" != *"$FILTER"* ]]; then
        continue
    fi

    REL_PATH="${FILE#$ROOT/}"

    # Run both compilers, capturing stdout
    W0_OUT=$(mktemp)
    WC_OUT=$(mktemp)

    W0_EXIT=0
    "$W0" $W0_FLAGS "$FILE" > "$W0_OUT" 2>/dev/null || W0_EXIT=$?

    WC_RC=$(mktemp)
    WC_EXIT=0
    WHIST_RC_TRACE=1 "$WC" $WC_FLAGS "$FILE" > "$WC_OUT" 2>"$WC_RC" || WC_EXIT=$?

    # Normalize: strip trailing whitespace from each line
    sed -i '' 's/[[:space:]]*$//' "$W0_OUT" 2>/dev/null || sed -i 's/[[:space:]]*$//' "$W0_OUT"
    sed -i '' 's/[[:space:]]*$//' "$WC_OUT" 2>/dev/null || sed -i 's/[[:space:]]*$//' "$WC_OUT"

    # Check output match
    MATCH=1
    if ! diff -q "$W0_OUT" "$WC_OUT" > /dev/null 2>&1; then
        MATCH=0
        FAIL=$((FAIL + 1))
        FAIL_LIST+=("$REL_PATH")
        echo -e "  ${RED}FAIL${RESET}  $REL_PATH"
        if [[ $VERBOSE -eq 1 ]]; then
            diff "$W0_OUT" "$WC_OUT" | head -20 || true
            echo ""
        fi
    fi

    # Check for RC leaks beyond the known baseline.
    # Use net alloc-free count (address-based tracking is unreliable due to
    # malloc address reuse).
    LEAK_COUNT=$(awk '/^RC_ALLOC:/{a++} /^RC_FREE:/{f++} END{print (a+0)-(f+0)}' "$WC_RC")
    if [[ "$LEAK_COUNT" -ne "$KNOWN_LEAKS" ]]; then
        DELTA=$((LEAK_COUNT - KNOWN_LEAKS))
        LEAK=$((LEAK + 1))
        LEAK_LIST+=("$REL_PATH (delta=$DELTA)")
        echo -e "  ${YELLOW}LEAK${RESET}  $REL_PATH  (net=$LEAK_COUNT, expected=$KNOWN_LEAKS, delta=$DELTA)"
        if [[ $VERBOSE -eq 1 ]]; then
            # Show per-address details for debugging
            awk '/^RC_ALLOC:/{alloc[$2]++} /^RC_FREE:/{free[$2]++} END{
                for(a in alloc) {
                    f = (a in free ? free[a] : 0);
                    if(alloc[a] > f) print a ": alloc=" alloc[a] " free=" f " net=+" (alloc[a]-f)
                }
            }' "$WC_RC"
            echo ""
        fi
    fi

    if [[ $MATCH -eq 1 && "$LEAK_COUNT" -eq "$KNOWN_LEAKS" ]]; then
        PASS=$((PASS + 1))
        if [[ $VERBOSE -eq 1 ]]; then
            echo -e "  ${GREEN}PASS${RESET}  $REL_PATH"
        fi
    fi

    rm -f "$W0_OUT" "$WC_OUT" "$WC_RC"
done

TOTAL=$((PASS + FAIL))
echo "---"
echo -e "${BOLD}Results:${RESET} $TOTAL files compared"
echo -e "  ${GREEN}PASS: $PASS${RESET}"

EXIT_CODE=0

if [[ $FAIL -gt 0 ]]; then
    echo -e "  ${RED}FAIL: $FAIL${RESET}"
    EXIT_CODE=1
fi

if [[ $LEAK -gt 0 ]]; then
    echo -e "  ${YELLOW}LEAK: $LEAK${RESET}"
    EXIT_CODE=1
fi

if [[ $FAIL -gt 0 ]]; then
    echo ""
    echo -e "${RED}Failed files:${RESET}"
    for F in "${FAIL_LIST[@]}"; do
        echo "  $F"
    done
fi

if [[ $LEAK -gt 0 ]]; then
    echo ""
    echo -e "${YELLOW}Files with RC imbalance (expected net=$KNOWN_LEAKS):${RESET}"
    for F in "${LEAK_LIST[@]}"; do
        echo "  $F"
    done
fi

if [[ $EXIT_CODE -eq 0 ]]; then
    echo ""
    echo -e "${GREEN}All outputs match, no extra leaks!${RESET}"
fi

exit $EXIT_CODE
