#!/usr/bin/env bash
# Metamorphic conformance v3.26.0 (Phase 125, milestone 1): formatter
# transparan-semantik — dump AST sebelum/sesudah `fmt` byte-identik,
# se-korpus + program fuzz. Tanpa oracle eksternal (properti
# metamorfik): pemformat ulang tak boleh mengubah makna.
set -u
NUSA_BIN="${1:?usage: e2e_fmtprop.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Determinisme dump AST (oracle bagi properti: dump harus stabil).
for f in "$SRC_DIR/examples/hello.ns" "$SRC_DIR/selfhost/lex.ns" \
         "$SRC_DIR/std/math.ns"; do
  timeout 120 "$NUSA_BIN" ast "$f" > d1.txt 2>derr.log \
    || { cat derr.log; fail "fmtprop-det" "ast gagal: $f"; }
  timeout 120 "$NUSA_BIN" ast "$f" > d2.txt 2>>derr.log \
    || { cat derr.log; fail "fmtprop-det" "ast gagal-2: $f"; }
  cmp -s d1.txt d2.txt || fail "fmtprop-det" "dump tak-deterministik: $f"
done
pass "fmtprop-det"

# 2. fmt menjaga AST se-korpus runnable.
N=0
while IFS= read -r f; do
  rel="${f#$SRC_DIR/}"
  cp "$f" case.ns
  timeout 120 "$NUSA_BIN" ast case.ns > before.txt 2>ast.log \
    || { cat ast.log; fail "fmtprop-corpus" "ast gagal: $rel"; }
  timeout 60 "$NUSA_BIN" fmt case.ns >fmt.log 2>&1 \
    || { cat fmt.log; fail "fmtprop-corpus" "fmt gagal: $rel"; }
  timeout 120 "$NUSA_BIN" ast case.ns > after.txt 2>>ast.log \
    || { cat ast.log; fail "fmtprop-corpus" "ast-2 gagal: $rel"; }
  cmp -s before.txt after.txt \
    || fail "fmtprop-corpus" "AST berubah oleh fmt: $rel"
  N=$((N + 1))
done < <(find "$SRC_DIR/std" "$SRC_DIR/examples" "$SRC_DIR/tutorials" \
              "$SRC_DIR/bench" "$SRC_DIR/selfhost" "$SRC_DIR/tests" \
              "$SRC_DIR/packages" "$SRC_DIR/realworld" -name "*.ns" | sort)
echo "fmtprop-corpus: $N files"
[ "$N" -ge 80 ] || fail "fmtprop-corpus" "korpus menyusut ($N)"
pass "fmtprop-corpus"

# 3. Properti di atas program fuzz berseed (bentuk di luar korpus).
if ! python3 "$SRC_DIR/compiler/tests/fuzz_gen.py" 125 25 . >manifest.txt 2>fuzz.log; then
  cat fuzz.log; fail "fmtprop-fuzz" "generator gagal"
fi
NF=0
while IFS= read -r line; do
  f="${line%% *}"
  [ -f "$f" ] || fail "fmtprop-fuzz" "hilang: $f"
  cp "$f" fz.ns
  if ! timeout 60 "$NUSA_BIN" ast fz.ns > fbefore.txt 2>/dev/null; then
    continue # program bad-tagged: di luar properti (parser boleh menolak)
  fi
  timeout 60 "$NUSA_BIN" fmt fz.ns > /dev/null 2>&1 \
    || fail "fmtprop-fuzz" "fmt gagal valid: $f"
  timeout 60 "$NUSA_BIN" ast fz.ns > fafter.txt 2>/dev/null \
    || fail "fmtprop-fuzz" "ast-2 gagal valid: $f"
  cmp -s fbefore.txt fafter.txt || fail "fmtprop-fuzz" "AST berubah: $f"
  NF=$((NF + 1))
done < manifest.txt
echo "fmtprop-fuzz: $NF valid programs"
[ "$NF" -ge 10 ] || fail "fmtprop-fuzz" "terlalu sedikit valid ($NF)"
pass "fmtprop-fuzz"

echo "E2E FMTPROP PASS ($PASS_N cases)"
