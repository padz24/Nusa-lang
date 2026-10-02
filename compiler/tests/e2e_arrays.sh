#!/usr/bin/env bash
# Integration test v0.7.0: string vars + array literal/index/len/bounds.
set -u
NUSA_BIN="${1:?usage: e2e_arrays.sh <nusa-binary>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

run_case() {
  local name="$1" expected="$2"
  timeout 10 "$NUSA_BIN" run "$name.ns" > "$name.got" 2>"$name.log" || {
    echo "E2E FAIL [$name]: exit $?"; cat "$name.log"; exit 1;
  }
  if ! diff <(printf '%s\n' "$expected") "$name.got" > /dev/null; then
    echo "E2E FAIL [$name]:"; echo "--- expected ---"; printf '%s\n' "$expected"
    echo "--- got ---"; cat "$name.got"; exit 1
  fi
  echo "case $name OK"
}

fail_case() {
  local name="$1" pattern="$2" src="$3"
  printf '%s' "$src" > "$name.ns"
  if "$NUSA_BIN" build "$name.ns" -o "$name.out" 2>"$name.log"; then
    echo "E2E FAIL [$name]: should not build"; exit 1
  fi
  grep -q "$pattern" "$name.log" || { echo "E2E FAIL [$name]:"; cat "$name.log"; exit 1; }
  echo "case $name OK"
}

# 1. Contoh milestone (juga di examples/)
cp "$ROOT/examples/arrays.ns" ./arr.ns
timeout 10 "$NUSA_BIN" run arr.ns > arr.got 2>arr.log || { echo "E2E FAIL [arr]"; cat arr.log; exit 1; }
printf 'Hello\n1\n4\n30\n' | diff - arr.got || { echo "E2E FAIL [arr]: got:"; cat arr.got; exit 1; }
grep -q "alloca \[4 x i64\]" arr.ns.ll || { echo "E2E FAIL [arr IR]"; exit 1; }
echo "case arr OK"

# 2. String regression (v0.7.0 menegaskan kembali)
cat > str.ns <<'EOF'
fn main() {
    let message: string = "Hello";
    print(message);
}
EOF
run_case str "Hello"

# 3. Index dinamis + reassignment array utuh
cat > dyn.ns <<'EOF'
fn main() {
    let a = [10, 20, 30];
    let i = 2;
    print(a[i]);
    let b = a;
    b = a;
    print(b[0] + len(b));
}
EOF
run_case dyn "$(printf '30\n13')"

# 4. Bounds check: indeks terlalu besar
printf 'fn main() {\n    let n = [1, 2];\n    print(n[5]);\n}\n' > oob.ns
timeout 10 "$NUSA_BIN" run oob.ns > oob.got 2>oob.log && { echo "E2E FAIL [oob]: should fail"; exit 1; }
grep -q "index out of bounds" oob.got oob.log || { echo "E2E FAIL [oob]:"; cat oob.got oob.log; exit 1; }
echo "case oob OK"

# 5. Bounds check: indeks negatif
printf 'fn main() {\n    let n = [1, 2];\n    let i = 0 - 1;\n    print(n[i]);\n}\n' > neg.ns
timeout 10 "$NUSA_BIN" run neg.ns > neg.got 2>neg.log && { echo "E2E FAIL [neg]: should fail"; exit 1; }
grep -q "index out of bounds" neg.got neg.log || { echo "E2E FAIL [neg]:"; cat neg.got neg.log; exit 1; }
echo "case neg OK"

# 6-11. Penolakan statis
fail_case mixed "mismatched array element type" 'fn main() { let x = [1, true]; }
'
fail_case empty "empty array" 'fn main() { let x = []; }
'
fail_case nested "nested arrays" 'fn main() { let x = [[1]]; }
'
fail_case idxstr "cannot index" 'fn main() { let s = "hi"; print(s[0]); }
'
fail_case printarr "cannot print" 'fn main() { let a = [1]; print(a); }
'
fail_case arrparam "array parameters" 'fn f(a: [i64]) -> i64 { return a[0]; }
fn main() { }
'

echo "E2E ARRAYS PASS"
