#!/usr/bin/env bash
# Integration test v1.14.0 (Phase 33): cross-compilation end-to-end.
#  - Host default tak berubah (tanpa triple di IR).
#  - --target + --emit-ir (triple), -c objek ELF per-arsitektur (file).
#  - Link cross tanpa sysroot ditolak dini; target bogus jelas.
#  - run --target cross ditolak; manifest target dihormati pkg build.
set -u
NUSA_BIN="${1:?usage: e2e_cross.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

expect_ok() {
  local name="$1"; shift
  "$@" > "$name.log" 2>&1 || { cat "$name.log"; fail "$name" "rc != 0"; }
  pass "$name"
}

expect_fail() {
  local name="$1" pattern="$2"; shift 2
  "$@" > "$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should fail (rc=0)"
  [ $rc -le 128 ] || fail "$name" "CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

printf 'fn main() {\nprint(40 + 2);\n}\n' > hello.ns

# 1. Host default: jalan + IR tanpa triple.
timeout 60 "$NUSA_BIN" run hello.ns > host.got 2>host.err || { cat host.err; fail "host" "run failed"; }
[ "$(cat host.got)" = "42" ] || fail "host" "wrong output"
grep -q "target triple" hello.ns.ll && fail "host" "default IR must not pin triple"
pass "host"

# 2. --emit-ir menanam triple.
expect_ok ir-x64 "$NUSA_BIN" build hello.ns --target linux-x86_64 --emit-ir -o x64.ll
grep -q 'target triple = "x86_64-linux-gnu"' x64.ll || fail "ir-x64" "triple missing"
expect_ok ir-arm "$NUSA_BIN" build hello.ns --target linux-aarch64 --emit-ir -o arm.ll
grep -q 'target triple = "aarch64-linux-gnu"' arm.ll || fail "ir-arm" "triple missing"

# 3. Objek ELF per-arsitektur (compile-only, tanpa sysroot).
expect_ok obj-x64 "$NUSA_BIN" build hello.ns --target linux-x86_64 -c -o x64.o
[ -s x64.o ] || fail "obj-x64" "empty object"
file x64.o | grep -q "x86-64" || fail "obj-x64" "not x86-64: $(file x64.o)"
expect_ok obj-arm "$NUSA_BIN" build hello.ns --target linux-aarch64 -c -o arm.o
file arm.o | grep -q "ARM aarch64" || fail "obj-arm" "not aarch64: $(file arm.o)"

# 4. Link cross tanpa sysroot ditolak dini (bukan dump ld).
expect_fail nosysroot "needs --sysroot" "$NUSA_BIN" build hello.ns --target linux-x86_64 -o xbin

# 5. Target bogus + run cross jelas.
expect_fail bad-target "Supported:" "$NUSA_BIN" build hello.ns --target windows-x64 -o xbin
expect_fail run-cross "not the host" "$NUSA_BIN" run hello.ns --target linux-x86_64
expect_ok run-host "$NUSA_BIN" run hello.ns --target android-aarch64

# 6. Manifest target dihormati pkg build (CLI menang).
expect_ok pkg-init "$NUSA_BIN" pkg init xapp --dir xapp
printf '[package]\nname = "xapp"\nversion = "0.1.0"\ntarget = "linux-x86_64"\n\n[dependencies]\n' > xapp/nusa.toml
printf 'fn main() {\nprint(40 + 2);\n}\n' > xapp/main.ns
cd xapp
expect_fail pkg-cross "needs --sysroot" "$NUSA_BIN" pkg build
expect_ok pkg-host "$NUSA_BIN" pkg build --target android-aarch64
[ -x target/xapp ] || fail "pkg-host" "target binary missing"
[ "$(./target/xapp)" = "42" ] || fail "pkg-host" "wrong output"
cd ..

# 7. wasm32 bare-metal (Phase 67, v2.18.0): IR + objek + tolak link/run.
expect_ok ir-wasm "$NUSA_BIN" build hello.ns --target wasm32 --emit-ir -o wasm.ll
grep -q 'target triple = "wasm32-unknown-unknown"' wasm.ll || fail "ir-wasm" "triple missing"
expect_ok obj-wasm "$NUSA_BIN" build hello.ns --target wasm32 -c -o wasm.o
[ -s wasm.o ] || fail "obj-wasm" "empty object"
file wasm.o | grep -qi "webassembly" || fail "obj-wasm" "not wasm: $(file wasm.o)"
expect_fail nosysroot-wasm "needs --sysroot" "$NUSA_BIN" build hello.ns --target wasm32 -o wbin
expect_fail run-wasm "not the host" "$NUSA_BIN" run hello.ns --target wasm32

echo "E2E CROSS PASS ($PASS_N cases)"
