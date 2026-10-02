#!/usr/bin/env bash
# Integration test v1.12.0 (Phase 31): registry end-to-end.
# Semua file-backed + lokal + deterministik (tanpa network).
set -u
NUSA_BIN="${1:?usage: e2e_registry.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1
export NUSA_REGISTRY="$TMPDIR/reg"
# Hermetik Phase 80: cache registry per-suite (lihat e2e_eco2.sh).
export HOME="$TMPDIR/home"
mkdir -p "$HOME"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

expect_ok() {
  local name="$1"; shift
  "$@" > "$name.log" 2>&1 || { cat "$name.log"; fail "$name" "rc != 0"; }
  pass "$name"
}

expect_fail() {
  local name="$1" pattern="$2"; shift 2
  "$@" > "$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should fail (rc=0)"
  [ $rc -le 128 ] || fail "$name" "CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

# 1. Publish leaf package ke registry.
expect_ok lib-init "$NUSA_BIN" pkg init calc --dir libs/calc
printf 'fn double(n: i64) -> i64 {\nreturn n * 2;\n}\n' > libs/calc/calc.ns
rm -f libs/calc/main.ns
cd "$TMPDIR/libs/calc"
expect_ok publish "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
cd "$TMPDIR"
[ -f reg/index/calc.toml ] || fail "publish" "index missing"
[ -f reg/packages/calc-0.1.0.tar.gz ] || fail "publish" "tarball missing"
grep -q "tarball = " reg/index/calc.toml || fail "publish" "tarball hash missing"

# 1b. Simpan sumber 0.1.0 pristine untuk pemulihan kasus 6.
mkdir -p pristine && cp libs/calc/nusa.toml libs/calc/calc.ns pristine/
# 2. Search menemukan + miss jelas.
expect_ok search "$NUSA_BIN" search calc
grep -q "calc (0.1.0)" search.log || fail "search" "version line"
expect_ok search-miss "$NUSA_BIN" search zzzqqq
grep -q "no packages matching" search-miss.log || fail "search-miss" "message"

# 3. Install + build + run via registry (cache terisi).
expect_ok app-init "$NUSA_BIN" pkg init app --dir app
cd "$TMPDIR/app"
expect_ok install "$NUSA_BIN" install calc
grep -q 'calc = "\^0.1.0"' nusa.toml || fail "install" "caret req"
grep -q 'registry = ' nusa.lock || fail "install" "lock registry"
printf 'import calc;\nfn main() {\nprint(double(21));\n}\n' > main.ns
expect_ok build "$NUSA_BIN" pkg build
[ "$(./target/app)" = "42" ] || fail "build" "wrong output"
cd "$TMPDIR"

# 4b. Republish identik idempoten; konten-beda ditolak.
cd "$TMPDIR/libs/calc"
expect_ok repub "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
printf '\n// vandal\n' >> calc.ns
expect_fail repub-conflict "already published in" "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
# kembalikan isi + naikkan versi (lanjut kasus 4).
printf 'fn double(n: i64) -> i64 {\nreturn n * 2;\n}\nfn triple(n: i64) -> i64 {\nreturn n * 3;\n}\n' > calc.ns
cd "$TMPDIR"
# 4. Versi kedua + caret memilih maksimum yang memenuhi.
printf '[package]\nname = "calc"\nversion = "0.2.0"\n\n[dependencies]\n' > libs/calc/nusa.toml
rm -f libs/calc/nusa.lock
cd "$TMPDIR/libs/calc"
expect_ok pub2 "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
cd "$TMPDIR"
expect_ok search2 "$NUSA_BIN" search calc
grep -q "0.1.0, 0.2.0" search2.log || fail "search2" "both versions"
# 4c. Patch 0.1.5 untuk uji maksimum caret.
printf '[package]\nname = "calc"\nversion = "0.1.5"\n\n[dependencies]\n' > libs/calc/nusa.toml
rm -f libs/calc/nusa.lock
cd "$TMPDIR/libs/calc"
expect_ok pub3 "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
cd "$TMPDIR"

# 5. Caret memilih maksimum yang memenuhi; exact-miss jelas.
rm -rf app2 && expect_ok app2-init "$NUSA_BIN" pkg init app2 --dir app2
printf 'fn main() {\nprint(1);\n}\n' > app2/main.ns
cd "$TMPDIR/app2"
expect_ok install-caret "$NUSA_BIN" install 'calc@^0.1.0'
grep -q 'calc = "\^0.1.0"' nusa.toml || fail "install-caret" "req kept"
grep -q 'version = "0.1.5"' nusa.lock || fail "install-caret" "max satisfying"
cd "$TMPDIR"
cd "$TMPDIR/app2"
expect_fail install-exact-miss "no version of 'calc' satisfies '8.0.0'" "$NUSA_BIN" install 'calc@8.0.0'
cd "$TMPDIR"

# 6. Tamper tarball terdeteksi; cache tamper sembuh dari registry.
echo "evil" >> reg/packages/calc-0.1.0.tar.gz
rm -rf "$HOME/.cache/nusa"
cd "$TMPDIR/app"
expect_fail tamper "tarball checksum mismatch" "$NUSA_BIN" pkg build
cd "$TMPDIR"
# pulihkan registry dari sumber pristine
rm reg/packages/calc-0.1.0.tar.gz
cp pristine/nusa.toml pristine/calc.ns libs/calc/
rm -f libs/calc/nusa.lock
cd "$TMPDIR/libs/calc"
expect_ok repub3 "$NUSA_BIN" pkg publish --registry "$TMPDIR/reg"
cd "$TMPDIR"
# Isi ulang cache (build sukses), lalu tamper isi ter-unpack.
cd "$TMPDIR/app"
expect_ok refill "$NUSA_BIN" pkg build
[ "$(./target/app)" = "42" ] || fail "refill" "wrong output"
cd "$TMPDIR"
CACHE_PKG=$(find "$HOME/.cache/nusa" -path "*calc/0.1.0/pkg/calc.ns" 2>/dev/null | head -n 1)
[ -n "$CACHE_PKG" ] || fail "heal" "cache file missing"
echo "// evil" >> "$CACHE_PKG"
cd "$TMPDIR/app"
expect_ok heal "$NUSA_BIN" pkg build
[ "$(./target/app)" = "42" ] || fail "heal" "wrong output after heal"
cd "$TMPDIR"

echo "E2E REGISTRY PASS ($PASS_N cases)"
