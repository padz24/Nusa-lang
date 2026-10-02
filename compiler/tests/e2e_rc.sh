#!/usr/bin/env bash
# RC gate v3.0.0-rc.1 (Phase 88): baner RC + freeze utuh +
# build SELURUH korpus runnable + spot-run + IR + bersih-artefak.
# NOL perubahan bahasa/arsitektur/API/format (beku sejak beta);
# perubahan = label versi + harness + docs + record rilis saja.
# Phase 89 (final): terima juga baner final 3.0.0 + penanda
# v3.0.0/Phase 89 sebagai regresi; gate final ketat di e2e_release.sh.
set -u
NUSA_BIN="${1:?usage: e2e_rc.sh <nusa-binary>}"
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

# 1. Baner RC (ketat di eranya; Phase 89: terima rc.1 ATAU final).
VER="$("$NUSA_BIN" --version 2>&1)" || fail "rc-version" "rc != 0"
echo "$VER" | grep -Eq "^nusa version 3\.0\.0(-rc\.1)? " \
  || fail "rc-version" "banner bukan rc.1/final: $VER"
"$NUSA_BIN" --help 2>&1 | grep -Eq "RC|Stable" \
  || fail "rc-version" "help tak menyebut RC/Stable"
if "$NUSA_BIN" --help 2>&1 | grep -q "Beta"; then
  fail "rc-version" "help masih menyebut Beta"
fi
pass "rc-version"

# 2. Freeze utuh: spec final + record rilis + penanda versi.
SPEC="$ROOT/docs/language-spec-3.0.md"
[ -f "$SPEC" ] || fail "rc-freeze" "missing docs/language-spec-3.0.md"
[ -f "$ROOT/docs/release-3.0-rc.1.md" ] || [ -f "$ROOT/docs/release-3.0.md" ] \
  || fail "rc-freeze" "missing release record (rc.1/final)"
grep -q "Status: \*\*normative\*\*" "$SPEC" || fail "rc-freeze" "spec tak normatif"
grep -q "^## 1\. Lexical grammar\$" "$SPEC" || fail "rc-freeze" "spec §1 hilang"
grep -q "^## 19\. Platform semantics\$" "$SPEC" || fail "rc-freeze" "spec §19 hilang"
grep -Eq "v3\.0\.0(-rc\.1)?" "$ROOT/docs/standard-library.md" || fail "rc-freeze" "stdlib header bukan 3.0"
grep -Eq "Phase 8[89]" "$ROOT/docs/compiler.md" || fail "rc-freeze" "compiler.md tanpa Phase 88/89"
pass "rc-freeze"

# 3. Build-all korpus runnable (ambang sama dengan beta).
N_BUILD=0
N_LIB=0
i=0
while IFS= read -r f; do
  rel="${f#$ROOT/}"
  if timeout 60 "$NUSA_BIN" check "$f" >"c.log" 2>&1; then
    i=$((i + 1))
    timeout 180 "$NUSA_BIN" build "$f" -o "rc$i" >"rc$i.log" 2>&1 \
      || { cat "rc$i.log"; fail "rc-build" "build gagal: $rel"; }
    [ -x "rc$i" ] || fail "rc-build" "exe hilang: $rel"
    N_BUILD=$((N_BUILD + 1))
  elif grep -q "no 'fn main' found" c.log; then
    N_LIB=$((N_LIB + 1))
  else
    echo "--- $rel ---"; cat c.log; fail "rc-build" "check gagal di luar pola"
  fi
done < <(find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" \
              "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns" | sort)
echo "rc-built=$N_BUILD rc-lib=$N_LIB"
[ "$N_BUILD" -ge 40 ] || fail "rc-build" "terlalu sedikit ter-build ($N_BUILD)"
[ $((N_BUILD + N_LIB)) -ge 80 ] || fail "rc-build" "korpus menyusut"
pass "rc-build"

# 4. Spot-run golden + verifikasi LLVM IR (@main).
cp "$ROOT/examples/hello.ns" ./hello.ns || fail "rc-run" "copy hello"
timeout 60 "$NUSA_BIN" run ./hello.ns >hello.got 2>hello.err \
  || { cat hello.err; fail "rc-run" "hello run failed"; }
printf 'Hello, Nusa!\n' | cmp -s hello.got - \
  || { cat hello.got; fail "rc-run" "hello output berubah"; }
[ -f ./hello.ns.ll ] || fail "rc-run" "IR hello.ns.ll hilang"
grep -q "@main" ./hello.ns.ll || fail "rc-run" "IR tanpa @main"
pass "rc-run"

# 5. Bersihkan .ns.ll kelahiran uji (snapshot-diff).
find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" \
     "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns.ll" \
     2>/dev/null | sort > post_ll.txt
comm -13 pre_ll.txt post_ll.txt | while IFS= read -r f; do rm -f "$f"; done
rm -f ./hello.ns.ll
pass "rc-clean"

echo "E2E RC PASS ($PASS_N cases)"
