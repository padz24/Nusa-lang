#!/usr/bin/env bash
# Beta gate v3.0.0-beta (Phase 84): baner versi + build SELURUH
# korpus runnable (real-world-build: tiap program check-hijau WAJIB
# ter-build) + spot-run. Artefak .ns.ll yang lahir di pohon sumber
# dibersihkan (snapshot-diff, bukan hapus-buta).
# Phase 88 (RC1): baner diterima beta ATAU rc.* agar regresi
# build-all tetap hijau pasca-bump label; gate RC spesifik di e2e_rc.sh.
# Phase 89 (final): terima juga 3.0.0 final tanpa suffix; gate
# final ketat di e2e_release.sh.
set -u
NUSA_BIN="${1:?usage: e2e_beta.sh <nusa-binary>}"
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

find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" \
     "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns.ll" \
     2>/dev/null | sort > pre_ll.txt

# 1. Baner rilis beta (Phase 88: terima beta/rc.*; Phase 89:
# terima juga final 3.0.0 tanpa suffix sebagai regresi).
VER="$("$NUSA_BIN" --version 2>&1)" || fail "beta-version" "rc != 0"
echo "$VER" | grep -Eq "^nusa version 3\.0\.0(-(beta|rc\.[0-9]+))? " \
  || fail "beta-version" "banner tak-3.0: $VER"
"$NUSA_BIN" --help 2>&1 | grep -Eq "Beta|RC|Stable" \
  || fail "beta-version" "help tak menyebut Beta/RC/Stable"
pass "beta-version"

# 2. Build-all korpus runnable.
N_BUILD=0
N_LIB=0
i=0
while IFS= read -r f; do
  rel="${f#$ROOT/}"
  if timeout 60 "$NUSA_BIN" check "$f" >"c.log" 2>&1; then
    i=$((i + 1))
    timeout 180 "$NUSA_BIN" build "$f" -o "beta$i" >"beta$i.log" 2>&1 \
      || { cat "beta$i.log"; fail "beta-build" "build gagal: $rel"; }
    [ -x "beta$i" ] || fail "beta-build" "exe hilang: $rel"
    N_BUILD=$((N_BUILD + 1))
  elif grep -q "no 'fn main' found" c.log; then
    N_LIB=$((N_LIB + 1))
  else
    echo "--- $rel ---"; cat c.log; fail "beta-build" "check gagal di luar pola"
  fi
done < <(find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" \
              "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns" | sort)
echo "beta-built=$N_BUILD beta-lib=$N_LIB"
[ "$N_BUILD" -ge 40 ] || fail "beta-build" "terlalu sedikit ter-build ($N_BUILD)"
[ $((N_BUILD + N_LIB)) -ge 80 ] || fail "beta-build" "korpus menyusut"
pass "beta-build"

# 3. Spot-run golden (disalin ke TMPDIR agar repo bersih).
cp "$ROOT/examples/hello.ns" ./hello.ns || fail "beta-run" "copy hello"
timeout 60 "$NUSA_BIN" run ./hello.ns >hello.got 2>hello.err \
  || { cat hello.err; fail "beta-run" "hello run failed"; }
printf 'Hello, Nusa!\n' | cmp -s hello.got - \
  || { cat hello.got; fail "beta-run" "hello output berubah"; }
pass "beta-run"

# 4. Bersihkan .ns.ll kelahiran uji (snapshot-diff).
find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" \
     "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns.ll" \
     2>/dev/null | sort > post_ll.txt
comm -13 pre_ll.txt post_ll.txt | while IFS= read -r f; do rm -f "$f"; done
pass "beta-clean"

echo "E2E BETA PASS ($PASS_N cases)"
