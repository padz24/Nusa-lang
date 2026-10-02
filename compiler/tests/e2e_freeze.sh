#!/usr/bin/env bash
# Integration test v3.0-candidate (Phase 81): language freeze enforcement.
# Struktural: kandidat ada + 6 area terpetakan + normatif 2.0 utuh +
# spot-run golden (bahasa tetap bekerja, tak berubah).
set -u
NUSA_BIN="${1:?usage: e2e_freeze.sh <nusa-binary>}"
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

CAND="$ROOT/docs/language-spec-3.0-candidate.md"
FRZ="$ROOT/docs/freeze-3.0.md"
SPEC2="$ROOT/docs/language-spec-2.0.md"
[ -f "$CAND" ] || fail "freeze-files" "missing candidate"
[ -f "$FRZ" ] || fail "freeze-files" "missing freeze design"
[ -f "$SPEC2" ] || fail "freeze-files" "missing spec 2.0"
pass "freeze-files"

for m in "syntax:" "type semantics:" "memory semantics:" "module semantics:" "error semantics:" "concurrency semantics:"; do
  grep -q "$m" "$CAND" || fail "freeze-map" "marker hilang: $m"
done
grep -q "Status: \*\*candidate\*\*" "$CAND" || fail "freeze-map" "status candidate hilang"
grep -q "e2e_spec2" "$CAND" || fail "freeze-map" "conformance spec2 hilang"
grep -q "e2e_compat" "$CAND" || fail "freeze-map" "conformance compat hilang"
pass "freeze-map"

grep -q "normative" "$SPEC2" || fail "freeze-normative" "spec 2.0 kehilangan status normatif"
pass "freeze-normative"

cp "$ROOT/examples/hello.ns" ./hello.ns || fail "freeze-spot" "copy hello"
timeout 60 "$NUSA_BIN" run ./hello.ns >hello.got 2>hello.err \
  || { cat hello.err; fail "freeze-spot" "hello run failed"; }
printf 'Hello, Nusa!\n' | cmp -s hello.got - \
  || { cat hello.got; fail "freeze-spot" "hello output berubah (bahasa bergeser?)"; }
pass "freeze-spot"

echo "E2E FREEZE PASS ($PASS_N cases)"
