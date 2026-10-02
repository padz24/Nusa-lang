#!/usr/bin/env bash
# Parallel conformance v3.22.0 (Phase 121, milestone 1): `std/parallel`
# (partisi block statik). Determinisme di bawah scheduling dibuktikan
# pengulangan (hasil = jumlah, urutan tak relevan).
set -u
NUSA_BIN="${1:?usage: e2e_parallel.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Berkas uji repo hijau.
timeout 180 "$NUSA_BIN" test "$SRC_DIR/tests/parallel_test.ns" >t.log 2>&1 \
  || { cat t.log; fail "parallel-test" "rc != 0"; }
grep -q "1 passed, 0 failed" t.log \
  || { cat t.log; fail "parallel-test" "ringkasan salah"; }
pass "parallel-test"

# 2. Deterministik 3x di bawah scheduling (worker Affairs OS).
cat > det.ns <<'EOF'
import parallel;
fn sq(x: i64) -> i64 {
    return x * x;
}
fn main() {
    print(par_map_sum(200, 7, sq));
}
EOF
for i in 1 2 3; do
  timeout 120 "$NUSA_BIN" run det.ns >"det$i.got" 2>"det$i.err" \
    || { cat "det$i.err"; fail "parallel-det" "run $i gagal"; }
  printf '2646700\n' | cmp -s "det$i.got" - \
    || { cat "det$i.got"; fail "parallel-det" "run $i tak-eksak"; }
done
pass "parallel-det"

# 3. `import std.parallel` kanonis + fmt bersih.
printf 'import std.parallel;\nfn id(x: i64) -> i64 {\n    return x;\n}\nfn main() {\n    print(par_map_sum(10, 2, id));\n}\n' > canon.ns
timeout 120 "$NUSA_BIN" run canon.ns >canon.got 2>canon.err \
  || { cat canon.err; fail "parallel-canon" "import kanonis gagal"; }
printf '45\n' | cmp -s canon.got - \
  || { cat canon.got; fail "parallel-canon" "output salah"; }
timeout 60 "$NUSA_BIN" fmt --check "$SRC_DIR/std/parallel.ns" \
  >fmt.log 2>&1 || { cat fmt.log; fail "parallel-fmt" "stdlib tak fmt-clean"; }
pass "parallel-canon"

echo "E2E PARALLEL PASS ($PASS_N cases)"
