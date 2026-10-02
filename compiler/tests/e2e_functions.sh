#!/usr/bin/env bash
# Integration test v0.5.0: function declaration, parameters, return type,
# return statement, calls, local scope, argument validation — semuanya
# dieksekusi dengan NILAI yang benar.
set -u
NUSA_BIN="${1:?usage: e2e_functions.sh <nusa-binary>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Program milestone MASTER PROMPT v0.5.0 -> 30
cp "$ROOT/examples/functions.ns" ./funcs.ns
OUT="$("$NUSA_BIN" run funcs.ns 2>funcs.log)" || { echo "E2E FAIL [milestone]"; cat funcs.log; exit 1; }
[ "$OUT" = "30" ] || { echo "E2E FAIL [milestone]: '$OUT'"; exit 1; }
echo "case milestone OK"

# 2. Multi-fungsi, nested calls, bool/string/void, local scope
cat > multi.ns <<'EOF'
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}

fn mul(a: i64, b: i64) -> i64 {
    return a * b;
}

fn is_big(n: i64) -> bool {
    return n > 100;
}

fn greet(name: string) {
    print(name);
}

fn main() {
    let r = add(mul(2, 3), 4);
    print(r);
    print(is_big(r));
    print(is_big(500));
    greet("Nusa");
}
EOF
"$NUSA_BIN" run multi.ns > multi.got 2>multi.log || { echo "E2E FAIL [multi]"; cat multi.log; exit 1; }
printf '10\nfalse\ntrue\nNusa\n' | diff - multi.got || { echo "E2E FAIL [multi]: got:"; cat multi.got; exit 1; }
echo "case multi OK"

# 3. Forward reference: main dulu, callee belakangan
printf 'fn main() { print(add(1, 2)); }\nfn add(a: i64, b: i64) -> i64 { return a + b; }\n' > fwd.ns
OUT="$("$NUSA_BIN" run fwd.ns 2>fwd.log)" || { echo "E2E FAIL [fwd]"; cat fwd.log; exit 1; }
[ "$OUT" = "3" ] || { echo "E2E FAIL [fwd]: '$OUT'"; exit 1; }
echo "case fwd OK"

# 4. Rekursi kompilasi (build-only; run akan tak berhenti)
printf 'fn f(n: i64) -> i64 { return f(n); }\nfn main() { print(f(1)); }\n' > rec.ns
"$NUSA_BIN" build rec.ns -o rec_out 2>rec.log || { echo "E2E FAIL [rec build]"; cat rec.log; exit 1; }
grep -q "define i64 @f(i64" rec.ns.ll || { echo "E2E FAIL [rec IR]"; exit 1; }
echo "case rec-build OK"

# 5. Argument validation: fewer args
printf 'fn add(a: i64, b: i64) -> i64 { return a + b; }\nfn main() { print(add(1)); }\n' > arity.ns
if "$NUSA_BIN" build arity.ns -o arity_out 2>arity.log; then echo "E2E FAIL [arity]"; exit 1; fi
grep -q "wrong number of arguments" arity.log || { echo "E2E FAIL [arity]:"; cat arity.log; exit 1; }
echo "case arity OK"

# 6. Argument validation: wrong type
printf 'fn add(a: i64, b: i64) -> i64 { return a + b; }\nfn main() { print(add(1, true)); }\n' > argty.ns
if "$NUSA_BIN" build argty.ns -o argty_out 2>argty.log; then echo "E2E FAIL [argty]"; exit 1; fi
grep -q "mismatched argument type" argty.log || { echo "E2E FAIL [argty]:"; cat argty.log; exit 1; }
echo "case argty OK"

# 7. Undefined function
printf 'fn main() { print(foo(1)); }\n' > undef.ns
if "$NUSA_BIN" build undef.ns -o undef_out 2>undef.log; then echo "E2E FAIL [undef]"; exit 1; fi
grep -q "undefined function 'foo'" undef.log || { echo "E2E FAIL [undef]:"; cat undef.log; exit 1; }
echo "case undef OK"

# 8. Missing return
printf 'fn f() -> i64 { let x = 1; }\nfn main() { }\n' > noreturn.ns
if "$NUSA_BIN" build noreturn.ns -o noreturn_out 2>noreturn.log; then echo "E2E FAIL [noreturn]"; exit 1; fi
grep -q "must return a value" noreturn.log || { echo "E2E FAIL [noreturn]:"; cat noreturn.log; exit 1; }
echo "case noreturn OK"

echo "E2E FUNCTIONS PASS"
