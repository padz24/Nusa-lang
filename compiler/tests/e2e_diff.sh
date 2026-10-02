#!/usr/bin/env bash
# Differential conformance v3.27.0 (Phase 126, milestone 1): matriks
# level-opt + profil + debug — stdout+exit IDENTIK di semua konfigurasi
# untuk tiap program (semantik sama, optimasi boleh beda).
# Melengkapi O0≡O2 e2e_verify (bukan duplikat): O1/O3/Os/Oz, -g,
# --profile dev/debug/release, dan default.
set -u
NUSA_BIN="${1:?usage: e2e_diff.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

CONFIGS=("default" "-O0" "-O1" "-O2" "-O3" "-Os" "-Oz" "-g" "prof-release" "prof-debug")
FLAGS=("" "-O0" "-O1" "-O2" "-O3" "-Os" "-Oz" "-g" "--profile release" "--profile debug")

for prog in fib loopsum strcat matchdispatch closureapply; do
  cp "$SRC_DIR/bench/programs/$prog.ns" prog.ns
  # Baseline = default.
  timeout 300 "$NUSA_BIN" build prog.ns -o base >base.log 2>&1 \
    || { cat base.log; fail "diff-$prog" "build default gagal"; }
  timeout 300 ./base >base.got 2>base.err; bese=$?
  crashed "$bese" && fail "diff-$prog" "CRASH baseline"
  i=0
  for cfg in "${CONFIGS[@]}"; do
    # shellcheck disable=SC2086
    timeout 300 "$NUSA_BIN" build prog.ns -o "cfg$i" ${FLAGS[$i]} \
      >"cfg$i.log" 2>&1 || { cat "cfg$i.log"; fail "diff-$prog" "build $cfg gagal"; }
    timeout 300 ./"cfg$i" >"cfg$i.got" 2>"cfg$i.err"; rce=$?
    crashed "$rce" && fail "diff-$prog" "CRASH $cfg"
    [ "$rce" -eq "$bese" ] || fail "diff-$prog" "exit $cfg=$rce vs $bese"
    cmp -s "cfg$i.got" base.got || fail "diff-$prog" "stdout $cfg beda"
    i=$((i + 1))
  done
  pass "diff-$prog"
done

echo "E2E DIFF PASS ($PASS_N cases)"
