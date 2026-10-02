#!/usr/bin/env bash
# Integration test v1.19.0 (Phase 38): self-host lexer differential.
# `nusa lex` (C++) vs `selfhost/lex.ns` (Nusa) harus byte-identik
# pada korpus valid. Korpus dinyatakan bebas \0/overflow (divergensi
# terdokumentasi di docs/selfhosting.md).
set -u
NUSA_BIN="${1:?usage: e2e_selfhost.sh <nusa-binary>}"
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

LEX="$ROOT/selfhost/lex.ns"

# 0. Selfhost + helper lex tanpa error.
"$NUSA_BIN" check "$LEX" > check.log 2>&1 || { cat check.log; fail "check" "selfhost must check clean"; }
pass "check"

# 1. Diferensial per file korpus (valid saja; timeout per file).
N=0
for f in "$ROOT"/std/*.ns "$ROOT"/examples/*.ns "$ROOT"/bench/programs/*.ns "$LEX"; do
  [ -f "$f" ] || continue
  if ! "$NUSA_BIN" lex "$f" > cxx.out 2>/dev/null; then
    echo "SKIP-invalid: $f"
    continue
  fi
  timeout 300 "$NUSA_BIN" run "$LEX" "$f" > self.out 2>self.err || { cat self.err; fail "run" "selfhost failed on $f"; }
  cmp -s cxx.out self.out || { echo "--- diff $f ---"; diff cxx.out self.out | head -n 10; fail "diff" "mismatch on $f"; }
  N=$((N + 1))
done
[ "$N" -ge 25 ] || fail "corpus" "too few files compared ($N)"
echo "compared $N files identical"
pass "differential"

# 2. `nusa lex` CLI: missing file + format spot-check.
"$NUSA_BIN" lex /nonexistent.ns > /dev/null 2>&1 && fail "lex-missing" "should fail"
pass "lex-missing"
printf 'fn main() {\n}\n' > tiny.ns
"$NUSA_BIN" lex tiny.ns > tiny.got 2>&1 || fail "lex" "rc != 0"
printf "'fn'|fn|1|1\nidentifier|main|1|4\n'('|(|1|8\n')'|)|1|9\n'{'|{|1|11\n'}'|}|2|1\nend of file||3|1\n" | diff - tiny.got || fail "lex" "format drift"
pass "lex"

# 3. Korpus bebas hazard divergensi (\0 dan literal raksasa).
if grep -rIlP '\x00' "$ROOT"/std/*.ns "$ROOT"/examples/*.ns "$ROOT"/bench/programs/*.ns "$LEX" 2>/dev/null | grep -q .; then
  fail "hazard" "NUL byte in corpus"
fi
if grep -rEoh '[0-9]{20,}' "$ROOT"/std/*.ns "$ROOT"/examples/*.ns "$ROOT"/bench/programs/*.ns "$LEX" 2>/dev/null | grep -q .; then
  fail "hazard" "giant literal in corpus"
fi
pass "hazard"

echo "E2E SELFHOST PASS ($PASS_N cases)"
