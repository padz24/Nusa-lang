#!/usr/bin/env bash
# Integration test v2.30.1 (Phase 80): compatibility sweep.
# `check` SEMUA .ns repo (program harus hijau; lib tanpa main
# harus gagal TEPAT dengan "no 'fn main' found") + `build`
# subset runnable. Ambang total mengunci penyusutan korpus.
set -u
NUSA_BIN="${1:?usage: e2e_compat.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

N_CHECK=0
N_LIB=0
while IFS= read -r f; do
  rel="${f#$ROOT/}"
  if timeout 60 "$NUSA_BIN" check "$f" >"c.log" 2>&1; then
    N_CHECK=$((N_CHECK + 1))
  elif grep -q "no 'fn main' found" c.log; then
    N_LIB=$((N_LIB + 1))
  else
    echo "--- $rel ---"; cat c.log; fail "compat-check" "unexpected check failure"
  fi
done < <(find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns" | sort)
echo "checked-ok=$N_CHECK lib-no-main=$N_LIB"
[ "$N_CHECK" -ge 40 ] || fail "compat-count" "too few check-ok ($N_CHECK)"
[ "$N_LIB" -ge 30 ] || fail "compat-count" "too few libs ($N_LIB)"
[ $((N_CHECK + N_LIB)) -ge 80 ] || fail "compat-count" "corpus shrank"
pass "compat-check"

# Build subset runnable (mencakup terbesar + konkurensi + bench).
i=0
for f in "$ROOT/selfhost/parse.ns" "$ROOT/examples/nusa10.ns" "$ROOT/examples/concurrency.ns" "$ROOT/bench/programs/fib.ns" "$ROOT/tests/dist_test.ns"; do
  i=$((i + 1))
  timeout 180 "$NUSA_BIN" build "$f" -o "b$i" >"b$i.log" 2>&1 || { cat "b$i.log"; fail "compat-build" "build failed: $f"; }
  [ -x "b$i" ] || fail "compat-build" "not executable: $f"
done
pass "compat-build"

echo "E2E COMPAT PASS ($PASS_N cases)"
