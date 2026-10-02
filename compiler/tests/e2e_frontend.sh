#!/usr/bin/env bash
# Integration test front-end:
#  - `let` + ekspresi harus LOLOS parser. Sejak v0.3.0 codegen juga
#    mendukungnya, jadi expr.ns harus build + run dengan sukses.
#  - syntax yang benar-benar salah tetap gagal di parser dengan diagnostic.
set -u
NUSA_BIN="${1:?usage: e2e_frontend.sh <nusa-binary>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

cp "$ROOT/examples/expr.ns" ./expr.ns

# 1. expr.ns: front-end menerima DAN (v0.3.0+) codegen mengeksekusi.
#    let x = 10 + 20 * 3 dihitung (70) walau tidak dicetak; output terlihat:
OUT="$("$NUSA_BIN" run expr.ns 2>expr.log)" || { echo "E2E FAIL: expr.ns should run"; cat expr.log; exit 1; }
[ "$OUT" = "Hello, Nusa!" ] || { echo "E2E FAIL: expr.ns output '$OUT'"; exit 1; }

# 2. Precedence spot-check via front-end: operator salah tempat -> parse error
printf 'fn main() { let x = 10 + ; }\n' > badexpr.ns
if "$NUSA_BIN" build badexpr.ns -o badexpr_out 2>badexpr.log; then
  echo "E2E FAIL: malformed expr should not build"
  exit 1
fi
grep -q "expected expression" badexpr.log || { echo "E2E FAIL: expected 'expected expression'"; cat badexpr.log; exit 1; }

# 3. `let x == 3;` -> parser menolak (bukan codegen)
printf 'fn main() { let x == 3; }\n' > deq.ns
if "$NUSA_BIN" build deq.ns -o deq_out 2>deq.log; then
  echo "E2E FAIL: 'let x == 3' should not build"
  exit 1
fi
grep -q "expected '=' after variable name" deq.log || { echo "E2E FAIL: expected '=' diagnostic"; cat deq.log; exit 1; }

# 4. `return 1;` di main lolos parser, ditolak typechecker (void function)
printf 'fn main() { return 1; }\n' > ret.ns
if "$NUSA_BIN" build ret.ns -o ret_out 2>ret.log; then
  echo "E2E FAIL: 'return 1' in main should not build"
  exit 1
fi
grep -q "void function" ret.log || { echo "E2E FAIL: expected void-function error"; cat ret.log; exit 1; }

echo "E2E FRONTEND PASS"
