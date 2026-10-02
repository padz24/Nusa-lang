#!/usr/bin/env bash
# Plugin conformance v3.13.0 (Phase 112, milestone 1): kontrak JSON
# request-response atas program .ns, dieksekusi di bawah batas
# sandbox proses (timeout/mem/output). WASM = jalur migrasi
# (docs/plugin-3.13.md), bukan backend hari ini.
set -u
NUSA_BIN="${1:?usage: e2e_plugin.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

cat > calc.ns <<'EOF'
import json;
import string;
fn main() {
    let req = input();
    let op = json_get_string(req, "op");
    if str_eq(op, "add") {
        let a = json_get_int(req, "a");
        let b = json_get_int(req, "b");
        print(str_concat("{\"ok\":true,\"result\":", str_concat(int_to_str(a + b), "}")));
    } else {
        print("{\"ok\":false,\"error\":\"unknown op\"}");
    }
}
EOF
cat > calc.json <<'EOF'
{"name":"calc","version":"1.0","ops":["add"]}
EOF

# 1. Roundtrip tersandbox + manifest gate lolos.
echo '{"op":"add","a":20,"b":22}' \
  | timeout 300 "$NUSA_BIN" plugin run calc.ns --manifest calc.json \
  >round.got 2>round.err || { cat round.err; fail "plug-round" "rc != 0"; }
grep -q '{"ok":true,"result":42}' round.got \
  || { cat round.got; fail "plug-round" "respons salah"; }
grep -q "verdict=ok" round.err \
  || { cat round.err; fail "plug-round" "tanpa verdict"; }
pass "plug-round"

# 2. Op di luar manifest DITOLAK tanpa eksekusi.
echo '{"op":"del"}' \
  | timeout 120 "$NUSA_BIN" plugin run calc.ns --manifest calc.json \
  >ref.got 2>ref.err && fail "plug-refuse" "rc=0 untuk op asing"
grep -q "verdict=refused" ref.err \
  || { cat ref.err; fail "plug-refuse" "bukan refusal"; }
grep -q "not in manifest" ref.got \
  || { cat ref.got; fail "plug-refuse" "alasan tak jelas"; }
pass "plug-refuse"

# 3. Tanpa manifest: jalan (gate nonaktif, terdokumentasi).
echo '{"op":"add","a":1,"b":2}' \
  | timeout 300 "$NUSA_BIN" plugin run calc.ns \
  >noman.got 2>noman.err || { cat noman.err; fail "plug-noman" "rc != 0"; }
grep -q '"result":3' noman.got \
  || { cat noman.got; fail "plug-noman" "respons salah"; }
pass "plug-noman"

# 4. Plugin gantung -> verdict timeout (batas dienforce).
printf 'fn main() {\n    while true {\n    }\n}\n' > hang.ns
echo '{"op":"x"}' \
  | timeout 120 "$NUSA_BIN" plugin run hang.ns --timeout 3 \
  >hang.got 2>hang.err && fail "plug-timeout" "rc=0 untuk hang"
grep -q "verdict=timeout" hang.err \
  || { cat hang.err; fail "plug-timeout" "bukan timeout"; }
pass "plug-timeout"

# 5. Plugin rusak -> build-fail; request kosong/manifest rusak ditolak.
printf 'fn main() { @ }\n' > bad.ns
echo '{"op":"x"}' \
  | timeout 120 "$NUSA_BIN" plugin run bad.ns \
  >bad.got 2>bad.err && fail "plug-buildfail" "rc=0 untuk rusak"
grep -q "verdict=build-fail" bad.err \
  || { cat bad.err; fail "plug-buildfail" "bukan build-fail"; }
printf '' | timeout 60 "$NUSA_BIN" plugin run calc.ns \
  >empty.got 2>empty.err && fail "plug-empty" "rc=0 untuk kosong"
grep -q "empty request" empty.err \
  || { cat empty.err; fail "plug-empty" "pesan tak jelas"; }
echo '{"op":"add"}' \
  | timeout 60 "$NUSA_BIN" plugin run calc.ns --manifest badmanifest.json \
  >mman.got 2>mman.err && fail "plug-manifest" "rc=0 untuk manifest hilang"
grep -q "cannot read manifest" mman.err \
  || { cat mman.err; fail "plug-manifest" "pesan tak jelas"; }
pass "plug-negatives"

echo "E2E PLUGIN PASS ($PASS_N cases)"
