#!/usr/bin/env bash
# Integration test v1.8.0 (Phase 27): closures end-to-end.
#  - Lambda anotasi/inferensi-konteks, capture by-value, nested/curried.
#  - Kombinator generik std (opt_map) + function values lama.
#  - Penolakan: inferensi/arity/ret/array-capture/guard-lambda + fmt.
set -u
NUSA_BIN="${1:?usage: e2e_closures.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

run_ok() {
  local name="$1"
  "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "wrong output"; }
  pass "$name"
}

expect_fail() {
  local name="$1" pattern="$2"
  "$NUSA_BIN" check "$name.ns" >"$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should not check (rc=0)"
  [ $rc -le 128 ] || fail "$name" "COMPILER CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

# 1. Dasar: anotasi + higher-order + capture (nilai runtime).
cat > v1.ns <<'EOF'
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    let add = |a: i64, b: i64| a + b;
    print(add(1, 2));
    print(apply(|x: i64| x * 2, 21));
    let k = 10;
    let f = |x: i64| x + k;
    print(f(5));
    print(apply(|x| x + k, 1));
}
EOF
printf '3\n42\n15\n11\n' > v1.want
run_ok v1

# 2. Curried/nested + blok body + generic std (nilai runtime).
cat > v2.ns <<'EOF'
import option;
fn main() {
    print(opt_unwrap_or(opt_map(parse_int_opt("21"), |x| x * 2), 0));
    let mk = |a: i64| |b: i64| a + b;
    let add5 = mk(5);
    print(add5(3));
    let fs = |x: i64| { return x + 1; };
    print(fs(10));
}
EOF
printf '42\n8\n11\n' > v2.want
run_ok v2

# 3. Snapshot per-iterasi + capture fn/struct + shadowing (nilai runtime).
cat > v3.ns <<'EOF'
fn inc(x: i64) -> i64 {
    return x + 1;
}
struct P { v: i64; }
fn main() {
    let n = 0;
    while n < 3 {
        let f = |x: i64| x + n;
        print(f(100));
        n = n + 1;
    }
    let g = inc;
    let h = |x: i64| g(x) + 1;
    print(h(10));
    let p: P = P { v: 7 };
    let k = |x: i64| x + p.v;
    print(k(1));
    let k2 = 1;
    let sh = |k2: i64| k2 * 2;
    print(sh(21));
    print(k2);
}
EOF
printf '100\n101\n102\n12\n8\n42\n1\n' > v3.want
run_ok v3

# 4. Function values lama tetap jalan (kompatibilitas 21b).
cat > v4.ns <<'EOF'
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}
fn apply2(f: fn(i64, i64) -> i64, x: i64, y: i64) -> i64 {
    return f(x, y);
}
fn main() {
    let op = add;
    print(op(20, 22));
    print(apply2(add, 1, 2));
    print(apply2(op, 3, 4));
}
EOF
printf '42\n3\n7\n' > v4.want
run_ok v4

# 5. Penolakan: inferensi tanpa konteks.
cat > e1.ns <<'EOF'
fn main() {
    let g = |x| x + 1;
}
EOF
expect_fail e1 "cannot infer type of lambda parameter 'x'"

# 6. Penolakan: arity.
cat > e2.ns <<'EOF'
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    print(apply(|x: i64, y: i64| x, 1));
}
EOF
expect_fail e2 "lambda expects 1 parameter(s)"

# 7. Penolakan: return mismatch.
cat > e3.ns <<'EOF'
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    print(apply(|x: i64| "s", 1));
}
EOF
expect_fail e3 "mismatched return type"

# 8. Penolakan: capture array.
cat > e4.ns <<'EOF'
fn main() {
    let a: [i64] = [1, 2];
    let f: fn(i64) -> i64 = |x: i64| x + a[0];
}
EOF
expect_fail e4 "cannot capture array 'a'"

# 9. Penolakan: lambda di guard.
cat > e5.ns <<'EOF'
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    match 1 {
        n if apply(|v: i64| v, n) > 0 => { print(n); },
        _ => { print(0); },
    }
}
EOF
expect_fail e5 "lambdas cannot appear in match guards"

# 10. fmt lambda idempoten.
cat > messy.ns <<'EOF'
fn main() {
    let add = |a: i64, b: i64| a + b;
    let id = || 42;
    print(add(1, 2));
}
EOF
"$NUSA_BIN" fmt messy.ns > fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
"$NUSA_BIN" fmt --check messy.ns > /dev/null 2>&1 || fail "fmt" "not stable after fmt"
grep -q "|a: i64, b: i64|" messy.ns || fail "fmt" "lambda bars moved"
grep -q "|| 42" messy.ns || fail "fmt" "empty params moved"
pass "fmt"

echo "E2E CLOSURES PASS ($PASS_N cases)"
