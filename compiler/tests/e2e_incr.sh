#!/usr/bin/env bash
# Integration test v1.16.0 (Phase 35): incremental compilation.
#  - Build dingin → hangat (Fresh, tanpa clang ulang).
#  - Perubahan komentar → Fresh (IR identik).
#  - Perubahan logika → rebuilding + binary baru.
#  - Perubahan flag (-O) → rebuilding; --fresh memaksa.
#  - Output hilang → rebuild; modul import ikut fingerprint.
set -u
NUSA_BIN="${1:?usage: e2e_incr.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# 1. Dingin → hangat.
printf 'fn main() {\nprint(40 + 2);\n}\n' > main.ns
"$NUSA_BIN" build main.ns -o app > b1.log 2>&1 || { cat b1.log; fail "cold" "build failed"; }
grep -q "^Built " b1.log || fail "cold" "no Built message"
[ "$(./app)" = "42" ] || fail "cold" "wrong output"
"$NUSA_BIN" build main.ns -o app > b2.log 2>&1 || fail "warm" "rc != 0"
grep -q "^Fresh .* (cached)" b2.log || { cat b2.log; fail "warm" "not fresh"; }
pass "warm"

# 2. Komentar saja → Fresh.
printf '// cosmetic\n' >> main.ns
"$NUSA_BIN" build main.ns -o app > b3.log 2>&1 || fail "comment" "rc != 0"
grep -q "^Fresh " b3.log || { cat b3.log; fail "comment" "not fresh (IR should match)"; }
[ "$(./app)" = "42" ] || fail "comment" "wrong output"
pass "comment"

# 3. Logika berubah → rebuilding + binary baru.
printf 'fn main() {\nprint(40 + 3);\n}\n' > main.ns
"$NUSA_BIN" build main.ns -o app > b4.log 2>&1 || fail "change" "rc != 0"
grep -q "rebuilding: ~" b4.log || { cat b4.log; fail "change" "no rebuilding report"; }
grep -q "^Built " b4.log || fail "change" "no Built message"
[ "$(./app)" = "43" ] || fail "change" "wrong output"
pass "change"

# 4. Flag berubah → rebuilding; --fresh memaksa.
"$NUSA_BIN" build main.ns -o app -O2 > b5.log 2>&1 || fail "opt" "rc != 0"
grep -q "rebuilding: flags changed" b5.log || { cat b5.log; fail "opt" "no flags report"; }
"$NUSA_BIN" build main.ns -o app --fresh > b6.log 2>&1 || fail "fresh" "rc != 0"
grep -q "^Built " b6.log || fail "fresh" "not rebuilt"
pass "flags"

# 5. Output hilang → rebuild (walau fingerprint cocok).
rm -f app
"$NUSA_BIN" build main.ns -o app > b7.log 2>&1 || fail "relimk" "rc != 0"
grep -q "^Built " b7.log || fail "relimk" "not rebuilt"
pass "relink"

# 6. Modul import ikut fingerprint (ubah dep → rebuild).
mkdir -p libs
export NUSA_PATH="$TMPDIR/libs"
printf 'fn two() -> i64 {\nreturn 2;\n}\n' > libs/dep.ns
printf 'import dep;\nfn main() {\nprint(40 + two());\n}\n' > m2.ns
"$NUSA_BIN" build m2.ns -o m2app > m1.log 2>&1 || { cat m1.log; fail "dep-cold" "build failed"; }
[ "$(./m2app)" = "42" ] || fail "dep-cold" "wrong output"
printf '// dep touch\n' >> libs/dep.ns
"$NUSA_BIN" build m2.ns -o m2app > m2.log 2>&1 || fail "dep-touch" "rc != 0"
grep -q "^Fresh " m2.log || fail "dep-touch" "dep comment should stay fresh"
printf 'fn two() -> i64 {\nreturn 3;\n}\n' > libs/dep.ns
"$NUSA_BIN" build m2.ns -o m2app > m3.log 2>&1 || fail "dep-change" "rc != 0"
grep -q "rebuilding: ~" m3.log || { cat m3.log; fail "dep-change" "no report"; }
grep -q "dep.ns" m3.log || fail "dep-change" "dep not named"
[ "$(./m2app)" = "43" ] || fail "dep-change" "wrong output"
pass "dep"

echo "E2E INCR PASS ($PASS_N cases)"
