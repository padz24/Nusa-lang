#!/usr/bin/env bash
# Integration test v1.1.0 (diagnostics):
#  A. Error recovery: error di beberapa item top-level SEMUA dilaporkan.
#  B. Error cap: file sampah dibatasi (25 + 1 ringkasan per tahap).
#  C. Pesan Inggris: hint memakai bahasa Inggris, tanpa sisa "contoh:".
#  D. Guard sumber: tidak ada literal "contoh:" di sumber compiler.
set -u
NUSA_BIN="${1:?usage: e2e_diagnostics.sh <nusa-binary>}"
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

# A. Recovery: dua error parse di dua fungsi → dua-duanya terlapor.
printf 'fn a() { let x = ; }\nfn b() { let y = ; }\nfn main() { }\n' > rec.ns
REC_OUT="$("$NUSA_BIN" check rec.ns 2>&1)"; REC_RC=$?
[ $REC_RC -ne 0 ] || fail "recovery" "should fail (rc=0)"
echo "$REC_OUT" | grep -q "rec.ns:1:.*expected expression" || { echo "$REC_OUT"; fail "recovery" "first error missing"; }
echo "$REC_OUT" | grep -q "rec.ns:2:.*expected expression" || { echo "$REC_OUT"; fail "recovery" "second error missing"; }
pass "recovery-multi"

# A2. Token liar top-level dilaporkan, bukan crash/hang.
printf 'let x = 1;\nfn main() { print(1); }\n' > stray.ns
timeout 20 "$NUSA_BIN" check stray.ns >stray.log 2>&1
[ $? -ne 0 ] || fail "stray" "should fail (rc=0)"
grep -q "expected 'import', 'type', 'const', 'trait', 'impl', 'struct', 'enum', 'extern', or 'fn' declaration" stray.log || { cat stray.log; fail "stray" "message missing"; }
pass "recovery-stray"

# B. Cap: 60 karakter liar → 26 baris error lexer + ringkasan, tanpa duplikat parser.
{ for _ in $(seq 1 60); do printf '@'; done; printf '\n'; } > garbage.ns
G_OUT="$("$NUSA_BIN" check garbage.ns 2>&1)"; G_RC=$?
[ $G_RC -ne 0 ] || fail "cap" "should fail (rc=0)"
echo "$G_OUT" | grep -q "too many errors" || { echo "$G_OUT"; fail "cap" "summary missing"; }
[ "$(echo "$G_OUT" | grep -c "error")" -le 28 ] || fail "cap" "error flood not capped"
pass "error-cap"

# C. Hint Inggris (pesan + help), tanpa "contoh:" di output mana pun.
ALL_OUT=""
printf 'fn main() { let x = ; }\n' > h1.ns
H1="$("$NUSA_BIN" check h1.ns 2>&1)"; ALL_OUT="$ALL_OUT$H1"
echo "$H1" | grep -q "help: expression: number, string" || { echo "$H1"; fail "hint-expr" "english hint missing"; }
printf 'fn f(a: void) { }\nfn main() { }\n' > h2.ns
H2="$("$NUSA_BIN" check h2.ns 2>&1)"; ALL_OUT="$ALL_OUT$H2"
echo "$H2" | grep -q "help: use 'i64', 'bool', or 'string'" || { echo "$H2"; fail "hint-void-param" "english hint missing"; }
printf 'fn v() { }\nfn main() { let x = v(); }\n' > h3.ns
H3="$("$NUSA_BIN" check h3.ns 2>&1)"; ALL_OUT="$ALL_OUT$H3"
echo "$H3" | grep -q "call the void function as a statement" || { echo "$H3"; fail "hint-void-init" "english hint missing"; }
printf 'fn main() { print(1 / 0); }\n' > h4.ns
H4="$("$NUSA_BIN" check h4.ns 2>&1)"; ALL_OUT="$ALL_OUT$H4"
echo "$H4" | grep -q "the divisor here is always 0" || { echo "$H4"; fail "hint-divzero" "english hint missing"; }
printf 'fn main() { let x: i64 = "s"; }\n' > h5.ns
H5="$("$NUSA_BIN" check h5.ns 2>&1)"; ALL_OUT="$ALL_OUT$H5"
echo "$H5" | grep -q "change the annotation or the initial value to match" || { echo "$H5"; fail "hint-mismatch" "english hint missing"; }
printf 'import b;\nfn main() { }\n' > cyc_a.ns
printf 'import cyc_a;\nfn helper() -> i64 { return 1; }\n' > b.ns
H6="$("$NUSA_BIN" check cyc_a.ns 2>&1)"; ALL_OUT="$ALL_OUT$H6"
echo "$H6" | grep -q "break the cycle by moving" || { echo "$H6"; fail "hint-cycle" "english hint missing"; }
echo "$ALL_OUT" | grep -q "contoh:" && fail "hint-language" "indonesian hint leaked"
pass "hints-english"

# D. Guard sumber compiler: tidak ada literal "contoh:" (pesan user = Inggris).
if grep -rn 'contoh:' "$ROOT/compiler" --include=*.cpp --include=*.h | grep -v "tests/"; then
  fail "source-guard" "indonesian literal in compiler sources"
fi
pass "source-guard"

echo "DIAGNOSTICS PASS ($PASS_N cases)"
