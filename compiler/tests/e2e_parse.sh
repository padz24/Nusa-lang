#!/usr/bin/env bash
# Integration test v2.23.0 (Phase 72): self-host parser differential.
# `nusa ast` (C++) vs `selfhost/parse.ns` (Nusa, fused lexer-parser)
# harus byte-identik pada korpus valid. Korpus dinyatakan bebas
# hazard divergensi (lihat docs/selfhost-2.23.md §4).
set -u
NUSA_BIN="${1:?usage: e2e_parse.sh <nusa-binary>}"
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

PARSE="$ROOT/selfhost/parse.ns"

# 0. Selfhost parser tanpa error.
"$NUSA_BIN" check "$PARSE" > check.log 2>&1 || { cat check.log; fail "check" "selfhost must check clean"; }
pass "check"

# 1. Diferensial per file korpus (valid saja; timeout per file).
N=0
for f in "$ROOT"/std/*.ns "$ROOT"/examples/*.ns "$ROOT"/bench/programs/*.ns "$ROOT"/selfhost/*.ns; do
  [ -f "$f" ] || continue
  if ! "$NUSA_BIN" ast "$f" > cxx.out 2>/dev/null; then
    echo "SKIP-invalid: $f"
    continue
  fi
  timeout 500 "$NUSA_BIN" run "$PARSE" "$f" > self.out 2>self.err || { cat self.err; fail "run" "selfhost failed on $f"; }
  cmp -s cxx.out self.out || { echo "--- diff $f ---"; diff cxx.out self.out | head -n 6; fail "diff" "mismatch on $f"; }
  N=$((N + 1))
done
[ "$N" -ge 25 ] || fail "corpus" "too few files compared ($N)"
echo "compared $N files identical"
pass "differential"

# 2. `nusa ast` CLI: missing file + format spot-check.
"$NUSA_BIN" ast /nonexistent.ns > /dev/null 2>&1 && fail "ast-missing" "should fail"
pass "ast-missing"
printf 'fn main() {\n print(1);\n}\n' > tiny.ns
"$NUSA_BIN" ast tiny.ns > tiny.got 2>&1 || fail "ast" "rc != 0"
printf '(program (fn main() -> void (print 1)))\n' | diff - tiny.got || fail "ast" "format drift"
pass "ast"

# 3. Korpus bebas hazard divergensi (NUL + literal raksasa).
# (Normalisasi int nol-padding + escape \0 tercakup diferensial §1.)
if grep -rIlP '\x00' "$ROOT"/std/*.ns "$ROOT"/examples/*.ns "$ROOT"/bench/programs/*.ns "$ROOT"/selfhost/*.ns 2>/dev/null | grep -q .; then
  fail "hazard" "NUL byte in corpus"
fi
if grep -rEoh '[0-9]{20,}' "$ROOT"/std/*.ns "$ROOT"/examples/*.ns "$ROOT"/bench/programs/*.ns "$ROOT"/selfhost/*.ns 2>/dev/null | grep -q .; then
  fail "hazard" "giant literal in corpus"
fi
pass "hazard"

echo "E2E PARSE PASS ($PASS_N cases)"
