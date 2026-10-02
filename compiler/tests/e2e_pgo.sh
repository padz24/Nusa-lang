#!/usr/bin/env bash
# PGO conformance v3.5.0 (Phase 104, milestone 1): instrumented build,
# workload, merge, optimized build — via flag driver ke clang
# (LLVM tetap optimizer). `--profile` milik Phase 62 -> namespace
# `--pgo-*` (stabilitas CLI, didokumentasikan di docs/pgo-3.5.md).
set -u
NUSA_BIN="${1:?usage: e2e_pgo.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1
cat > work.ns <<'EOF'
fn fib(n: i64) -> i64 {
    if n < 2 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
fn main() {
    print(fib(20));
}
EOF

# 1. Loop penuh: generate -> run workload -> merge -> use -> run.
timeout 180 "$NUSA_BIN" build work.ns -o work_inst --pgo-generate \
  >gen.log 2>&1 || { cat gen.log; fail "pgo-generate" "build gagal"; }
rm -f *.profraw
timeout 60 ./work_inst >inst.got 2>inst.err \
  || { cat inst.err; fail "pgo-run-inst" "run instrumented gagal"; }
printf '6765\n' | cmp -s inst.got - \
  || { cat inst.got; fail "pgo-run-inst" "output salah"; }
ls *.profraw >/dev/null 2>&1 || fail "pgo-run-inst" "tanpa .profraw"
command -v llvm-profdata >/dev/null 2>&1 \
  || fail "pgo-merge" "llvm-profdata hilang"
llvm-profdata merge -o work.profdata ./*.profraw >merge.log 2>&1 \
  || { cat merge.log; fail "pgo-merge" "merge gagal"; }
timeout 180 "$NUSA_BIN" build work.ns -o work_opt --pgo-use=work.profdata \
  >use.log 2>&1 || { cat use.log; fail "pgo-use" "build gagal"; }
timeout 60 ./work_opt >opt.got 2>opt.err \
  || { cat opt.err; fail "pgo-run-opt" "run optimized gagal"; }
printf '6765\n' | cmp -s opt.got - \
  || { cat opt.got; fail "pgo-run-opt" "output salah"; }
pass "pgo-loop"

# 2. Profil hilang -> error bersih (pre-clang).
timeout 60 "$NUSA_BIN" build work.ns -o bad --pgo-use=nope.profdata \
  >missing.log 2>&1 && fail "pgo-missing" "rc=0 untuk profil hilang"
grep -q "not found" missing.log \
  || { cat missing.log; fail "pgo-missing" "pesan tak jelas"; }
pass "pgo-missing"

# 3. Kedua flag -> eksklusif.
timeout 60 "$NUSA_BIN" build work.ns -o both --pgo-generate \
  --pgo-use=work.profdata >both.log 2>&1 \
  && fail "pgo-exclusive" "rc=0 untuk kombinasi"
grep -q "exclusive" both.log \
  || { cat both.log; fail "pgo-exclusive" "pesan tak jelas"; }
pass "pgo-exclusive"

# 4. Mode PGO = flag cache: base Fresh, generate wajib rebuild.
rm -rf .nusa-cache
timeout 180 "$NUSA_BIN" build work.ns -o cacheme >c1.log 2>&1 \
  || { cat c1.log; fail "pgo-cache" "build base gagal"; }
timeout 60 "$NUSA_BIN" build work.ns -o cacheme >c2.log 2>&1 \
  || { cat c2.log; fail "pgo-cache" "rebuild base gagal"; }
grep -q "Fresh" c2.log || { cat c2.log; fail "pgo-cache" "cache base hilang"; }
timeout 180 "$NUSA_BIN" build work.ns -o cacheme --pgo-generate \
  >c3.log 2>&1 || { cat c3.log; fail "pgo-cache" "build generate gagal"; }
grep -q "Fresh" c3.log && { cat c3.log; fail "pgo-cache" "biner instrumented tertukar cache"; }
pass "pgo-cache"

echo "E2E PGO PASS ($PASS_N cases)"
