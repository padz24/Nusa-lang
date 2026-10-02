#!/usr/bin/env bash
# IR conformance v2.3.0 (Phase 52, milestone 1): fondasi MIR.
# - `compiler/mir/` murni standar C++ (tanpa compiler/* lain, tanpa LLVM).
# - Unit `test_mir` (dijalankan ctest) + pipeline existing tak tersentuh
#   (program .ns representatif tetap build+run).
set -u
NUSA_BIN="${1:?usage: e2e_mir.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# 1. MIR tanpa include ke lapisan lain / LLVM (self-include mir/ OK).
if grep -rn '#include "compiler/' "$SRC_DIR/compiler/mir/" 2>/dev/null | grep -v '"compiler/mir/' | grep -q .; then
  grep -rn '#include "compiler/' "$SRC_DIR/compiler/mir/" | grep -v '"compiler/mir/'
  fail "mir-isolated" "mir/ include compiler/* lain"
fi
if grep -rn '#include <llvm' "$SRC_DIR/compiler/mir/" 2>/dev/null | grep -q .; then
  fail "mir-isolated" "mir/ include LLVM"
fi
pass "mir-isolated"

# 2. Pipeline tak tersentuh: if/while/call/array tetap jalan.
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1
cat > mir_reg.ns <<'EOF'
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}
fn main() {
    let s = add(20, 10);
    print(s);
    let i = 0;
    while i < 3 {
        i = i + 1;
    }
    print(i);
    if s == 30 {
        print(true);
    } else {
        print(false);
    }
}
EOF
timeout 30 "$NUSA_BIN" run mir_reg.ns >mir_reg.got 2>mir_reg.err || { cat mir_reg.err; fail "mir-regression" "run gagal"; }
printf '30\n3\ntrue\n' | cmp -s mir_reg.got - || { cat mir_reg.got; fail "mir-regression" "output salah"; }
pass "mir-regression"

echo "E2E MIR PASS ($PASS_N cases)"
