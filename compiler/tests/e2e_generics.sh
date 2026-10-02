#!/usr/bin/env bash
# Integration test v1.2.0 (Phase 21c): generics end-to-end.
#  - Monomorphisasi: identity/multi-type, higher-order generik, nested,
#    rekursi, dua instance (i64 + string).
#  - Nilai runtime; IR hanya berisi instance (tanpa template).
#  - Penolakan: konflik, uninferrable, arity, generic-as-value,
#    main-generik, dup tparam, arg struct.
#  - fmt + doc generik.
set -u
NUSA_BIN="${1:?usage: e2e_generics.sh <nusa-binary>}"
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

# 1. identity dua instance + first + higher-order generik + nested.
cat > v1.ns <<'EOF'
fn identity<T>(value: T) -> T { return value; }
fn first<T>(a: T, b: T) -> T { return a; }
fn apply<T, U>(f: fn(T) -> U, x: T) -> U { return f(x); }
fn add1(x: i64) -> i64 { return x + 1; }
fn wrap<T>(x: T) -> T { return identity(x); }
fn main() {
    print(identity(41) + 1);
    print(identity("hi"));
    print(first(1, 2));
    print(apply(add1, 10));
    print(wrap("w"));
}
EOF
printf '42\nhi\n1\n11\nw\n' > v1.want
run_ok v1

# 2. Generik tak dipakai tak dicek (model C++): template rusak lolos
# sampai diinstansiasi.
cat > v2.ns <<'EOF'
fn f<T>(x: T) -> T { return f(x); }
fn main() { print("ok"); }
EOF
"$NUSA_BIN" check v2.ns >v2.log 2>&1 || fail "unused-generic" "should check"
pass "unused-generic"

# 3. IR: hanya instance mangled, tanpa template mentah.
cat > v3.ns <<'EOF'
fn identity<T>(value: T) -> T { return value; }
fn main() { print(identity(1)); print(identity("s")); }
EOF
"$NUSA_BIN" build v3.ns -o v3.out >v3.log 2>&1 || { cat v3.log; fail "v3" "build failed"; }
grep -q "define .* @identity_i64" v3.ns.ll || fail "v3" "missing identity_i64"
grep -q "define .* @identity_string" v3.ns.ll || fail "v3" "missing identity_string"
if grep -q "define .* @identity(" v3.ns.ll; then fail "v3" "raw template emitted"; fi
./v3.out >v3.got 2>&1 || fail "v3" "run failed"
printf '1\ns\n' | cmp -s v3.got - || { cat v3.got; fail "v3" "wrong output"; }
pass "v3"

# 4. Penolakan.
printf 'fn f<T>(a: T, b: T) -> T { return a; }\nfn main() { print(f(1, "s")); }\n' > conflict.ns
expect_fail conflict "mismatched argument type for 'f'"
printf 'fn zero<T>() -> T { return 1; }\nfn main() { print(zero()); }\n' > noinfer.ns
expect_fail noinfer "cannot infer type argument 'T' for 'zero'"
printf 'fn f<T>(a: T) -> T { return a; }\nfn main() { print(f(1, 2)); }\n' > arity.ns
expect_fail arity "wrong number of arguments for 'f'"
printf 'fn f<T>(x: T) -> T { return x; }\nfn main() { let g = f; }\n' > asvalue.ns
expect_fail asvalue "cannot use generic function 'f' as a value"
printf 'fn main<T>() { }\n' > main.ns
expect_fail main "'fn main' cannot be generic"
printf 'fn f<T, T>(x: T) -> T { return x; }\nfn main() { }\n' > duptp.ns
expect_fail duptp "duplicate type parameter 'T' in 'f'"
printf 'struct P { x: i64; }\nfn id<T>(v: T) -> T { return v; }\nfn main() { let p = P { x: 1 }; print(id(p).x + 1); }\n' > structarg.ns
"$NUSA_BIN" run structarg.ns >structarg.got 2>structarg.err || { cat structarg.err; fail "structarg" "run failed"; }
echo "2" | cmp -s structarg.got - || { cat structarg.got; fail "structarg" "wrong output"; }
pass "structarg"

# 5. fmt + doc generik.
printf 'fn identity<T>(value:T)->T{return value;}\nfn main(){}\n' > f.ns
"$NUSA_BIN" fmt f.ns >fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
grep -q "fn identity<T>(value: T) -> T {" f.ns || { cat f.ns; fail "fmt" "generic misformatted"; }
pass "fmt-generic"
printf '/// Identity.\nfn identity<T>(value: T) -> T {\n    return value;\n}\nfn main() { }\n' > d.ns
"$NUSA_BIN" doc d.ns -o d.md >doc.log 2>&1 || { cat doc.log; fail "doc" "doc failed"; }
grep -q '### `identity<T>(value: T) -> T`' d.md || { cat d.md; fail "doc" "generic sig missing"; }
pass "doc-generic"

echo "GENERICS PASS ($PASS_N cases)"
