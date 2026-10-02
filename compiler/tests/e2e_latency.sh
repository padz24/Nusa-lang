#!/usr/bin/env bash
# Latency conformance v3.25.0 (Phase 124, milestone 1): harness jitter
# selesai + metrik hadir + invarian robust. BUKAN gate batas-keras
# (varian perangkat wajar — angka tercatat di docs/latency-3.25.md).
set -u
NUSA_BIN="${1:?usage: e2e_latency.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

bash "$SRC_DIR/bench/latency.sh" "$NUSA_BIN" 7 >suite.log 2>&1 \
  || { tail -n 5 suite.log; fail "lat-suite" "rc != 0"; }
grep -q "LATENCY DONE" suite.log || fail "lat-suite" "tak selesai"
pass "lat-suite"

for d in "runs" "min_ms" "median_ms" "max_ms" "spread_ms"; do
  grep -q "^lat:$d=" suite.log || fail "lat-dims" "hilang: $d"
  v="$(grep "^lat:$d=" suite.log | cut -d= -f2)"
  [ -n "$v" ] || fail "lat-dims" "kosong: $d"
done
[ "$(grep "^lat:runs=" suite.log | cut -d= -f2)" = "7" ] \
  || fail "lat-dims" "runs != 7"
pass "lat-dims"

# Invarian robust: max > 0, max >= median >= min (urutan sehat).
mx="$(grep "^lat:max_ms=" suite.log | cut -d= -f2)"
md="$(grep "^lat:median_ms=" suite.log | cut -d= -f2)"
mn="$(grep "^lat:min_ms=" suite.log | cut -d= -f2)"
python3 - "$mn" "$md" "$mx" <<'PYEOF' || fail "lat-order" "urutan rusak"
import sys
mn, md, mx = (int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
sys.exit(0 if 0 < mn <= md <= mx else 1)
PYEOF
pass "lat-order"

echo "E2E LATENCY PASS ($PASS_N cases)"
