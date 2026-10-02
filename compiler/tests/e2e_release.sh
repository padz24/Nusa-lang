#!/usr/bin/env bash
# Release gate v3.0.0 final (Phase 89): baner final eksak + DoD
# artefak (§44–45 master) + build SELURUH korpus + spot-run +
# IR + bersih-artefak. NOL perubahan bahasa/arsitektur/API/format;
# delta rc.1 → final = hapus suffix + record + changelog + regen.
set -u
NUSA_BIN="${1:?usage: e2e_release.sh <nusa-binary>}"
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

# 1. Baner final eksak (tanpa suffix pra-rilis).
VER="$("$NUSA_BIN" --version 2>&1)" || fail "rel-version" "rc != 0"
echo "$VER" | grep -q "^nusa version 3\.0\.0 " \
  || fail "rel-version" "banner bukan final: $VER"
if echo "$VER" | grep -Eq "\-beta|\-rc"; then
  fail "rel-version" "banner masih pra-rilis: $VER"
fi
"$NUSA_BIN" --help 2>&1 | grep -q "Stable" \
  || fail "rel-version" "help tak menyebut Stable"
if "$NUSA_BIN" --help 2>&1 | grep -Eq "Beta|— RC|-rc"; then
  fail "rel-version" "help masih pra-rilis"
fi
pass "rel-version"

# 2. DoD artefak §44: tiap deliverable WAJIB ada sebagai berkas.
for f in docs/language-spec-3.0.md docs/migration-3.0.md \
         docs/security-3.0.md docs/perf-3.0.md docs/realworld-3.0.md \
         docs/release-3.0.md docs/bootstrap.md docs/bootstrap-2.24.md \
         docs/bootstrap-full-2.25.md docs/debugging.md docs/debugging-2.15.md \
         docs/lsp.md docs/ffi.md docs/pkg.md docs/freeze-3.0.md \
         docs/targets.md bench/suite.sh CHANGELOG.md; do
  [ -f "$ROOT/$f" ] || fail "rel-dod" "missing $f"
done
grep -q "Status: \*\*normative\*\*" "$ROOT/docs/language-spec-3.0.md" \
  || fail "rel-dod" "spec tak normatif"
grep -q "^## \[3\.0\.0\]" "$ROOT/CHANGELOG.md" \
  || fail "rel-dod" "CHANGELOG tanpa [3.0.0]"
grep -q "^# Nusa Standard Library — v3\.0\.0$" "$ROOT/docs/standard-library.md" \
  || fail "rel-dod" "stdlib header bukan v3.0.0 final"
grep -q "Phase 89" "$ROOT/docs/compiler.md" \
  || fail "rel-dod" "compiler.md tanpa Phase 89"
pass "rel-dod"

# 3. DoD fungsional: tooling §44 hidup (LSP/fmt/test/pkg/migrate/new).
HELP="$("$NUSA_BIN" --help 2>&1)" || fail "rel-tools" "help rc != 0"
for c in "nusa lsp" "nusa fmt" "nusa test" "nusa pkg" "nusa migrate" "nusa bench"; do
  echo "$HELP" | grep -q "$c" || fail "rel-tools" "help tanpa: $c"
done
"$NUSA_BIN" new --list 2>/dev/null | grep -q "hello-world" \
  || fail "rel-tools" "templates hilang"
"$NUSA_BIN" migrate "$ROOT/examples" >mig.log 2>&1 \
  || fail "rel-tools" "migrate examples rc != 0"
grep -q "compatible" mig.log || fail "rel-tools" "migrate tanpa ringkasan"
pass "rel-tools"

# 4. Build-all korpus runnable (ambang beta/RC).
N_BUILD=0
N_LIB=0
i=0
while IFS= read -r f; do
  rel="${f#$ROOT/}"
  if timeout 60 "$NUSA_BIN" check "$f" >"c.log" 2>&1; then
    i=$((i + 1))
    timeout 180 "$NUSA_BIN" build "$f" -o "rel$i" >"rel$i.log" 2>&1 \
      || { cat "rel$i.log"; fail "rel-build" "build gagal: $rel"; }
    [ -x "rel$i" ] || fail "rel-build" "exe hilang: $rel"
    N_BUILD=$((N_BUILD + 1))
  elif grep -q "no 'fn main' found" c.log; then
    N_LIB=$((N_LIB + 1))
  else
    echo "--- $rel ---"; cat c.log; fail "rel-build" "check gagal di luar pola"
  fi
done < <(find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" \
              "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns" | sort)
echo "rel-built=$N_BUILD rel-lib=$N_LIB"
[ "$N_BUILD" -ge 40 ] || fail "rel-build" "terlalu sedikit ter-build ($N_BUILD)"
[ $((N_BUILD + N_LIB)) -ge 80 ] || fail "rel-build" "korpus menyusut"
pass "rel-build"

# 5. Spot-run golden + verifikasi LLVM IR (@main).
cp "$ROOT/examples/hello.ns" ./hello.ns || fail "rel-run" "copy hello"
timeout 60 "$NUSA_BIN" run ./hello.ns >hello.got 2>hello.err \
  || { cat hello.err; fail "rel-run" "hello run failed"; }
printf 'Hello, Nusa!\n' | cmp -s hello.got - \
  || { cat hello.got; fail "rel-run" "hello output berubah"; }
[ -f ./hello.ns.ll ] || fail "rel-run" "IR hello.ns.ll hilang"
grep -q "@main" ./hello.ns.ll || fail "rel-run" "IR tanpa @main"
pass "rel-run"

# 6. Bersihkan .ns.ll kelahiran uji (snapshot-diff).
find "$ROOT/std" "$ROOT/examples" "$ROOT/tutorials" "$ROOT/bench" \
     "$ROOT/selfhost" "$ROOT/tests" "$ROOT/packages" "$ROOT/realworld" -name "*.ns.ll" \
     2>/dev/null | sort > post_ll.txt
comm -13 pre_ll.txt post_ll.txt | while IFS= read -r f; do rm -f "$f"; done
rm -f ./hello.ns.ll
pass "rel-clean"

echo "E2E RELEASE PASS ($PASS_N cases)"
