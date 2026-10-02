#!/usr/bin/env bash
# Runtime conformance v2.6.0 (Phase 55, milestone 1): modularisasi
# nol-perilaku + platform layer.
# - Simbol publik nusa_rt.o identik baseline (28 simbol).
# - Jalur fallback (tanpa nusa_rt.o) build+run OK.
# - Higiene platform: tanpa __ANDROID__/__BIONIC__/linux/ di luar
#   rt_platform.h; rt_platform.h ada.
# - Regresi `.ns` concurrency.
set -u
NUSA_BIN="${1:?usage: e2e_runtime.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$(dirname "$NUSA_BIN_ABS")"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# 1. Simbol publik identik baseline 3.24 (Fase 55+56+118+123:
# +chan_len, +chan_new_bounded/try_send; nusa_alloc/free weak = W).
nm "$BUILD_DIR/nusa_rt.o" | grep " [TVW] " | awk '{print $3}' | sort > /tmp/rt_syms.got 2>/dev/null || fail "rt-symbols" "nm gagal"
cat > /tmp/rt_syms.want <<'EOF'
nusa_alloc
nusa_atomic_add
nusa_atomic_free
nusa_atomic_get
nusa_atomic_new
nusa_atomic_set
nusa_chan_free
nusa_chan_len
nusa_chan_new
nusa_chan_new_bounded
nusa_chan_new_tagged
nusa_chan_recv
nusa_chan_recv_tagged
nusa_chan_send
nusa_chan_send_tagged
nusa_chan_try_send
nusa_crypto_rand_hex
nusa_free
nusa_mutex_free
nusa_mutex_lock
nusa_mutex_new
nusa_mutex_try_lock
nusa_mutex_unlock
nusa_random_u64
nusa_sha256_hex
nusa_sleep_ms
nusa_tcp_accept
nusa_tcp_close
nusa_tcp_connect
nusa_tcp_listen
nusa_tcp_recv
nusa_tcp_send
nusa_thread_join
nusa_thread_spawn
nusa_time_now_ms
nusa_time_now_s
EOF
cmp -s /tmp/rt_syms.got /tmp/rt_syms.want || { echo "--- got ---"; cat /tmp/rt_syms.got; fail "rt-symbols" "simbol berubah"; }
rm -f /tmp/rt_syms.got /tmp/rt_syms.want
pass "rt-symbols"

# 2. Higiene platform.
[ -f "$SRC_DIR/runtime/rt_platform.h" ] || fail "rt-platform" "rt_platform.h hilang"
if grep -rn '__ANDROID__\|__BIONIC__\|linux/' "$SRC_DIR/runtime/" --include='rt_*.c' --include='nusa_rt.c' 2>/dev/null | grep -q .; then
  fail "rt-platform" "kode platform-spesifik di luar rt_platform.h"
fi
for part in rt_handle rt_thread rt_sync rt_chan rt_time rt_crypto rt_net; do
  [ -f "$SRC_DIR/runtime/$part.c" ] || fail "rt-parts" "$part.c hilang"
done
pass "rt-platform"

# 3. Jalur fallback: sembunyikan nusa_rt.o sementara (trap restore).
TMPDIR="$(mktemp -d)"
trap 'mv "$BUILD_DIR/nusa_rt.o.hidden" "$BUILD_DIR/nusa_rt.o" 2>/dev/null; rm -rf "$TMPDIR"' EXIT
mv "$BUILD_DIR/nusa_rt.o" "$BUILD_DIR/nusa_rt.o.hidden" || fail "rt-fallback" "tak bisa sembunyikan nusa_rt.o"
cd "$TMPDIR" || exit 1
cp "$SRC_DIR/examples/hello.ns" .
timeout 60 "$NUSA_BIN" build hello.ns -o hello_fb >fb.log 2>&1 || { cat fb.log; fail "rt-fallback" "build fallback gagal"; }
[ "$(./hello_fb)" = "Hello, Nusa!" ] || fail "rt-fallback" "run fallback salah"
pass "rt-fallback"
mv "$BUILD_DIR/nusa_rt.o.hidden" "$BUILD_DIR/nusa_rt.o" || fail "rt-fallback" "restore gagal"
trap 'rm -rf "$TMPDIR"' EXIT

# 4. Regresi concurrency (registry + modular tetap benar).
cat > rt_conc.ns <<'EOF'
fn work() -> i64 {
    return 40 + 2;
}
fn main() {
    let h = thread_spawn(work);
    print(thread_join(h));
    let c = chan_new();
    chan_send(c, 5);
    print(chan_recv(c));
    chan_free(c);
}
EOF
timeout 30 "$NUSA_BIN" run rt_conc.ns >rt_conc.got 2>rt_conc.err || { cat rt_conc.err; fail "rt-regression" "run gagal"; }
printf '42\n5\n' | cmp -s rt_conc.got - || { cat rt_conc.got; fail "rt-regression" "output salah"; }
pass "rt-regression"

echo "E2E RUNTIME PASS ($PASS_N cases)"
