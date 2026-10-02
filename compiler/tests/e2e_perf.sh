#!/usr/bin/env bash
# Performance gate v3.0.0-beta (Phase 86): suite resmi selesai +
# 8/8 dimensi hadir + invarian (hangat<=dingin, ukuran positif).
# BUKAN gate perbandingan-ketat (deviasi perangkat wajar) —
# angka tercatat di docs/perf-3.0.md §4.
set -u
NUSA_BIN="${1:?usage: e2e_perf.sh <nusa-binary>}"
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

bash "$ROOT/bench/suite.sh" "$NUSA_BIN" >suite.log 2>&1 \
  || { tail -n 5 suite.log; fail "perf-suite" "rc != 0"; }
grep -q "SUITE DONE" suite.log || fail "perf-suite" "tak selesai"
pass "perf-suite"

for d in "compile_hello_s" "incr_cold_s" "incr_warm_s" "size_compiler_bytes" \
         "startup_hello_ms" "run_fib_median_ms" "rss_fib_kib" \
         "pkg_tree_s" "pkg_build_s" "lsp_init_ms"; do
  grep -q "^perf:$d=" suite.log || fail "perf-dims" "hilang: $d"
  v="$(grep "^perf:$d=" suite.log | cut -d= -f2)"
  [ -n "$v" ] || fail "perf-dims" "kosong: $d"
done
pass "perf-dims"

cold="$(grep "^perf:incr_cold_s=" suite.log | cut -d= -f2)"
warm="$(grep "^perf:incr_warm_s=" suite.log | cut -d= -f2)"
python3 - "$cold" "$warm" <<'PYEOF' || fail "perf-incr" "warm>cold ($cold vs $warm)"
import sys
sys.exit(0 if float(sys.argv[2]) <= float(sys.argv[1]) else 1)
PYEOF
pass "perf-incr"

cbin="$(grep "^perf:size_compiler_bytes=" suite.log | cut -d= -f2)"
[ "$cbin" -gt 1000000 ] || fail "perf-size" "compiler mencurigakan: $cbin"
for p in fib loopsum strcat matchdispatch closureapply; do
  r="$(grep "^perf:rss_${p}_kib=" suite.log | cut -d= -f2)"
  [ "$r" -gt 1000 ] || fail "perf-size" "rss $p mencurigakan: $r"
done
pass "perf-size"

echo "E2E PERF PASS ($PASS_N cases)"
