#!/usr/bin/env bash
# Integration test v0.1.0: nusa build + run hello.ns -> "Hello, Nusa!"
set -u
NUSA_BIN="${1:?usage: e2e_hello.sh <nusa-binary>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

cp "$ROOT/examples/hello.ns" "$TMPDIR/hello.ns"
cd "$TMPDIR" || exit 1

"$NUSA_BIN" build hello.ns -o hello > build.log 2>&1
rc=$?
cat build.log
if [ $rc -ne 0 ]; then
  echo "E2E FAIL: nusa build exited $rc"
  exit 1
fi

if [ ! -x ./hello ]; then
  echo "E2E FAIL: ./hello not found or not executable"
  exit 1
fi

OUT="$(./hello)"
if [ "$OUT" != "Hello, Nusa!" ]; then
  echo "E2E FAIL: expected 'Hello, Nusa!' got '$OUT'"
  exit 1
fi

# nusa run juga harus mencetak hal yang sama
OUT2="$("$NUSA_BIN" run hello.ns 2>/dev/null)"
if [ "$OUT2" != "Hello, Nusa!" ]; then
  echo "E2E FAIL: 'nusa run' expected 'Hello, Nusa!' got '$OUT2'"
  exit 1
fi

# LLVM IR harus ada dan memuat @main + @puts
if [ ! -f hello.ns.ll ]; then
  echo "E2E FAIL: hello.ns.ll not generated"
  exit 1
fi
grep -q "define i32 @main(i32" hello.ns.ll || { echo "E2E FAIL: IR missing @main"; exit 1; }
grep -q "@puts" hello.ns.ll || { echo "E2E FAIL: IR missing @puts"; exit 1; }

# Negative test: malformed source tidak boleh crash, harus error rapi
printf 'fn main() { print( ); }' > bad.ns
if "$NUSA_BIN" build bad.ns -o bad_out 2>bad.log; then
  echo "E2E FAIL: malformed source should not build successfully"
  exit 1
fi
cat bad.log
echo "E2E PASS"
