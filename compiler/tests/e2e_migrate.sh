#!/usr/bin/env bash
# Integration test v3.0-audit (Phase 83): `nusa migrate` + audit korpus.
# Repo 2.x: 82 compatible / 3 deprecated / 0 requires migration /
# 0 broken / 2 unsupported (87 file); fixture: 5 kategori + exit +
# read-only (NOL file ditulis).
set -u
NUSA_BIN="${1:?usage: e2e_migrate.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# 1. Audit repo: angka pasti §6 migration-3.0.md.
"$NUSA_BIN" migrate "$ROOT" >repo.log 2>&1 \
  || fail "migrate-repo" "rc != 0 (korpus harus bebas R/B)"
grep -q "82 compatible, 3 deprecated, 0 requires migration, 0 broken, 2 unsupported (87 files)" repo.log \
  || { tail -n 2 repo.log; fail "migrate-repo" "ringkasan berubah"; }
grep -q "examples/concurrency.ns: deprecated (program)" repo.log \
  || fail "migrate-repo" "concurrency tak deprecated"
grep -q "packages/tls/tls.ns: unsupported (library)" repo.log \
  || fail "migrate-repo" "tls tak unsupported"
grep -q "packages/sqlite/sqlite.ns: unsupported (library)" repo.log \
  || fail "migrate-repo" "sqlite tak unsupported"
pass "migrate-repo"

# 2. Fixture 5 kategori.
mkdir -p proj/nativelib
printf 'fn main() {\n print(42);\n}\n' > proj/good.ns
printf 'fn helper(x: i64) -> i64 {\n return x * 2;\n}\n' > proj/lib.ns
printf 'fn main() {\n let c = chan_new();\n chan_send(c, 7);\n print(chan_recv(c));\n chan_free(c);\n}\n' > proj/dep.ns
printf 'fn main() {\n print(none(0));\n}\n' > proj/old.ns
printf 'fn main() {\n print(no_such_fn(1));\n}\n' > proj/brok.ns
printf 'fn pad(x: i64) -> i64 {\n return x;\n}\n' > proj/nativelib/nl.ns
printf '[package]\nname = "nl"\nversion = "0.1.0"\n\n[capabilities]\nnative = true\n' > proj/nativelib/nusa.toml
"$NUSA_BIN" migrate proj >proj.log 2>&1
[ $? -eq 1 ] || { cat proj.log; fail "migrate-fixture" "rc harus 1 (R+B hadir)"; }
grep -q "proj/good.ns: compatible (program)" proj.log || fail "migrate-fixture" "good"
grep -q "proj/lib.ns: compatible (library)" proj.log || fail "migrate-fixture" "lib"
grep -q "proj/dep.ns: deprecated (program)" proj.log || fail "migrate-fixture" "dep"
grep -q "proj/old.ns: requires migration (check-failed)" proj.log || fail "migrate-fixture" "old"
grep -q "proj/brok.ns: broken (check-failed)" proj.log || fail "migrate-fixture" "brok"
grep -q "proj/nativelib/nl.ns: unsupported (library)" proj.log || fail "migrate-fixture" "native"
grep -q "2 compatible, 1 deprecated, 1 requires migration, 1 broken, 1 unsupported (6 files)" proj.log \
  || { tail -n 1 proj.log; fail "migrate-fixture" "ringkasan fixture"; }
pass "migrate-fixture"

# 3. Read-only: pohon tak berubah + file tunggal rc.
find proj -type f | sort > before.txt
"$NUSA_BIN" migrate proj >/dev/null 2>&1
find proj -type f | sort > after.txt
cmp -s before.txt after.txt || fail "migrate-readonly" "file berubah/bertambah"
"$NUSA_BIN" migrate proj/good.ns | grep -q "(1 files)" \
  || fail "migrate-single" "mode file tunggal"
pass "migrate-readonly"

echo "E2E MIGRATE PASS ($PASS_N cases)"
