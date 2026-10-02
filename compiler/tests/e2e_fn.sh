#!/usr/bin/env bash
# Integration test v1.2.0 (Phase 21b): function types end-to-end.
#  - Function reference + indirect call + higher-order + fn-returning-fn.
#  - Nilai runtime; void-fn values.
#  - Penolakan: non-fn call, main/builtin/print as value, print/==/array/
#    field, arity/mismatch via variabel, alias-to-fn.
#  - Lintas modul + fmt.
set -u
NUSA_BIN="${1:?usage: e2e_fn.sh <nusa-binary>}"
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

# 1. Reference + higher-order + fn-returning-fn + void values.
cat > v1.ns <<'EOF'
fn add(a: i64, b: i64) -> i64 { return a + b; }
fn apply(f: fn(i64, i64) -> i64, x: i64, y: i64) -> i64 { return f(x, y); }
fn mk() -> fn(i64, i64) -> i64 { return add; }
fn v() { print(100); }
fn run(g: fn() -> void) { g(); }
fn main() {
    let op = add;
    print(op(20, 22));
    print(apply(add, 1, 2));
    print(apply(op, 3, 4));
    let f = mk();
    print(f(10, 20));
    run(v);
    let w = v;
    w();
}
EOF
printf '42\n3\n7\n30\n100\n100\n' > v1.want
run_ok v1

# 2. Lintas modul: value fungsi import.
cat > ops.ns <<'EOF'
fn sub(a: i64, b: i64) -> i64 { return a - b; }
EOF
cat > v2.ns <<'EOF'
import ops;
fn apply(f: fn(i64, i64) -> i64, x: i64, y: i64) -> i64 { return f(x, y); }
fn main() { print(apply(sub, 10, 4)); }
EOF
echo "6" > v2.want
run_ok v2

# 3. Penolakan.
printf 'fn main() { let x = 5; print(x(1)); }\n' > nonfn.ns
expect_fail nonfn "cannot call 'x' of type 'i64'"
printf 'fn main() { let f = main; }\n' > mainval.ns
expect_fail mainval "cannot use 'main' as a value"
printf 'fn main() { let f = len; }\n' > builtinval.ns
expect_fail builtinval "cannot use builtin 'len' as a value"
printf 'fn main() { let f = print; }\n' > printval.ns
expect_fail printval "cannot use 'print' as a value"
printf 'fn add(a: i64) -> i64 { return a; }\nfn main() { print(add); }\n' > printfn.ns
expect_fail printfn "cannot print value of type 'fn(i64) -> i64'"
printf 'fn add(a: i64) -> i64 { return a; }\nfn main() { let f = add; print(f == f); }\n' > cmpfn.ns
expect_fail cmpfn "cannot compare functions with '=='"
printf 'fn add(a: i64) -> i64 { return a; }\nfn main() { let fs = [add]; }\n' > arrfn.ns
expect_fail arrfn "arrays of functions are not supported"
printf 'fn cb() -> i64 { return 1; }\nstruct S { f: fn() -> i64; }\nfn main() { }\n' > fieldfn.ns
expect_fail fieldfn "function fields are not supported"
printf 'fn add(a: i64, b: i64) -> i64 { return a + b; }\nfn main() { let f = add; print(f(1)); }\n' > arity.ns
expect_fail arity "wrong number of arguments for 'f'"
printf 'fn add(a: i64) -> i64 { return a; }\nfn main() { let f = add; print(f("s")); }\n' > argty.ns
expect_fail argty "mismatched argument type for 'f'"
printf 'type F = fn(i64) -> i64;\nfn add(a: i64) -> i64 { return a; }\nfn main() { let f: F = add; print(f(41)); }\n' > aliasfn.ns
"$NUSA_BIN" run aliasfn.ns >aliasfn.got 2>aliasfn.err || { cat aliasfn.err; fail "aliasfn" "run failed"; }
echo "41" | cmp -s aliasfn.got - || { cat aliasfn.got; fail "aliasfn" "wrong output"; }
pass "aliasfn"

# 4. fmt: anotasi fn-type.
printf 'fn apply(f:fn(i64,i64)->i64,x:i64)->i64{return f(x,1);}\nfn main(){}\n' > f.ns
"$NUSA_BIN" fmt f.ns >fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
grep -q "fn apply(f: fn(i64, i64) -> i64, x: i64) -> i64 {" f.ns || { cat f.ns; fail "fmt" "fn-type misformatted"; }
pass "fmt-fntype"

echo "FN PASS ($PASS_N cases)"
