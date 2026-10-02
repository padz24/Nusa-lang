#!/usr/bin/env bash
# Trace conformance v3.20.0 (Phase 119, milestone 1): std/trace +
# `nusa trace run` (injeksi ID) + `nusa trace check` (validator log).
set -u
NUSA_BIN="${1:?usage: e2e_trace.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Berkas uji repo hijau.
timeout 120 "$NUSA_BIN" test "$SRC_DIR/tests/trace_test.ns" >t.log 2>&1 \
  || { cat t.log; fail "trace-test" "rc != 0"; }
grep -q "1 passed, 0 failed" t.log \
  || { cat t.log; fail "trace-test" "ringkasan salah"; }
pass "trace-test"

# 2. trace run: satu ID untuk semua baris; check memvalidasi.
cat > app.ns <<'EOF'
import trace;
fn main() {
    let t = trace_begin("serve");
    trace_event("db", "ok", "rows=3");
    trace_end(t);
}
EOF
timeout 180 "$NUSA_BIN" trace run app.ns >app.log 2>app.err \
  || { cat app.err; fail "trace-run" "rc != 0"; }
grep -q "trace: request id tr-" app.err \
  || { cat app.err; fail "trace-run" "tanpa request id"; }
ID=$(grep -o 'tr-[0-9-]*-[0-9a-f]*' app.err | head -n 1)
[ -n "$ID" ] || fail "trace-run" "id tak terbaca"
NID=$(grep -o "\"id\":\"[^\"]*\"" app.log | sort -u | wc -l)
[ "$NID" -eq 1 ] || { cat app.log; fail "trace-run" "id tak tunggal"; }
grep -q "\"id\":\"$ID\"" app.log \
  || fail "trace-run" "id run != id log"
timeout 60 "$NUSA_BIN" trace check app.log >check.log 2>&1 \
  || { cat check.log; fail "trace-check" "rc != 0"; }
grep -q "3 lines (1 begin, 1 end, 1 event), 0 invalid" check.log \
  || { cat check.log; fail "trace-check" "ringkasan salah"; }
pass "trace-runcheck"

# 3. check menolak log rusak dengan nomor baris.
printf '{"type":"begin","id":"a","op":"x","t":1}\nBUKAN-JSON\n{"type":"event","id":"a"}\n' > bad.log
if timeout 60 "$NUSA_BIN" trace check bad.log >badcheck.log 2>&1; then
  fail "trace-bad" "rc=0 untuk log rusak"
fi
grep -q "2 invalid" badcheck.log \
  || { cat badcheck.log; fail "trace-bad" "hitungan salah"; }
grep -q "line 2" badcheck.log \
  || { cat badcheck.log; fail "trace-bad" "nomor baris hilang"; }
if timeout 60 "$NUSA_BIN" trace bogus > /dev/null 2>&1; then
  fail "trace-usage" "rc=0 tanpa subcommand"
fi
pass "trace-bad"

# 4. fmt bersih.
timeout 60 "$NUSA_BIN" fmt --check "$SRC_DIR/std/trace.ns" \
  >fmt.log 2>&1 || { cat fmt.log; fail "trace-fmt" "stdlib tak fmt-clean"; }
pass "trace-fmt"

echo "E2E TRACE PASS ($PASS_N cases)"
