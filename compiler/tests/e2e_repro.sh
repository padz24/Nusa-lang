#!/usr/bin/env bash
# Reproducible-build conformance v2.12.0 (Phase 61, milestone 1).
# - Tarball publish byte-identik antar-kemas.
# - Lock mem-pin compiler eksak; pin salah/usang → build ditolak.
# - Build `--fresh` ganda byte-identik (path sama).
set -u
NUSA_BIN="${1:?usage: e2e_repro.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== tarball deterministik =="
"$NUSA_BIN" pkg init reprolib --dir reprolib >/dev/null 2>&1 || fail "repro-pub" "init gagal"
cd reprolib || exit 1
"$NUSA_BIN" pkg publish --dry-run >pub1.log 2>&1 || { cat pub1.log; fail "repro-pub" "publish 1 gagal"; }
TB=$(ls dist/*.tar.gz) || fail "repro-pub" "tarball hilang"
sha256sum "$TB" | awk '{print $1}' > ../sum1.txt
sleep 2 # lewati batas mtime bila tak dinormalisasi
"$NUSA_BIN" pkg publish --dry-run >pub2.log 2>&1 || { cat pub2.log; fail "repro-pub" "publish 2 gagal"; }
sha256sum "$TB" | awk '{print $1}' > ../sum2.txt
cmp -s ../sum1.txt ../sum2.txt || { cat ../sum1.txt ../sum2.txt; fail "repro-pub" "tarball tak byte-identik"; }
pass "repro-pub"
cd "$TMPDIR" || exit 1

echo "== pin compiler di lock =="
"$NUSA_BIN" pkg init reproapp --dir reproapp >/dev/null 2>&1 || fail "repro-pin" "init gagal"
cd reproapp || exit 1
"$NUSA_BIN" pkg update >upd.log 2>&1 || { cat upd.log; fail "repro-pin" "update gagal"; }
grep -q '^\[meta\]' nusa.lock || { cat nusa.lock; fail "repro-pin" "[meta] hilang"; }
grep -q "compiler = \"$("$NUSA_BIN" --version | awk '{print $3}')\"" nusa.lock || { cat nusa.lock; fail "repro-pin" "pin tak cocok versi biner"; }
pass "repro-pin-meta"
sed -i 's/^compiler = ".*/compiler = "0.0.0"/' nusa.lock
"$NUSA_BIN" pkg build >buildbad.log 2>&1
rc=$?
if crashed "$rc"; then cat buildbad.log; fail "repro-pin-deny" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "repro-pin-deny" "pin salah harus ditolak"
grep -q "compiler" buildbad.log || { cat buildbad.log; fail "repro-pin-deny" "pesan compiler hilang"; }
pass "repro-pin-deny"
"$NUSA_BIN" pkg update >/dev/null 2>&1 || fail "repro-pin-heal" "update gagal"
"$NUSA_BIN" pkg build >buildok.log 2>&1 || { cat buildok.log; fail "repro-pin-heal" "build pasca-update gagal"; }
pass "repro-pin-heal"
cd "$TMPDIR" || exit 1

echo "== build ulang byte-identik =="
cd reproapp || exit 1
"$NUSA_BIN" pkg build --fresh -o rb1 >rb1.log 2>&1 || { cat rb1.log; fail "repro-build" "build 1 gagal"; }
"$NUSA_BIN" pkg build --fresh -o rb2 >rb2.log 2>&1 || { cat rb2.log; fail "repro-build" "build 2 gagal"; }
cmp rb1 rb2 || fail "repro-build" "biner tak byte-identik"
[ "$(./rb1)" = "$(./rb2)" ] || fail "repro-build" "run beda"
pass "repro-build"
cd "$TMPDIR" || exit 1

echo "E2E REPRO PASS ($PASS_N cases)"
