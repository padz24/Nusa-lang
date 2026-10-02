#!/usr/bin/env bash
# Integration test FINAL v1.0.0: Definition-of-DoD spot checks.
#  - Program uji Nusa 1.0 verbatim: build + run + output eksak.
#  - Format diagnostics sesuai contoh master (span carets).
#  - Seluruh 8 perintah CLI berfungsi.
set -u
NUSA_BIN="${1:?usage: e2e_final.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; }

# 1. Program uji 1.0 verbatim
cp "$ROOT/examples/nusa10.ns" ./nusa10.ns
timeout 60 "$NUSA_BIN" build nusa10.ns -o nusa10 >build.log 2>&1 ||
  fail "1.0-build" "should compile"
[ -x ./nusa10 ] || fail "1.0-build" "executable missing"
file ./nusa10 2>/dev/null | grep -qi "aarch64" || fail "1.0-arch" "not AArch64"
timeout 20 ./nusa10 >run.got 2>run.log || fail "1.0-run" "should run"
printf 'Result is greater than 20\n30\nNusa\n' | diff - run.got ||
  fail "1.0-output" "wrong output"
grep -q "define i32 @main(i32" nusa10.ns.ll || fail "1.0-ir" "IR missing @main"
grep -q "define i64 @add" nusa10.ns.ll || fail "1.0-ir" "IR missing @add"
grep -q "%Person = type" nusa10.ns.ll || fail "1.0-ir" "IR missing %Person"
pass "1.0-program"

# 2. Format diagnostics = contoh master (span carets + help)
printf 'fn main() {\n    let a = 1;\n    print(result);\n}\n' > diag.ns
timeout 20 "$NUSA_BIN" build diag.ns -o diag.out >diag.log 2>&1
[ $? -ne 0 ] || fail "diag" "should not build"
grep -q "diag.ns:3:11: error: undefined variable 'result'" diag.log ||
  fail "diag" "header wrong"
grep -q "print(result);" diag.log || fail "diag" "snippet missing"
grep -qF '^^^^^^' diag.log || fail "diag" "span carets missing"
grep -q "help: declare 'result' before using it" diag.log ||
  fail "diag" "suggestion missing"
pass "diagnostics"

# 3. Seluruh CLI 1.0
"$NUSA_BIN" --version 2>&1 | grep -q "nusa version" || fail "cli" "--version"
"$NUSA_BIN" --help 2>&1 | grep -q "Usage" || fail "cli" "--help"
printf 'fn main() { print("c"); }\n' > c.ns
"$NUSA_BIN" check c.ns >/dev/null 2>&1 || fail "cli" "check"
"$NUSA_BIN" build c.ns -o c.out >/dev/null 2>&1 || fail "cli" "build"
[ -x ./c.out ] || fail "cli" "build output"
timeout 20 "$NUSA_BIN" run c.ns 2>/dev/null | grep -q "c" || fail "cli" "run"
mkdir -p tcli && printf 'fn main() { assert(true); }\n' > tcli/ok_test.ns
"$NUSA_BIN" test tcli >/dev/null 2>&1 || fail "cli" "test"
printf 'fn main() {\n    print("c");\n}\n' > cf.ns
"$NUSA_BIN" fmt --check cf.ns >/dev/null 2>&1 || fail "cli" "fmt"
"$NUSA_BIN" doc c.ns 2>/dev/null | grep -q "API" || fail "cli" "doc"
pass "cli"

echo "E2E FINAL PASS"
