#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if ! command -v lizard >/dev/null 2>&1; then
    echo "error: lizard is required. Install it and retry." >&2
    exit 1
fi

run_lizard() {
    set +e
    lizard "$@"
    local status=$?
    set -e
    if [ "$status" -gt 1 ]; then
        exit "$status"
    fi
}

echo "== w0 Complexity Baseline =="
echo "Directory: $ROOT_DIR"
echo "Date: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
echo

echo "== Threshold Report (NLOC>30 or CCN>10) =="
run_lizard *.c -Tnloc=30 -Tcyclomatic_complexity=10
echo

echo "== Top Functions by Cyclomatic Complexity =="
run_lizard *.c --sort cyclomatic_complexity
echo

echo "== Top Functions by NLOC =="
run_lizard *.c --sort nloc
