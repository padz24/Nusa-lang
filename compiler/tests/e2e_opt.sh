#!/usr/bin/env bash
# Optimization conformance + benchmark v2.4.0 (Phase 53, milestone 1).
# - `compiler/mir/Opt.*` murni standar C++ (tanpa compiler/* lain).
# - Benchmark sintetik: kompilasi bench/mir_opt_main.cpp + jalankan,
#   asersi reduksi instruksi + verify pra/pasca.
# - Regresi `.ns`: pipeline existing tak tersentuh pass baru.
set -u
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
NUSA_BIN="${1:?usage: e2e_opt.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# 1. Lapisan: Opt.* hanya include mir/ + standar.
if grep -rn '#include "compiler/' "$SRC_DIR/compiler/mir/Opt."* 2>/dev/null | grep -v '"compiler/mir/' | grep -q .; then
  fail "opt-isolated" "Opt include lapisan lain"
fi
pass "opt-isolated"

# 2. Benchmark: build bench + run 3x (mandat: ulangi ≥3x).
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1
CXX="${CXX:-clang++}"
"$CXX" -std=c++17 -O2 -I"$SRC_DIR" "$SRC_DIR/bench/mir_opt_main.cpp" \
  "$SRC_DIR/compiler/mir/Mir.cpp" "$SRC_DIR/compiler/mir/Opt.cpp" \
  "$SRC_DIR/compiler/mir/Cfg.cpp" \
  -o mir_opt_bench 2>bench_build.log || { cat bench_build.log; fail "opt-bench-build" "kompilasi bench gagal"; }
pass "opt-bench-build"
LAST_AFTER=0
for i in 1 2 3; do
  ./mir_opt_bench 2000 >"bench_$i.log" 2>&1 || { cat "bench_$i.log"; fail "opt-bench-run" "run $i gagal"; }
  grep -q "verify_pre=1 verify_post=1" "bench_$i.log" || { cat "bench_$i.log"; fail "opt-verify" "verify gagal run $i"; }
  AFTER=$(sed -n 's/^instrs_after=//p' "bench_$i.log")
  BEFORE=$(sed -n 's/^instrs_before=//p' "bench_$i.log")
  [ -n "$AFTER" ] && [ "$AFTER" -lt "$BEFORE" ] || { cat "bench_$i.log"; fail "opt-reduce" "tak ada reduksi run $i"; }
  echo "run $i: before=$BEFORE after=$AFTER $(grep '^opt_us=' "bench_$i.log")"
  LAST_AFTER=$AFTER
done
pass "opt-bench-run"

# 3. Regresi `.ns` (pass MIR belum di-rewire ke pipeline).
cat > opt_reg.ns <<'EOF'
fn main() {
    print(20 + 10 * 3);
    let a: [i64] = [1, 2, 3];
    print(len(a));
}
EOF
timeout 30 "$NUSA_BIN" run opt_reg.ns >opt_reg.got 2>opt_reg.err || { cat opt_reg.err; fail "opt-regression" "run gagal"; }
printf '50\n3\n' | cmp -s opt_reg.got - || { cat opt_reg.got; fail "opt-regression" "output salah"; }
pass "opt-regression"

echo "E2E OPT PASS ($PASS_N cases)"
