#!/usr/bin/env bash
# Build-system conformance v2.13.0 (Phase 62, milestone 1).
# `clean` (artefak saja, idempoten) + `--profile` (dev/debug/release,
# eksplisit menang, asing ditolak) untuk build + pkg build.
set -u
NUSA_BIN="${1:?usage: e2e_buildsys.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== clean =="
mkdir -p proj/sub proj/sub/target
printf 'fn main() {\n    print(1);\n}\n' > proj/main.ns
printf 'keep\n' > proj/KEEP.txt
printf 'ir-stub\n' > proj/main.ns.ll
printf 'ir-stub\n' > proj/sub/y.ns.ll
mkdir -p proj/.nusa-cache proj/target proj/dist proj/sub/.nusa-cache
printf 'x\n' > proj/.nusa-cache/f
printf 'x\n' > proj/target/bin
printf 'x\n' > proj/dist/t.tgz
printf 'x\n' > proj/sub/target/keep
printf 'x\n' > proj/sub/.nusa-cache/g
"$NUSA_BIN" clean proj > clean.log 2>&1 || { cat clean.log; fail "clean" "rc != 0"; }
grep -q "removed)" clean.log || { cat clean.log; fail "clean" "ringkasan hilang"; }
[ ! -e proj/main.ns.ll ] || fail "clean" "*.ns.ll tak terhapus"
[ ! -e proj/sub/y.ns.ll ] || fail "clean" "rekursif gagal"
[ ! -d proj/.nusa-cache ] || fail "clean" ".nusa-cache tak terhapus"
[ ! -d proj/sub/.nusa-cache ] || fail "clean" ".nusa-cache nested gagal"
[ ! -e proj/target/bin ] || fail "clean" "target/ tak terhapus"
[ ! -e proj/dist/t.tgz ] || fail "clean" "dist/ tak terhapus"
[ -f proj/sub/target/keep ] || fail "clean" "sub/target ikut terhapus!"
[ -f proj/KEEP.txt ] || fail "clean" "sumber ikut terhapus!"
[ -f proj/main.ns ] || fail "clean" "sumber ikut terhapus!"
pass "clean"
"$NUSA_BIN" clean proj > clean2.log 2>&1 || { cat clean2.log; fail "clean-idem" "rc != 0"; }
grep -q "(0 removed)" clean2.log || { cat clean2.log; fail "clean-idem" "bukan 0"; }
pass "clean-idem"
"$NUSA_BIN" clean no-such-dir > clean3.log 2>&1
rc=$?
if crashed "$rc"; then cat clean3.log; fail "clean-bad" "CRASH"; fi
[ $rc -ne 0 ] || fail "clean-bad" "dir hilang harus gagal"
pass "clean-bad"

echo "== profiles =="
printf 'fn main() {\n    print(6 * 7);\n}\n' > prof.ns
"$NUSA_BIN" build prof.ns --profile dev -o prof_dev >bd.log 2>&1 || { cat bd.log; fail "prof-dev" "build gagal"; }
[ "$(./prof_dev)" = "42" ] || fail "prof-dev" "run salah"
pass "prof-dev"
"$NUSA_BIN" build prof.ns --profile release -o prof_rel >br.log 2>&1 || { cat br.log; fail "prof-release" "build gagal"; }
[ "$(./prof_rel)" = "42" ] || fail "prof-release" "run salah"
pass "prof-release"
"$NUSA_BIN" build prof.ns --profile debug --emit-ir -o prof_dbg.ll >bg.log 2>&1 || { cat bg.log; fail "prof-debug" "build gagal"; }
grep -q "DICompileUnit" prof_dbg.ll || fail "prof-debug" "DWARF hilang"
pass "prof-debug"
"$NUSA_BIN" build prof.ns --emit-ir -o prof_nodebug.ll >bn.log 2>&1 || { cat bn.log; fail "prof-nodebug" "build gagal"; }
if grep -q "DICompileUnit" prof_nodebug.ll; then fail "prof-nodebug" "default harus tanpa DWARF"; fi
pass "prof-nodebug"
# Eksplisit menang (dua urutan) + asing ditolak.
"$NUSA_BIN" build prof.ns --profile release -g --emit-ir -o prof_mix.ll >/dev/null 2>&1 || fail "prof-mix" "build gagal"
grep -q "DICompileUnit" prof_mix.ll || fail "prof-mix" "-g eksplisit kalah"
"$NUSA_BIN" build prof.ns -O2 --profile dev -o prof_mix2 >/dev/null 2>&1 || fail "prof-mix2" "build gagal"
[ "$(./prof_mix2)" = "42" ] || fail "prof-mix2" "run salah"
pass "prof-mix"
"$NUSA_BIN" build prof.ns --profile fast -o prof_nope >bp.log 2>&1
rc=$?
if crashed "$rc"; then cat bp.log; fail "prof-bad" "CRASH"; fi
[ $rc -ne 0 ] || fail "prof-bad" "profil asing harus gagal"
grep -q "unknown profile" bp.log || { cat bp.log; fail "prof-bad" "pesan hilang"; }
pass "prof-bad"

echo "== pkg build --profile =="
"$NUSA_BIN" pkg init profapp --dir profapp >/dev/null 2>&1 || fail "prof-pkg" "init gagal"
cd profapp || exit 1
"$NUSA_BIN" pkg build --profile release >pb.log 2>&1 || { cat pb.log; fail "prof-pkg" "build gagal"; }
[ "$(./target/profapp)" = "Hello, profapp" ] || fail "prof-pkg" "run salah"
pass "prof-pkg"
cd "$TMPDIR" || exit 1

echo "E2E BUILDSYS PASS ($PASS_N cases)"
