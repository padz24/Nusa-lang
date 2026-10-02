#!/usr/bin/env bash
# Integration test v0.3.0: variables & expressions dieksekusi dengan
# NILAI yang benar, bukan sekadar terkompilasi.
set -u
NUSA_BIN="${1:?usage: e2e_variables.sh <nusa-binary>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

run_case() {
  local name="$1" expected="$2"
  "$NUSA_BIN" run "$name.ns" > "$name.got" 2>"$name.log" || {
    echo "E2E FAIL [$name]: nusa run exited $?"; cat "$name.log"; exit 1;
  }
  if ! diff <(printf '%s\n' "$expected") "$name.got" > /dev/null; then
    echo "E2E FAIL [$name]:"; echo "--- expected ---"; printf '%s\n' "$expected"
    echo "--- got ---"; cat "$name.got"; exit 1
  fi
  echo "case $name OK"
}

# 1. Program milestone MASTER PROMPT v0.3.0
cat > milestone.ns <<'EOF'
fn main() {
    let a = 10;
    let b = 20;
    let result = a + b;
    print(result);
}
EOF
run_case milestone "30"

# 2. Contoh arithmetic (juga di examples/)
cp "$ROOT/examples/arithmetic.ns" ./arith.ns
"$NUSA_BIN" run arith.ns > arith.got 2>arith.log || { echo "E2E FAIL [arith]"; cat arith.log; exit 1; }
printf '30\n70\ntrue\n' | diff - arith.got || { echo "E2E FAIL [arith]: got:"; cat arith.got; exit 1; }
echo "case arith OK"

# 3. Precedence + unary + div + sub + bool + string var
cat > mixed.ns <<'EOF'
fn main() {
    let x = 10 + 20 * 3;
    print(x);
    let n = -5;
    print(n);
    print(20 / 4 - 1);
    print(10 - 4 * 2);
    let b = 1 == 2;
    print(b);
    print(7 >= 7);
    let s = "Nusa";
    print(s);
}
EOF
run_case mixed "$(printf '70\n-5\n4\n2\nfalse\ntrue\nNusa')"

# 4. Undefined variable -> error rapi (MASTER PROMPT wajib), tanpa crash
printf 'fn main() {\n    print(x);\n}\n' > undef.ns
if "$NUSA_BIN" build undef.ns -o undef_out 2>undef.log; then
  echo "E2E FAIL [undef]: should not build"; exit 1
fi
grep -q "undefined variable 'x'" undef.log || { echo "E2E FAIL [undef]:"; cat undef.log; exit 1; }
echo "case undef OK"

# 5. Duplicate variable -> error rapi
printf 'fn main() { let x = 1; let x = 2; }\n' > dup.ns
if "$NUSA_BIN" build dup.ns -o dup_out 2>dup.log; then
  echo "E2E FAIL [dup]: should not build"; exit 1
fi
grep -q "duplicate variable 'x'" dup.log || { echo "E2E FAIL [dup]:"; cat dup.log; exit 1; }
echo "case dup OK"

echo "E2E VARIABLES PASS"
