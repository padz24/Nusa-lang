#!/usr/bin/env bash
# ABI conformance v3.10 (Phase 109): bentuk IR kontrak biner
# (docs/abi-3.10.md). Drift bentuk = FAIL (disengaja: ABI berubah
# hanya via RFC + versi mayor). Compile-only (--emit-ir).
set -u
NUSA_BIN="${1:?usage: e2e_abi.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Struct non-packed urutan deklarasi + by-value + main C-ABI.
cat > s.ns <<'EOF'
struct Point {
    x: i64;
    name: string;
    ok: bool;
}
fn mk(v: i64) -> Point {
    return Point { x: v, name: "p", ok: true };
}
fn main() {
    let p = mk(1);
    print(p.x);
}
EOF
timeout 120 "$NUSA_BIN" build s.ns --emit-ir -o s.ll >s.log 2>&1 \
  || { cat s.log; fail "abi-struct" "build gagal"; }
grep -q "^%Point = type { i64, ptr, i1 }" s.ll \
  || fail "abi-struct" "layout Point berubah"
grep -q "^define %Point @mk(i64" s.ll \
  || fail "abi-struct" "return by-value berubah"
grep -q "^define i32 @main(i32" s.ll \
  || fail "abi-struct" "signature main berubah"
pass "abi-struct"

# 2. Enum = {tag:i64, slot...} + param by-value.
cat > e.ns <<'EOF'
enum E {
    Some(i64),
    None,
}
fn get(e: E) -> i64 {
    match e {
        Some(v) => { return v; },
        None => { return 0; },
    }
    return 0;
}
fn main() {
    print(get(E.Some(7)));
}
EOF
timeout 120 "$NUSA_BIN" build e.ns --emit-ir -o e.ll >e.log 2>&1 \
  || { cat e.log; fail "abi-enum" "build gagal"; }
grep -q "^%E = type { i64, i64 }" e.ll \
  || fail "abi-enum" "layout enum berubah"
grep -q "^define i64 @get(%E" e.ll \
  || fail "abi-enum" "param enum berubah"
pass "abi-enum"

# 3. Mangle generik + metode trait.
cat > m.ns <<'EOF'
fn id<T>(x: T) -> T {
    return x;
}
trait Show {
    fn show(self) -> string;
}
struct A {
    v: i64;
}
impl Show for A {
    fn show(self) -> string {
        return "A";
    }
}
fn main() {
    print(id(41));
    let a = A { v: 1 };
    print(a.show());
}
EOF
timeout 120 "$NUSA_BIN" build m.ns --emit-ir -o m.ll >m.log 2>&1 \
  || { cat m.log; fail "abi-mangle" "build gagal"; }
grep -q "^define i64 @id_i64(i64" m.ll \
  || fail "abi-mangle" "mangle generik berubah"
grep -q "^define ptr @Show_show_A(%A" m.ll \
  || fail "abi-mangle" "mangle trait berubah"
pass "abi-mangle"

# 4. Closure: pair {ptr,ptr} + trampolin internal + param ptr.
cat > c.ns <<'EOF'
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    let add1 = |x: i64| x + 1;
    print(apply(add1, 41));
}
EOF
timeout 120 "$NUSA_BIN" build c.ns --emit-ir -o c.ll >c.log 2>&1 \
  || { cat c.log; fail "abi-closure" "build gagal"; }
grep -q "^%__nusa_closure = type { ptr, ptr }" c.ll \
  || fail "abi-closure" "layout closure berubah"
grep -q "^define i64 @apply(ptr" c.ll \
  || fail "abi-closure" "param fn berubah"
grep -q '"\$lambda_0.tramp"' c.ll \
  || fail "abi-closure" "trampolin berubah"
pass "abi-closure"

# 5. Extern: declare-only + string->ptr; bool internal = i1.
cat > f.ns <<'EOF'
extern fn myputs(message: string) -> i64;
fn isyes(b: bool) -> bool {
    return b;
}
fn main() {
    print(isyes(true));
}
EOF
timeout 120 "$NUSA_BIN" build f.ns --emit-ir -o f.ll >f.log 2>&1 \
  || { cat f.log; fail "abi-ffi" "build gagal"; }
grep -q "^declare i64 @myputs(ptr)" f.ll \
  || fail "abi-ffi" "declare extern berubah"
grep -q "^define i1 @isyes(i1" f.ll \
  || fail "abi-ffi" "repr bool berubah"
pass "abi-ffi"

# 6. Default calling convention (tanpa anotasi cc eksplisit).
if grep -Eq "^define .+ cc[0-9]+ " s.ll m.ll c.ll; then
  fail "abi-cc" "calling convention eksplisit muncul"
fi
pass "abi-cc"

echo "E2E ABI PASS ($PASS_N cases)"
