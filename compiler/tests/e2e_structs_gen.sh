#!/usr/bin/env bash
# Integration test v1.2.0 (Phase 21d): generic structs end-to-end.
#  - Inferensi + eksplisit + nested + alias + by-value params/returns.
#  - Nilai runtime; IR instance-only (tanpa template).
#  - Penolakan: konflik, uninferrable, arity (targs + decl),
#    bare/non-generik, infinite-size, tabrakan nama, dup tparam.
#  - fmt + doc generik.
set -u
NUSA_BIN="${1:?usage: e2e_structs_gen.sh <nusa-binary>}"
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

# 1. Inferensi dua instance + nested + eksplisit + alias + by-value.
cat > v1.ns <<'EOF'
struct Point<T> { x: T; y: T; }
struct Line<T> { a: Point<T>; b: Point<T>; }
type IPoint = Point<i64>;
fn mkp<T>(x: T, y: T) -> Point<T> { return Point { x: x, y: y }; }
fn getx<T>(p: Point<T>) -> T { return p.x; }
fn main() {
    let p = Point { x: 1, y: 2 };
    print(p.x + p.y);
    let q = Point { x: "a", y: "b" };
    print(q.x);
    let l: Line<i64> = Line { a: Point<i64> { x: 1, y: 2 }, b: mkp(3, 4) };
    print(l.a.x + l.b.y);
    let r: IPoint = mkp(10, 20);
    print(getx(r));
}
EOF
printf '3\na\n5\n10\n' > v1.want
run_ok v1

# 2. By-value biasa (non-generik) tetap jalan.
cat > v2.ns <<'EOF'
struct P { x: i64; }
fn mk(v: i64) -> P { return P { x: v }; }
fn bump(p: P) -> P { return P { x: p.x + 1 }; }
fn main() { print(bump(mk(41)).x); }
EOF
echo "42" > v2.want
run_ok v2

# 3. IR: instance mangled, tanpa template mentah.
cat > v3.ns <<'EOF'
struct Point<T> { x: T; y: T; }
fn main() { let p = Point { x: 1, y: 2 }; print(p.x); }
EOF
"$NUSA_BIN" build v3.ns -o v3.out >v3.log 2>&1 || { cat v3.log; fail "v3" "build failed"; }
grep -q "define .* @main" v3.ns.ll || fail "v3" "missing main"
grep -q "%Point_i64 =" v3.ns.ll || fail "v3" "missing Point_i64 layout"
if grep -q "%Point =" v3.ns.ll; then fail "v3" "raw template emitted"; fi
./v3.out >v3.got 2>&1 || fail "v3" "run failed"
echo "1" | cmp -s v3.got - || { cat v3.got; fail "v3" "wrong output"; }
pass "v3"

# 4. Penolakan.
printf 'struct P<T> { x: T; y: T; }\nfn main() { let p = P { x: 1, y: "s" }; }\n' > conflict.ns
expect_fail conflict "mismatched field type: 'P.y' expects 'i64', found 'string'"
printf 'struct P<T, U> { x: T; }\nfn main() { let p = P { x: 1 }; }\n' > noinfer.ns
expect_fail noinfer "cannot infer type argument 'U' for struct 'P'"
printf 'struct P<T> { x: T; }\nfn main() { let p = P<i64, i64> { x: 1 }; }\n' > targarity.ns
expect_fail targarity "wrong number of type arguments for 'P'"
printf 'struct P<T> { x: T; }\nfn main() { let p: P = P { x: 1 }; }\n' > bare.ns
expect_fail bare "used without type arguments"
printf 'struct P { x: i64; }\nfn main() { let p: P<i64> = P { x: 1 }; }\n' > nongeneric.ns
expect_fail nongeneric "is not generic"
printf 'struct L<T> { tail: L<T>; }\nfn mk() -> L<i64> { return mk(); }\nfn main() { }\n' > infinite.ns
expect_fail infinite "has infinite size"
printf 'struct P<T> { x: T; }\nstruct P_i64 { x: i64; }\nfn main() { let p = P { x: 1 }; }\n' > collide.ns
expect_fail collide "is already declared"
printf 'struct P<T, T> { x: T; }\nfn main() { }\n' > duptp.ns
expect_fail duptp "duplicate type parameter 'T' in 'P'"
printf 'struct P<T> { x: Nope; }\nfn main() { }\n' > badfield.ns
expect_fail badfield "unknown type 'Nope'"

# 5. fmt + doc generik.
printf 'struct Point<T>{x:T;y:T;}\nfn main(){let p:Point<i64> = Point{x:1,y:2};print(p.x);}\n' > f.ns
"$NUSA_BIN" fmt f.ns >fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
grep -q "struct Point<T> {" f.ns || { cat f.ns; fail "fmt" "generic struct misformatted"; }
grep -q "let p: Point<i64> = Point {" f.ns || { cat f.ns; fail "fmt" "type app misformatted"; }
pass "fmt-structs-gen"
printf '/// A point.\nstruct Point<T> {\n    x: T;\n    y: T;\n}\nfn main() { }\n' > d.ns
"$NUSA_BIN" doc d.ns -o d.md >doc.log 2>&1 || { cat doc.log; fail "doc" "doc failed"; }
grep -q '### `struct Point<T>`' d.md || { cat d.md; fail "doc" "generic struct header missing"; }
pass "doc-structs-gen"

echo "STRUCTS-GEN PASS ($PASS_N cases)"
