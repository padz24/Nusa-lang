#!/usr/bin/env bash
# Const conformance v2.21.0 (Phase 70, RFC-0009 slice 1).
# Item top-level `const` (literal/arith/ref-const) + 10 penolakan +
# fmt/doc. Gagal-bersih = exit != 0 tanpa sinyal.
set -u
NUSA_BIN="${1:?usage: e2e_const.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

run_ok() {
  local name="$1"
  timeout 30 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run gagal"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "output salah"; }
  pass "$name"
}

expect_fail() {
  local name="$1" pattern="$2"
  timeout 30 "$NUSA_BIN" check "$name.ns" >"$name.log" 2>&1
  local rc=$?
  if crashed "$rc"; then cat "$name.log"; fail "$name" "CRASH (rc=$rc)"; fi
  [ $rc -ne 0 ] || fail "$name" "harusnya gagal (rc=0)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pola '$pattern' hilang"; }
  pass "$name"
}

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== runtime (terima) =="
cat > c_basic.ns <<'EOF'
const MAX: i64 = 100;
const DOUBLE = MAX * 2;
const OK: bool = DOUBLE == 200;
const GREET: string = "hi";
const NEG = 0 - 7;
fn main() {
    print(DOUBLE);
    print(MAX + 1);
    print(OK);
    print(GREET);
    print(NEG * 2);
}
EOF
printf '200\n101\ntrue\nhi\n-14\n' > c_basic.want
run_ok c_basic

cat > c_shadow.ns <<'EOF'
const X = 10;
fn f(X: i64) -> i64 {
    return X + 1;
}
fn main() {
    print(f(1));
    print(X);
}
EOF
printf '2\n10\n' > c_shadow.want
run_ok c_shadow

cat > c_match.ns <<'EOF'
const ONE = 1;
const TWO = ONE + 1;
fn f(x: i64) -> i64 {
    match x {
        1 => {
            return ONE;
        },
        _ => {
            return TWO;
        },
    }
    return 0;
}
fn main() {
    print(f(1));
    print(f(9));
}
EOF
printf '1\n2\n' > c_match.want
run_ok c_match

echo "== penolakan (10) =="
printf 'const A = B;\nconst B = A;\nfn main() {\n    print(A);\n}\n' > c_cycle.ns
expect_fail c_cycle "circular constant"
printf 'const Z = 1 / 0;\nfn main() {\n    print(Z);\n}\n' > c_div0.ns
expect_fail c_div0 "division by zero"
printf 'const BIG = 9223372036854775807 + 1;\nfn main() {\n    print(BIG);\n}\n' > c_over.ns
expect_fail c_over "overflow"
printf 'const X = parse_int("1");\nfn main() {\n    print(X);\n}\n' > c_call.ns
expect_fail c_call "not a constant expression"
printf 'const X = [1, 2];\nfn main() {\n    print(X);\n}\n' > c_arr.ns
expect_fail c_arr "not a constant expression"
printf 'const X = 1;\nfn main() {\n    X = 2;\n}\n' > c_assign.ns
expect_fail c_assign "cannot assign to constant"
printf 'const X: bool = 1;\nfn main() {\n    print(X);\n}\n' > c_mismatch.ns
expect_fail c_mismatch "mismatched types in 'const'"
printf 'const X = 1;\nconst X = 2;\nfn main() {\n    print(X);\n}\n' > c_dup.ns
expect_fail c_dup "duplicate constant"
printf 'fn MAX() -> i64 {\n    return 1;\n}\nconst MAX = 2;\nfn main() {\n    print(MAX);\n}\n' > c_clash.ns
expect_fail c_clash "duplicate definition"
printf 'const S = "a" + "b";\nfn main() {\n    print(S);\n}\n' > c_strop.ns
expect_fail c_strop "arithmetic needs"

echo "== tooling =="
printf 'const   MAX:i64=  100;\nfn main() {\n    print(MAX);\n}\n' > c_fmt.ns
"$NUSA_BIN" fmt c_fmt.ns >/dev/null 2>&1 || fail "c_fmt" "fmt gagal"
"$NUSA_BIN" fmt --check c_fmt.ns >/dev/null 2>&1 || fail "c_fmt" "tak bersih pasca-fmt"
grep -q "const MAX: i64 = 100;" c_fmt.ns || fail "c_fmt" "bentuk salah"
pass "c_fmt"
printf '/// Batas atas.\nconst MAX = 100;\nfn main() {\n    print(MAX);\n}\n' > c_doc.ns
"$NUSA_BIN" doc c_doc.ns 2>/dev/null | grep -q "## Constants" || fail "c_doc" "seksi hilang"
"$NUSA_BIN" doc c_doc.ns 2>/dev/null | grep -q "const MAX" || fail "c_doc" "entri hilang"
pass "c_doc"

echo "E2E CONST PASS ($PASS_N cases)"
