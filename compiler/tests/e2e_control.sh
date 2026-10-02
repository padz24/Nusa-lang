#!/usr/bin/env bash
# Integration test v0.6.0: if/else/while + nested + assignment —
# dieksekusi dengan NILAI yang benar (loop dibatasi timeout agar
# bug tak menggantung test suite).
set -u
NUSA_BIN="${1:?usage: e2e_control.sh <nusa-binary>}"
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

# 1. Contoh milestone (juga di examples/)
cp "$ROOT/examples/control.ns" ./ctrl.ns
timeout 10 "$NUSA_BIN" run ctrl.ns > ctrl.got 2>ctrl.log || { echo "E2E FAIL [ctrl]"; cat ctrl.log; exit 1; }
printf '10\n3\n2\n1\n' | diff - ctrl.got || { echo "E2E FAIL [ctrl]: got:"; cat ctrl.got; exit 1; }
grep -q "then:" ctrl.ns.ll && grep -q "else:" ctrl.ns.ll && grep -q "merge:" ctrl.ns.ll || { echo "E2E FAIL [ctrl IR]"; exit 1; }
grep -q "loop.cond:" ctrl.ns.ll && grep -q "loop.body:" ctrl.ns.ll && grep -q "loop.after:" ctrl.ns.ll || { echo "E2E FAIL [ctrl IR loop]"; exit 1; }
echo "case ctrl OK"

# 2. Cabang else
cat > elseb.ns <<'EOF'
fn main() {
    let x: i64 = 3;
    if x > 5 {
        print(1);
    } else {
        print(0);
    }
}
EOF
run_case elseb "0"

# 3. Nested if + else-if chain
cat > nestedif.ns <<'EOF'
fn grade(s: i64) -> i64 {
    if s > 80 {
        return 3;
    } else if s > 60 {
        return 2;
    } else {
        return 1;
    }
}

fn main() {
    print(grade(90));
    print(grade(70));
    print(grade(50));
}
EOF
run_case nestedif "$(printf '3\n2\n1')"

# 4. While: sum 1..10 = 55
cat > sum.ns <<'EOF'
fn main() {
    let total: i64 = 0;
    let i: i64 = 1;
    while i <= 10 {
        total = total + i;
        i = i + 1;
    }
    print(total);
}
EOF
run_case sum "55"

# 5. Nested loops
cat > nest.ns <<'EOF'
fn main() {
    let i: i64 = 1;
    while i <= 3 {
        let j: i64 = 1;
        while j <= 3 {
            print(i * 10 + j);
            j = j + 1;
        }
        i = i + 1;
    }
}
EOF
run_case nest "$(printf '11\n12\n13\n21\n22\n23\n31\n32\n33')"

# 6. While false: body dilewati
cat > wfalse.ns <<'EOF'
fn main() {
    let done: bool = false;
    while done {
        print(1);
    }
    print(0);
}
EOF
run_case wfalse "0"

# 7. Assignment salah tipe
printf 'fn main() { let n: i64 = 1; n = true; }\n' > aset.ns
if "$NUSA_BIN" build aset.ns -o aset_out 2>aset.log; then echo "E2E FAIL [aset]"; exit 1; fi
grep -q "mismatched types in assignment" aset.log || { echo "E2E FAIL [aset]:"; cat aset.log; exit 1; }
echo "case aset OK"

# 8. Assignment ke variabel tak dikenal
printf 'fn main() { x = 1; }\n' > aundef.ns
if "$NUSA_BIN" build aundef.ns -o aundef_out 2>aundef.log; then echo "E2E FAIL [aundef]"; exit 1; fi
grep -q "undefined variable 'x'" aundef.log || { echo "E2E FAIL [aundef]:"; cat aundef.log; exit 1; }
echo "case aundef OK"

# 9. Kondisi non-bool
printf 'fn main() { if 1 { print(1); } }\n' > cond.ns
if "$NUSA_BIN" build cond.ns -o cond_out 2>cond.log; then echo "E2E FAIL [cond]"; exit 1; fi
grep -q "if condition must be of type 'bool'" cond.log || { echo "E2E FAIL [cond]:"; cat cond.log; exit 1; }
echo "case cond OK"

# 10. Bare return di tengah main (bug 2.0: ret void vs i32 @main).
# Dulu: gagal verifikasi IR. Kini: keluar 0 tanpa mengeksekusi sisa.
printf 'fn main() {\n    print("a");\n    if true {\n        return;\n    }\n    print("b");\n}\n' > retmain.ns
OUT="$("$NUSA_BIN" run retmain.ns 2>retmain.log)" || { cat retmain.log; echo "E2E FAIL [retmain]"; exit 1; }
[ "$OUT" = "a" ] || { echo "E2E FAIL [retmain]: '$OUT'"; exit 1; }
echo "case retmain OK"

echo "E2E CONTROL PASS"
