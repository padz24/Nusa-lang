#!/usr/bin/env bash
# Supply-chain conformance v3.15.0 (Phase 114, milestone 1): preseden
# registry terlihat (anti-confusion) + build beku tanpa-fetch.
# Tanpa kriptografi baru (signatures = Phase 113-deferred).
set -u
NUSA_BIN="${1:?usage: e2e_supply.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"; rm -rf "$HOME/.cache/nusa"' EXIT
cd "$TMPDIR" || exit 1
export HOME="$TMPDIR/home"
mkdir -p "$HOME"
rm -rf "$HOME/.cache/nusa"

# Fixture app + satu registry-dep.
"$NUSA_BIN" pkg init suplib --dir suplib > /dev/null \
  || fail "supply-fixture" "lib init"
printf 'fn triple(n: i64) -> i64 {\n    return n * 3;\n}\n' > suplib/suplib.ns
rm -f suplib/main.ns
cd suplib || exit 1
"$NUSA_BIN" pkg publish --registry "$TMPDIR/reg" > /dev/null \
  || fail "supply-fixture" "publish"
cd "$TMPDIR" || exit 1
"$NUSA_BIN" pkg init supapp --dir app > /dev/null \
  || fail "supply-fixture" "app init"
printf 'fn main() {\n    print(1);\n}\n' > app/main.ns
cd app || exit 1
export NUSA_REGISTRY="$TMPDIR/reg"
"$NUSA_BIN" install 'suplib@0.1.0' > /dev/null \
  || fail "supply-fixture" "install"
[ -f nusa.lock ] || fail "supply-fixture" "tanpa lock"

# 1. --frozen: lock segar -> build jalan.
timeout 180 "$NUSA_BIN" pkg build --frozen >fr1.log 2>&1 \
  || { cat fr1.log; fail "frozen-ok" "rc != 0 padahal segar"; }
pass "frozen-ok"

# 2. --frozen tanpa lock -> tolak tanpa fetch.
rm -f nusa.lock
if timeout 120 "$NUSA_BIN" pkg build --frozen >fr2.log 2>&1; then
  fail "frozen-nolock" "rc=0 tanpa lock"
fi
grep -q "refusing to fetch" fr2.log \
  || { cat fr2.log; fail "frozen-nolock" "pesan tak jelas"; }
[ -f nusa.lock ] && fail "frozen-nolock" "lock tercipta (fetch terjadi)"
pass "frozen-nolock"

# 3. --frozen + lock basi -> tolak (tanpa update diam-diam).
timeout 120 "$NUSA_BIN" pkg update > /dev/null 2>&1 \
  || fail "frozen-stale" "update"
printf '[package]\nname = "supapp"\nversion = "0.2.0"\ncompiler = "3.0.0"\n' > nusa.toml
if timeout 120 "$NUSA_BIN" pkg build --frozen >fr3.log 2>&1; then
  fail "frozen-stale" "rc=0 dengan lock basi"
fi
grep -q "lock out of date" fr3.log \
  || { cat fr3.log; fail "frozen-stale" "bukan pesan lock"; }
pass "frozen-stale"

# 4. Shadowing antar-registry: terlihat, pertama menang.
cd "$TMPDIR" || exit 1
"$NUSA_BIN" pkg init suplib --dir shadowa > /dev/null \
  || fail "supply-shadow" "libA init"
printf 'fn triple(n: i64) -> i64 {\n    return n * 100;\n}\n' > shadowa/suplib.ns
rm -f shadowa/main.ns
printf '[package]\nname = "suplib"\nversion = "0.1.0"\ncompiler = "3.0.0"\n' > shadowa/nusa.toml
cd shadowa || exit 1
export NUSA_REGISTRY="$TMPDIR/regB" # hanya B (publish mendorong semua registry di env)
"$NUSA_BIN" pkg publish --registry "$TMPDIR/regB" > /dev/null \
  || fail "supply-shadow" "publish B"
cd "$TMPDIR" || exit 1
"$NUSA_BIN" pkg init supshadow --dir shapp > /dev/null \
  || fail "supply-shadow" "app init"
printf 'fn main() {\n    print(1);\n}\n' > shapp/main.ns
cd shapp || exit 1
export NUSA_REGISTRY="$TMPDIR/reg:$TMPDIR/regB"
"$NUSA_BIN" install 'suplib@0.1.0' > shadow.log 2>&1 \
  || { cat shadow.log; fail "supply-shadow" "install"; }
grep -q "also in registry" shadow.log \
  || { cat shadow.log; fail "supply-shadow" "shadowing senyap"; }
grep -q "$TMPDIR/regB" shadow.log \
  || { cat shadow.log; fail "supply-shadow" "registry kedua tak disebut"; }
grep -q "using" shadow.log \
  || { cat shadow.log; fail "supply-shadow" "pemenang tak disebut"; }
# Pemenang = registry pertama (reg): checksum lock == index reg.
PIN_CSUM=$(awk '/^\[\[package\]\]/{hit=0} /name = "suplib"/{hit=1} hit && /checksum =/{gsub(/.*= *"|"/,"");print;exit}' nusa.lock)
IDX_CSUM_A=$(awk '/version = "0.1.0"/{hit=1} hit && /checksum =/{gsub(/.*= *"|"/,"");print;exit}' "$TMPDIR/reg/index/suplib.toml")
[ -n "$PIN_CSUM" ] && [ -n "$IDX_CSUM_A" ] || fail "supply-shadow" "checksum tak terbaca"
[ "$PIN_CSUM" = "$IDX_CSUM_A" ] || fail "supply-shadow" "bukan registry pertama yang menang"
pass "supply-shadow"

echo "E2E SUPPLY PASS ($PASS_N cases)"
