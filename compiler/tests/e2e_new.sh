#!/usr/bin/env bash
# Integration test v1.20.0 (Phase 43): project templates `nusa new`.
# Setiap template: scaffold -> file ada -> build+run output eksak
# (library/server: `nusa test` hijau) + kasus negatif CLI.
set -u
NUSA_BIN="${1:?usage: e2e_new.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }

# 0. Daftar template = 4 master.
LIST="$("$NUSA_BIN" new --list 2>/dev/null)" || fail "list" "rc != 0"
for t in hello-world cli-app library server; do
  echo "$LIST" | grep -q "$t" || fail "list" "missing $t"
done
echo "case list OK"

# 1. hello-world: default nama (dash -> underscore) + run.
"$NUSA_BIN" new hello-world > /dev/null 2>&1 || fail "hello" "new"
[ -f hello_world/main.ns ] && [ -f hello_world/nusa.toml ] || fail "hello" "files"
grep -q 'name = "hello_world"' hello_world/nusa.toml || fail "hello" "manifest"
OUT="$("$NUSA_BIN" run hello_world/main.ns 2>/dev/null)" || fail "hello" "run"
[ "$OUT" = "Hello, hello_world!" ] || { echo "$OUT"; fail "hello" "output"; }
echo "case hello-world OK"

# 2. cli-app: dua cabang argv.
"$NUSA_BIN" new cli-app mycli > /dev/null 2>&1 || fail "cli" "new"
OUT="$("$NUSA_BIN" run mycli/main.ns 2>/dev/null)" || fail "cli" "run"
[ "$OUT" = "usage: mycli <name>" ] || { echo "$OUT"; fail "cli" "usage"; }
OUT="$("$NUSA_BIN" run mycli/main.ns Ada 2>/dev/null)" || fail "cli" "run-arg"
[ "$OUT" = "Hello, Ada" ] || { echo "$OUT"; fail "cli" "greet"; }
echo "case cli-app OK"

# 3. library: `nusa test` hijau (import sibling).
"$NUSA_BIN" new library mylib > /dev/null 2>&1 || fail "lib" "new"
[ -f mylib/calc.ns ] && [ -f mylib/calc_test.ns ] || fail "lib" "files"
"$NUSA_BIN" test mylib 2>&1 | grep -q "1 passed, 0 failed" || fail "lib" "test"
echo "case library OK"

# 4. server: serve-once nyata (server Nusa bg + client python).
"$NUSA_BIN" new server mysrv > /dev/null 2>&1 || fail "srv" "new"
"$NUSA_BIN" test mysrv 2>&1 | grep -q "1 passed, 0 failed" || fail "srv" "test"
"$NUSA_BIN" build mysrv/main.ns -o mysrv/server > /dev/null 2>&1 || fail "srv" "build"
./mysrv/server 18953 > srv.got 2>&1 &
SRV_PID=$!
python3 -c "import socket; s=socket.create_connection(('127.0.0.1',18953),timeout=15); s.sendall(b'/'); assert s.recv(64) == b'Hello, Nusa!'; s.close()" || { kill $SRV_PID 2>/dev/null; fail "srv" "client"; }
wait $SRV_PID || fail "srv" "server rc"
grep -q "serving" srv.got || { cat srv.got; fail "srv" "output"; }
echo "case server OK"

# 5. Negatif: template asing, nama invalid, dir tak-kosong.
"$NUSA_BIN" new desktop-app > /dev/null 2>&1 && fail "neg" "bad template"
"$NUSA_BIN" new hello-world '9bad' > /dev/null 2>&1 && fail "neg" "bad name"
"$NUSA_BIN" new hello-world mycli > /dev/null 2>&1 && fail "neg" "non-empty dir"
echo "case negative OK"

# 6. Manifest valid + fmt bersih + pkg build (audit Phase 43).
# `compiler =` = versi pen-scaffold; semua .ns scaffold lolos fmt.
# Phase 84: label bisa membawa suffix pra-rilis (-beta).
VER="$("$NUSA_BIN" --version | sed -E 's/^nusa version ([0-9.]+(-[A-Za-z0-9.]+)?).*/\1/')"
grep -q "compiler = \"$VER\"" hello_world/nusa.toml || fail "manifest" "compiler version"
for f in hello_world/main.ns mycli/main.ns mylib/calc.ns \
         mylib/calc_test.ns mysrv/main.ns mysrv/handler.ns \
         mysrv/server_test.ns; do
  "$NUSA_BIN" fmt --check "$f" > /dev/null 2>&1 || fail "fmt" "$f"
done
echo "case manifest-fmt OK"
(cd hello_world && "$NUSA_BIN_ABS" pkg build > /dev/null 2>&1) || fail "pkg" "hello build"
(cd mycli && "$NUSA_BIN_ABS" pkg build > /dev/null 2>&1) || fail "pkg" "cli build"
(cd mysrv && "$NUSA_BIN_ABS" pkg build > /dev/null 2>&1) || fail "pkg" "srv build"
(cd mylib && "$NUSA_BIN_ABS" pkg build > /dev/null 2>&1) && fail "pkg" "lib build should fail (no main)"
echo "case pkg-build OK"

echo "E2E NEW PASS (8 cases)"
