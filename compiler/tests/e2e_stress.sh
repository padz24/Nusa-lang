#!/usr/bin/env bash
# Concurrency stress + observability v2.7.0 (Phase 56, milestone 1).
# SEMUA kasus deterministik via join/collect (tanpa asersi timing).
# Gagal-bersih = exit != 0 tanpa sinyal (rc > 128 = CRASH).
set -u
NUSA_BIN="${1:?usage: e2e_stress.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

run_ok() {
  local name="$1"
  timeout 120 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run gagal"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "output salah"; }
  pass "$name"
}

expect_fail() {
  local name="$1" pattern="$2"
  timeout 30 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err"
  local rc=$?
  if crashed "$rc"; then cat "$name.err"; fail "$name" "CRASH (rc=$rc)"; fi
  [ $rc -eq 1 ] || fail "$name" "harap exit 1 (rc=$rc)"
  grep -q "$pattern" "$name.err" || { cat "$name.err"; fail "$name" "pola '$pattern' hilang"; }
  pass "$name"
}

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== chan_len observability =="
cat > s_len.ns <<'EOF'
fn main() {
    let c = chan_new();
    print(chan_len(c));
    chan_send(c, 1);
    chan_send(c, 2);
    chan_send(c, 3);
    print(chan_len(c));
    print(chan_recv(c));
    print(chan_len(c));
    print(chan_recv(c));
    print(chan_recv(c));
    print(chan_len(c));
    chan_free(c);
}
EOF
printf '0\n3\n1\n2\n2\n3\n0\n' > s_len.want
run_ok s_len

cat > s_poll.ns <<'EOF'
fn produce(c: i64) -> i64 {
    let i = 0;
    while i < 10 {
        chan_send(c, i);
        i = i + 1;
    }
    return 0;
}
fn main() {
    let c = chan_new();
    let h = thread_spawn(|| produce(c));
    let sum = 0;
    let got = 0;
    while got < 10 {
        if chan_len(c) > 0 {
            sum = sum + chan_recv(c);
            got = got + 1;
        }
    }
    thread_join(h);
    print(sum);
    print(chan_len(c));
    chan_free(c);
}
EOF
printf '45\n0\n' > s_poll.want
run_ok s_poll

echo "== stress deterministik =="
cat > s_fan.ns <<'EOF'
fn flood(c: i64) -> i64 {
    let i = 0;
    while i < 25 {
        chan_send(c, 1);
        i = i + 1;
    }
    return 0;
}
fn main() {
    let c = chan_new();
    let h1 = thread_spawn(|| flood(c));
    let h2 = thread_spawn(|| flood(c));
    let h3 = thread_spawn(|| flood(c));
    let h4 = thread_spawn(|| flood(c));
    let sum = 0;
    let i = 0;
    while i < 100 {
        sum = sum + chan_recv(c);
        i = i + 1;
    }
    thread_join(h1);
    thread_join(h2);
    thread_join(h3);
    thread_join(h4);
    print(sum);
    print(chan_len(c));
    chan_free(c);
}
EOF
printf '100\n0\n' > s_fan.want
run_ok s_fan

cat > s_atomic.ns <<'EOF'
fn hammer(a: i64) -> i64 {
    let i = 0;
    while i < 2000 {
        atomic_add(a, 1);
        i = i + 1;
    }
    return 0;
}
fn main() {
    let a = atomic_new(0);
    let h1 = thread_spawn(|| hammer(a));
    let h2 = thread_spawn(|| hammer(a));
    let h3 = thread_spawn(|| hammer(a));
    let h4 = thread_spawn(|| hammer(a));
    let h5 = thread_spawn(|| hammer(a));
    let h6 = thread_spawn(|| hammer(a));
    let h7 = thread_spawn(|| hammer(a));
    let h8 = thread_spawn(|| hammer(a));
    thread_join(h1);
    thread_join(h2);
    thread_join(h3);
    thread_join(h4);
    thread_join(h5);
    thread_join(h6);
    thread_join(h7);
    thread_join(h8);
    print(atomic_get(a));
    atomic_free(a);
}
EOF
printf '16000\n' > s_atomic.want
run_ok s_atomic

cat > s_mutex.ns <<'EOF'
fn pound(m: i64, a: i64) -> i64 {
    let i = 0;
    while i < 500 {
        mutex_lock(m);
        atomic_add(a, 1);
        mutex_unlock(m);
        i = i + 1;
    }
    return 0;
}
fn main() {
    let m = mutex_new();
    let a = atomic_new(0);
    let h1 = thread_spawn(|| pound(m, a));
    let h2 = thread_spawn(|| pound(m, a));
    let h3 = thread_spawn(|| pound(m, a));
    let h4 = thread_spawn(|| pound(m, a));
    thread_join(h1);
    thread_join(h2);
    thread_join(h3);
    thread_join(h4);
    print(atomic_get(a));
    print(mutex_try_lock(m));
    mutex_unlock(m);
    mutex_free(m);
    atomic_free(a);
}
EOF
printf '2000\n1\n' > s_mutex.want
run_ok s_mutex

echo "== misuse + audit async =="
cat > s_len_freed.ns <<'EOF'
fn main() {
    let c = chan_new();
    chan_free(c);
    print(chan_len(c));
}
EOF
expect_fail s_len_freed "invalid handle"

cat > s_async.ns <<'EOF'
fn main() {
    let async = 1;
}
EOF
timeout 20 "$NUSA_BIN" check s_async.ns >s_async.log 2>&1
rc=$?
if crashed "$rc"; then fail "s_async" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "s_async" "async harus ditolak (reserved)"
pass "s_async"

echo "== double-join konkuren (Phase 116): tepat satu pemenang klaim =="
cat > s_racejoin.ns <<'EOF'
fn work() -> i64 {
    sleep_ms(400);
    return 7;
}
fn main() {
    let h = thread_spawn(work);
    let j = thread_spawn(|| thread_join(h));
    sleep_ms(100);
    print(thread_join(h));
    print(thread_join(j));
}
EOF
# Invarian tiap run (pemenang tak tentu, sifat tentu): rc=1 (yang kalah
# fail-fast), pesan invalid-handle, TANPA baris "0" (nilai diam-diam
# pra-fix), tanpa sinyal. Tepat-satu-klaim = tak ada double-free/UB
# pthread_join ganda.
for i in 1 2 3; do
  timeout 60 "$NUSA_BIN" run s_racejoin.ns >s_racejoin.got 2>s_racejoin.err
  rc=$?
  if crashed "$rc"; then cat s_racejoin.err; fail "s_racejoin" "CRASH (rc=$rc)"; fi
  [ $rc -eq 1 ] || { cat s_racejoin.got s_racejoin.err; fail "s_racejoin" "run $i: harap exit 1 (rc=$rc)"; }
  grep -q "invalid handle in thread_join" s_racejoin.err \
    || { cat s_racejoin.err; fail "s_racejoin" "run $i: pesan klaim hilang"; }
  if grep -qx "0" s_racejoin.got; then cat s_racejoin.got; fail "s_racejoin" "run $i: nilai 0 diam-diam"; fi
done
pass "s_racejoin"

echo "E2E STRESS PASS ($PASS_N cases)"
