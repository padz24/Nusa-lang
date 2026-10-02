#!/usr/bin/env bash
# Integration test v1.13.0 (Phase 32): C FFI end-to-end.
#  - Panggilan libc (strlen/abs/isupper/toupper) incl. mapping bool.
#  - Link file .o langsung + arsip .a via -L/-l.
#  - Manifest [link] via pkg build.
#  - Penolakan: body/generik/main/tipe/value + simbol hilang saat link.
set -u
NUSA_BIN="${1:?usage: e2e_ffi.sh <nusa-binary>}"
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
  timeout 60 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
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

# 1. libc: strlen/abs/isupper/toupper (mapping bool i1<->i32).
cat > v1.ns <<'EOF'
extern fn strlen(s: string) -> i64;
extern fn abs(n: i64) -> i64;
extern fn isupper(c: i64) -> bool;
extern fn toupper(c: i64) -> i64;
fn main() {
    print(strlen("abcd"));
    print(abs(0 - 7));
    print(isupper(65));
    print(isupper(97));
    print(toupper(97));
}
EOF
printf '4\n7\ntrue\nfalse\n65\n' > v1.want
run_ok v1

# 2. Link file .o langsung (--link path).
cat > answer.c <<'EOF'
long long answer(void) { return 42; }
long long twice(long long x) { return x * 2; }
EOF
clang -c answer.c -o answer.o || fail "cc" "cannot compile helper C"
cat > v2.ns <<'EOF'
extern fn answer() -> i64;
extern fn twice(x: i64) -> i64;
fn main() {
    print(answer());
    print(twice(21));
}
EOF
timeout 60 "$NUSA_BIN" build v2.ns -o v2 --link ./answer.o > v2.log 2>&1 || { cat v2.log; fail "v2" "build failed"; }
[ "$(./v2)" = "$(printf '42\n42')" ] || fail "v2" "wrong output"
pass "v2"

# 3. Arsip .a via --link-dir + --link nama.
ar rcs libanswer.a answer.o || fail "ar" "cannot archive helper C"
cat > v3.ns <<'EOF'
extern fn answer() -> i64;
fn main() {
    print(answer());
}
EOF
timeout 60 "$NUSA_BIN" build v3.ns -o v3 --link-dir . --link answer > v3.log 2>&1 || { cat v3.log; fail "v3" "build failed"; }
[ "$(./v3)" = "42" ] || fail "v3" "wrong output"
pass "v3"

# 4. Manifest [link] files via pkg build.
expect_ok_pkg() { "$@" > pkg.log 2>&1 || { cat pkg.log; fail "pkg" "rc != 0"; }; }
expect_ok_pkg "$NUSA_BIN" pkg init ffiapp --dir ffiapp
cp answer.o ffiapp/
printf 'extern fn twice(x: i64) -> i64;\nfn main() {\nprint(twice(21));\n}\n' > ffiapp/main.ns
printf '[package]\nname = "ffiapp"\nversion = "0.1.0"\n\n[dependencies]\n\n[link]\nfiles = ["answer.o"]\n\n[capabilities]\nnative = true\n' > ffiapp/nusa.toml
cd ffiapp
expect_ok_pkg "$NUSA_BIN" pkg build
[ "$(./target/ffiapp)" = "42" ] || fail "pkg" "wrong output"
pass "pkg-link"
cd ..

# 5. Penolakan.
cat > e1.ns <<'EOF'
extern fn f(x: i64) -> i64 {
    return x;
}
fn main() {
}
EOF
expect_fail e1 "have no bodies"

cat > e2.ns <<'EOF'
extern fn f<T>(x: T) -> T;
fn main() {
}
EOF
expect_fail e2 "cannot be generic"

cat > e3.ns <<'EOF'
struct P { x: i64; }
extern fn f(p: P) -> i64;
fn main() {
}
EOF
expect_fail e3 "cannot have type 'P'"

cat > e4.ns <<'EOF'
extern fn f() -> string;
fn main() {
}
EOF
expect_fail e4 "cannot return 'string'"

cat > e5.ns <<'EOF'
extern fn strlen(s: string) -> i64;
fn main() {
    let f = strlen;
}
EOF
expect_fail e5 "cannot use extern function"

# 6. Simbol hilang: check lolos, link gagal jelas.
cat > e6.ns <<'EOF'
extern fn no_such_fn_xyz() -> i64;
fn main() {
    print(no_such_fn_xyz());
}
EOF
"$NUSA_BIN" check e6.ns > e6.log 2>&1 || fail "e6" "check should pass"
timeout 60 "$NUSA_BIN" build e6.ns -o e6 > e6blog.log 2>&1 && fail "e6" "link should fail"
grep -q "no_such_fn_xyz" e6blog.log || { cat e6blog.log; fail "e6" "missing symbol in message"; }
pass "e6"

echo "E2E FFI PASS ($PASS_N cases)"
