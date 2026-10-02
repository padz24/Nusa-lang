#!/usr/bin/env bash
# SBOM conformance v3.14.0 (Phase 113, milestone 1): inventaris JSON
# deterministik dari manifest + lock (tanpa kriptografi baru).
# Skema: docs/supply-3.14.md. Hermetik seperti e2e_eco2.
set -u
NUSA_BIN="${1:?usage: e2e_sbom.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"; rm -rf "$HOME/.cache/nusa"' EXIT
cd "$TMPDIR" || exit 1
export NUSA_REGISTRY="$TMPDIR/reg"
export HOME="$TMPDIR/home"
mkdir -p "$HOME"
rm -rf "$HOME/.cache/nusa"

# Fixture: path-dep (capability network) + registry-dep.
"$NUSA_BIN" pkg init sbomlib --dir libs/sbomlib > /dev/null \
  || fail "sbom-fixture" "lib init"
printf 'fn double(n: i64) -> i64 {\n    return n * 2;\n}\n' > libs/sbomlib/sbomlib.ns
rm -f libs/sbomlib/main.ns
sed -i 's/^network = false$/network = true/' libs/sbomlib/nusa.toml
"$NUSA_BIN" pkg init sbomreg --dir reglib > /dev/null \
  || fail "sbom-fixture" "reglib init"
printf 'fn triple(n: i64) -> i64 {\n    return n * 3;\n}\n' > reglib/sbomreg.ns
rm -f reglib/main.ns
cd reglib || exit 1
"$NUSA_BIN" pkg publish --registry "$TMPDIR/reg" > /dev/null \
  || fail "sbom-fixture" "publish"
cd "$TMPDIR" || exit 1
"$NUSA_BIN" pkg init sbomapp --dir app > /dev/null \
  || fail "sbom-fixture" "app init"
cd app || exit 1
"$NUSA_BIN" pkg add sbomlib --path ../libs/sbomlib > /dev/null \
  || fail "sbom-fixture" "add"
"$NUSA_BIN" install 'sbomreg@0.1.0' > /dev/null \
  || fail "sbom-fixture" "install"

# 1. SBOM valid + path-dep capability + registry pin checksum.
"$NUSA_BIN" sbom > sbom.json 2>sbom.err \
  || { cat sbom.err; fail "sbom-run" "rc != 0"; }
python3 -m json.tool sbom.json > /dev/null \
  || { cat sbom.json; fail "sbom-run" "bukan JSON valid"; }
grep -q '"sbomFormat":"nusa-sbom/1"' sbom.json \
  || fail "sbom-run" "format hilang"
grep -q '"name":"sbomlib"' sbom.json \
  || fail "sbom-run" "path-dep hilang"
grep -q '"network"' sbom.json \
  || fail "sbom-run" "capability path-dep hilang"
grep -q '"name":"sbomreg"' sbom.json \
  || fail "sbom-run" "registry-dep hilang"
grep -q '"source":"registry:' sbom.json \
  || fail "sbom-run" "sumber registry hilang"
grep -q '"checksum":"sha256:' sbom.json \
  || fail "sbom-run" "checksum pin hilang"
grep -q '"signatures":false' sbom.json \
  || fail "sbom-run" "penanda signatures hilang"
pass "sbom-run"

# 2. Deterministik: dua run byte-identik.
"$NUSA_BIN" sbom > sbom2.json 2>/dev/null \
  || fail "sbom-determinism" "rc != 0"
cmp -s sbom.json sbom2.json || fail "sbom-determinism" "tak deterministik"
pass "sbom-determinism"

# 3. `pkg sbom` identik + tanpa manifest ditolak bersih.
"$NUSA_BIN" pkg sbom > sbom3.json 2>/dev/null \
  || fail "sbom-pkg" "rc != 0"
cmp -s sbom.json sbom3.json || fail "sbom-pkg" "beda dari top-level"
mkdir -p "$TMPDIR/empty" && cd "$TMPDIR/empty" || exit 1
if HOME="$TMPDIR/home" "$NUSA_BIN" sbom > /dev/null 2>&1; then
  fail "sbom-nomanifest" "rc=0 tanpa manifest"
fi
pass "sbom-pkg"

echo "E2E SBOM PASS ($PASS_N cases)"
