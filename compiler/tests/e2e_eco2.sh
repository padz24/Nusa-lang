#!/usr/bin/env bash
# Ecosystem conformance v2.11.0 (Phase 60, milestone 1).
# Perintah read-only: tree / verify / outdated / audit
# (top-level + `pkg`). Deterministik, file-backed, lokal.
set -u
NUSA_BIN="${1:?usage: e2e_eco2.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

expect_ok() {
  local name="$1"; shift
  "$@" >"$name.log" 2>&1 || { cat "$name.log"; fail "$name" "rc != 0"; }
  pass "$name"
}

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"; rm -rf "$HOME/.cache/nusa"' EXIT
cd "$TMPDIR" || exit 1
export NUSA_REGISTRY="$TMPDIR/reg"
# Hermetik Phase 80: cache registry per-suite (flake paralel
# antar-suite via $HOME/.cache bersama).
export HOME="$TMPDIR/home"
mkdir -p "$HOME"
rm -rf "$HOME/.cache/nusa"

echo "== fixture: path-dep + registry 2 versi =="
expect_ok lib-init "$NUSA_BIN" pkg init eco2lib --dir libs/eco2lib
printf 'fn double(n: i64) -> i64 {\n    return n * 2;\n}\n' > libs/eco2lib/eco2lib.ns
rm -f libs/eco2lib/main.ns
sed -i 's/^network = false$/network = true/' libs/eco2lib/nusa.toml
grep -q "^network = true$" libs/eco2lib/nusa.toml || fail "lib-caps" "sed gagal"
expect_ok reglib-init "$NUSA_BIN" pkg init eco2calc --dir reglib
printf 'fn triple(n: i64) -> i64 {\n    return n * 3;\n}\n' > reglib/eco2calc.ns
rm -f reglib/main.ns
cd reglib || exit 1
expect_ok pub1 "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
printf '[package]\nname = "eco2calc"\nversion = "0.2.0"\ncompiler = "2.11.0"\n' > nusa.toml
printf 'fn triple(n: i64) -> i64 {\n    return n * 3;\n}\nfn quad(n: i64) -> i64 {\n    return n * 4;\n}\n' > eco2calc.ns
expect_ok pub2 "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
cd "$TMPDIR" || exit 1
expect_ok app-init "$NUSA_BIN" pkg init eco2app --dir app
cd app || exit 1
expect_ok app-add "$NUSA_BIN" pkg add eco2lib --path ../libs/eco2lib
expect_ok app-install "$NUSA_BIN" install 'eco2calc@0.1.0'
cd "$TMPDIR" || exit 1

echo "== tree =="
cd app || exit 1
"$NUSA_BIN" tree > tree.log 2>&1 || { cat tree.log; fail "tree" "rc != 0"; }
grep -q "eco2app" tree.log || { cat tree.log; fail "tree" "root hilang"; }
grep -q "eco2lib.*(path)" tree.log || { cat tree.log; fail "tree" "path-dep hilang"; }
grep -q "eco2calc.*0.1.0.*(registry)" tree.log || { cat tree.log; fail "tree" "registry-dep hilang"; }
pass "tree"
cd "$TMPDIR" || exit 1

echo "== verify OK lalu FAIL pasca-tamper =="
cd app || exit 1
"$NUSA_BIN" pkg verify > verify1.log 2>&1 || { cat verify1.log; fail "verify-ok" "rc != 0"; }
grep -q "verify: 0 failed" verify1.log || { cat verify1.log; fail "verify-ok" "ringkasan salah"; }
pass "verify-ok"
CACHE_NS=$(find "$HOME/.cache/nusa" -path "*eco2calc/0.1.0/pkg/eco2calc.ns" 2>/dev/null | head -n 1)
[ -n "$CACHE_NS" ] || fail "verify-tamper" "cache tak ketemu"
printf '\nfn evil() -> i64 {\n    return 0;\n}\n' >> "$CACHE_NS"
"$NUSA_BIN" verify > verify2.log 2>&1
rc=$?
if crashed "$rc"; then cat verify2.log; fail "verify-fail" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "verify-fail" "tamper harus FAIL"
grep -q "FAIL eco2calc" verify2.log || { cat verify2.log; fail "verify-fail" "baris FAIL hilang"; }
pass "verify-fail"
# Pulihkan cache agar kasus berikut deterministik.
rm -rf "$HOME/.cache/nusa"
"$NUSA_BIN" pkg update >/dev/null 2>&1
"$NUSA_BIN" verify > verify3.log 2>&1 || { cat verify3.log; fail "verify-heal" "rc != 0 pasca update"; }
grep -q "verify: 0 failed" verify3.log || { cat verify3.log; fail "verify-heal" "tak sembuh"; }
pass "verify-heal"
cd "$TMPDIR" || exit 1

echo "== outdated + audit =="
cd app || exit 1
"$NUSA_BIN" outdated > outdated.log 2>&1 || { cat outdated.log; fail "outdated" "rc != 0"; }
grep -q "eco2calc.*0.1.0 -> latest 0.2.0" outdated.log || { cat outdated.log; fail "outdated" "0.2.0 tak terdeteksi"; }
pass "outdated"
"$NUSA_BIN" audit > audit.log 2>&1 || { cat audit.log; fail "audit" "rc != 0"; }
grep -q "eco2app" audit.log || { cat audit.log; fail "audit" "root hilang"; }
grep -q "network" audit.log || { cat audit.log; fail "audit" "capability hilang"; }
grep -q "signatures: unavailable" audit.log || { cat audit.log; fail "audit" "catatan signing hilang"; }
pass "audit"
cd "$TMPDIR" || exit 1
rm -rf "$HOME/.cache/nusa"

echo "E2E ECO2 PASS ($PASS_N cases)"
