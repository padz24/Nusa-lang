#!/usr/bin/env bash
# Integration test v0.4.0: anotasi tipe dieksekusi benar; mismatch ditolak
# dengan pesan tipe yang jelas (contoh wajib MASTER PROMPT).
set -u
NUSA_BIN="${1:?usage: e2e_types.sh <nusa-binary>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Contoh types.ns -> nilai benar
cp "$ROOT/examples/types.ns" ./types.ns
"$NUSA_BIN" run types.ns > types.got 2>types.log || { echo "E2E FAIL [types]"; cat types.log; exit 1; }
printf '15\ntrue\nNusa\n30\n' | diff - types.got || { echo "E2E FAIL [types]: got:"; cat types.got; exit 1; }
echo "case types OK"

# 2. Contoh wajib MASTER PROMPT: let x: i64 = "hello" DITOLAK
printf 'fn main() {\n    let x: i64 = "hello";\n}\n' > mismatch.ns
if "$NUSA_BIN" build mismatch.ns -o mismatch_out 2>mismatch.log; then
  echo "E2E FAIL [mismatch]: should not build"; exit 1
fi
grep -q "mismatched types" mismatch.log || { echo "E2E FAIL [mismatch]:"; cat mismatch.log; exit 1; }
grep -q "i64" mismatch.log && grep -q "string" mismatch.log || { echo "E2E FAIL [mismatch]: types missing"; cat mismatch.log; exit 1; }
echo "case mismatch OK"

# 3. Program tanpa anotasi tetap jalan (inferensi, regression v0.3.0)
cat > infer.ns <<'EOF'
fn main() {
    let a = 10;
    let b = 20;
    let result = a + b;
    print(result);
}
EOF
OUT="$("$NUSA_BIN" run infer.ns 2>infer.log)" || { echo "E2E FAIL [infer]"; cat infer.log; exit 1; }
[ "$OUT" = "30" ] || { echo "E2E FAIL [infer]: '$OUT'"; exit 1; }
echo "case infer OK"

# 4. Operasi antar-tipe ditolak: 1 + true
printf 'fn main() { print(1 + true); }\n' > mix.ns
if "$NUSA_BIN" build mix.ns -o mix_out 2>mix.log; then
  echo "E2E FAIL [mix]: should not build"; exit 1
fi
grep -q "requires 'i64' operands" mix.log || { echo "E2E FAIL [mix]:"; cat mix.log; exit 1; }
echo "case mix OK"

# 5. void untuk variabel ditolak
printf 'fn main() { let x: void = 1; }\n' > voidvar.ns
if "$NUSA_BIN" build voidvar.ns -o voidvar_out 2>voidvar.log; then
  echo "E2E FAIL [voidvar]: should not build"; exit 1
fi
grep -q "'void'" voidvar.log || { echo "E2E FAIL [voidvar]:"; cat voidvar.log; exit 1; }
echo "case voidvar OK"

echo "E2E TYPES PASS"
