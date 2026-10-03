#!/usr/bin/env bash
# run_diff.sh - the acceptance test for the project.
#
# Runs the scripted command sequence against both implementations, each on its
# own private copy of the fixture so that the persisting `add` command cannot
# make one run affect the other, then diffs the two stdout streams. The Day 3
# milestone is not met until this diff is empty.
#
# stderr is captured separately and deliberately excluded from the comparison,
# because --diag diagnostics and error messages go there. Only stdout is under
# the byte-identical contract.
#
# Usage: spec/run_diff.sh [--diag]
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

DIAG=""
[[ "${1:-}" == "--diag" ]] && DIAG="--diag"

CPP_BIN="$ROOT/cpp/expense_tracker"
PY_MAIN="$ROOT/python/main.py"

if [[ ! -x "$CPP_BIN" ]]; then
  echo "Error: $CPP_BIN not built. Run: make -C $ROOT/cpp" >&2
  exit 1
fi

cp "$ROOT/spec/expenses.csv" "$WORK/cpp.csv"
"$CPP_BIN" --file "$WORK/cpp.csv" $DIAG \
  < "$ROOT/spec/commands.txt" > "$WORK/cpp.out" 2> "$WORK/cpp.err"
CPP_STATUS=$?

if [[ ! -f "$PY_MAIN" ]]; then
  echo "Python implementation not present yet. C++ stdout captured at $WORK/cpp.out" >&2
  echo "--- cpp stdout ---"
  cat "$WORK/cpp.out"
  echo "--- cpp stderr ---"
  cat "$WORK/cpp.err"
  exit 0
fi

cp "$ROOT/spec/expenses.csv" "$WORK/py.csv"
python3 "$PY_MAIN" --file "$WORK/py.csv" $DIAG \
  < "$ROOT/spec/commands.txt" > "$WORK/py.out" 2> "$WORK/py.err"
PY_STATUS=$?

FAILED=0

if ! diff -u "$WORK/cpp.out" "$WORK/py.out"; then
  echo "FAIL: stdout differs between implementations" >&2
  FAILED=1
fi

# The persisted file must agree too, otherwise `add` is only accidentally
# compatible and the next run would diverge.
if ! diff -u "$WORK/cpp.csv" "$WORK/py.csv"; then
  echo "FAIL: persisted expenses.csv differs between implementations" >&2
  FAILED=1
fi

if [[ "$CPP_STATUS" != "$PY_STATUS" ]]; then
  echo "FAIL: exit status differs (cpp=$CPP_STATUS python=$PY_STATUS)" >&2
  FAILED=1
fi

if [[ "$FAILED" == "0" ]]; then
  echo "PASS: implementations agree on stdout, persisted file, and exit status"
fi
exit "$FAILED"
