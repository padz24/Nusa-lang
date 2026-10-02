#!/usr/bin/env bash
# Integration test v1.11.0 (Phase 30): package manager end-to-end.
# Semua lokal + deterministik (tanpa network/registry).
set -u
NUSA_BIN="${1:?usage: e2e_pkg.sh <nusa-binary>}"
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

# 1. init: manifest + stub.
expect_ok init "$NUSA_BIN" pkg init myapp --dir app
grep -q 'name = "myapp"' app/nusa.toml || fail "init" "manifest name"
grep -q 'version = "0.1.0"' app/nusa.toml || fail "init" "manifest version"
[ -f app/main.ns ] || fail "init" "stub main.ns missing"

# 2. init ganda ditolak; nama invalid ditolak.
expect_fail init-dup "already exists" "$NUSA_BIN" pkg init myapp --dir app
expect_fail init-bad "invalid package name" "$NUSA_BIN" pkg init '9bad' --dir bad

# 3. Dep package + add + update (lock + checksum).
expect_ok lib-init "$NUSA_BIN" pkg init greet --dir libs/greet
printf 'fn greet(name: string) -> string {\nreturn str_concat(str_concat("Hi, ", name), "!");\n}\n' > libs/greet/greet.ns
cd app
expect_ok add "$NUSA_BIN" pkg add greet --path ../libs/greet
cd ..
cd app
expect_fail add-dup "already present" "$NUSA_BIN" pkg add greet --path ../libs/greet
cd ..
cd app
expect_ok update "$NUSA_BIN" pkg update
cd ..
grep -q 'name = "greet"' app/nusa.lock || fail "update" "lock entry"
grep -q 'sha256:' app/nusa.lock || fail "update" "lock checksum"
grep -q 'path = "../libs/greet"' app/nusa.lock || fail "update" "lock relpath"

# 4. build via paket + jalankan hasil.
printf 'import greet;\nfn main() {\nprint(greet("Nusa"));\n}\n' > app/main.ns
cd app
expect_ok build "$NUSA_BIN" pkg build
cd ..
[ -x app/target/myapp ] || fail "build" "target binary missing"
[ "$(./app/target/myapp)" = "Hi, Nusa!" ] || fail "build" "wrong output"

# 5. Stale lock terdeteksi; update ulang memulihkan.
echo "// touch" >> libs/greet/greet.ns
cd app
expect_fail stale "lock out of date" "$NUSA_BIN" pkg build
cd ..
cd app
expect_ok update2 "$NUSA_BIN" pkg update
cd ..
cd app
expect_ok build2 "$NUSA_BIN" pkg build
cd ..

# 6. publish --dry-run (build + tarball + sha).
cd app
expect_ok pub "$NUSA_BIN" pkg publish --dry-run
cd ..
ls app/dist/myapp-0.1.0.tar.gz || fail "pub" "tarball missing"

# 6. remove + registry-dep diarahkan.
cd app
expect_ok remove "$NUSA_BIN" pkg remove greet
cd ..
grep -q "greet" app/nusa.toml && fail "remove" "dep still present"
cd app
expect_ok add-ver "$NUSA_BIN" pkg add bad 1.2.3
cd ..
cd app
expect_fail registry "set \$NUSA_REGISTRY or --registry" "$NUSA_BIN" pkg update
cd ..
cd app
expect_ok remove-bad "$NUSA_BIN" pkg remove bad
cd ..
cd app
expect_ok update3 "$NUSA_BIN" pkg update
cd ..

# 7. Konflik versi transitif.
expect_ok libA "$NUSA_BIN" pkg init dep --dir libs/depA
printf 'fn x() -> i64 {\nreturn 1;\n}\n' > libs/depA/dep.ns
expect_ok libB "$NUSA_BIN" pkg init dep --dir libs/depB
printf '[package]\nname = "dep"\nversion = "9.9.9"\n\n[dependencies]\n' > libs/depB/nusa.toml
printf 'fn x() -> i64 {\nreturn 2;\n}\n' > libs/depB/dep.ns
expect_ok mid-init "$NUSA_BIN" pkg init mid --dir mid
cd mid
expect_ok mid-a "$NUSA_BIN" pkg add dep --path ../libs/depA
cd ..
expect_ok top-init "$NUSA_BIN" pkg init top --dir top
cd top
expect_ok top-mid "$NUSA_BIN" pkg add mid --path ../mid
cd ..
cd top
expect_ok top-b "$NUSA_BIN" pkg add dep --path ../libs/depB
cd ..
cd top
expect_fail conflict "version conflict for 'dep'" "$NUSA_BIN" pkg update
cd ..

# 8. (publish diuji di atas sebagai kasus 6b.)
echo "E2E PKG PASS ($PASS_N cases)"
