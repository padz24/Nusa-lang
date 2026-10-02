#!/usr/bin/env bash
# WASI conformance v3.12.0 (Phase 111, milestone 1): model kapabilitas
# eksplisit. Komputasi murni = compile-only (pra-3.12, dikunci ulang);
# subsistem tanpa pemetaan WASI ditolak eksplisit saat LINK (dengan
# atau tanpa sysroot); inspeksi tak pernah ditolak.
set -u
NUSA_BIN="${1:?usage: e2e_wasi.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1
cp "$NUSA_BIN_ABS/../../examples/hello.ns" . 2>/dev/null || cp "${NUSA_BIN_ABS%/*}/../examples/hello.ns" hello.ns 2>/dev/null || true
[ -f hello.ns ] || printf 'fn main() {\n    print("Hello, Nusa!");\n}\n' > hello.ns

# 1. Komputasi murni: pesan sysroot tetap (kompat e2e_cross).
timeout 120 "$NUSA_BIN" build hello.ns --target wasm32 -o wbin \
  >pure.log 2>&1 && fail "wasi-pure" "rc=0 tanpa sysroot"
grep -q "needs --sysroot" pure.log \
  || { cat pure.log; fail "wasi-pure" "pesan sysroot hilang"; }
pass "wasi-pure"

# 2. Threads saat link (dengan sysroot dummy) -> tolak eksplisit.
cat > th.ns <<'EOF'
fn work() -> i64 {
    return 40 + 2;
}
fn main() {
    let h = thread_spawn(work);
    print(thread_join(h));
}
EOF
timeout 120 "$NUSA_BIN" build th.ns --target wasm32 --sysroot "$TMPDIR" \
  -o thw >th.log 2>&1 && fail "wasi-thread" "rc=0 untuk threads"
grep -q "cannot link program using: threads" th.log \
  || { cat th.log; fail "wasi-thread" "kapabilitas tak disebut"; }
pass "wasi-thread"

# 3. Networking saat link -> tolak eksplisit (nama subsistem).
cat > net.ns <<'EOF'
fn main() {
    let c = tcp_connect("127.0.0.1", 80);
    print(c);
}
EOF
timeout 120 "$NUSA_BIN" build net.ns --target wasm32 --sysroot "$TMPDIR" \
  -o netw >net.log 2>&1 && fail "wasi-net" "rc=0 untuk networking"
grep -q "networking" net.log \
  || { cat net.log; fail "wasi-net" "kapabilitas tak disebut"; }
pass "wasi-net"

# 4. Inspeksi program terblokir tetap diizinkan (bukan buntu total).
timeout 120 "$NUSA_BIN" build th.ns --target wasm32 --emit-ir -o th.ll \
  >ir.log 2>&1 || { cat ir.log; fail "wasi-inspect" "emit-ir ditolak"; }
grep -q 'target triple = "wasm32-unknown-unknown"' th.ll \
  || fail "wasi-inspect" "triple hilang"
timeout 120 "$NUSA_BIN" build th.ns --target wasm32 -c -o th.o \
  >obj.log 2>&1 || { cat obj.log; fail "wasi-inspect" "-c ditolak"; }
file th.o | grep -qi "webassembly" \
  || fail "wasi-inspect" "objek bukan wasm: $(file th.o)"
pass "wasi-inspect"

echo "E2E WASI PASS ($PASS_N cases)"
