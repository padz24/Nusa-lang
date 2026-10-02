#!/usr/bin/env bash
# Integration test v1.9.0 (Phase 28): concurrency end-to-end.
# Semua kasus runtime DETERMINISTIK (sinkronisasi via join/channel;
# tanpa asersi atas timing/sleep).
set -u
NUSA_BIN="${1:?usage: e2e_concurrency.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

run_ok() {
  local name="$1"
  timeout 60 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "wrong output"; }
  pass "$name"
}

expect_fail() {
  local name="$1" pattern="$2"
  "$NUSA_BIN" check "$name.ns" >"$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should not check (rc=0)"
  [ $rc -le 128 ] || fail "$name" "COMPILER CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

# 1. Spawn/join dua thread (hasil deterministik via join).
cat > v1.ns <<'EOF'
fn work() -> i64 {
    return 40 + 2;
}
fn main() {
    let h1 = thread_spawn(work);
    let h2 = thread_spawn(work);
    print(thread_join(h1));
    print(thread_join(h2));
}
EOF
printf '42\n42\n' > v1.want
run_ok v1

# 2. Closure capture lintas thread (env heap by-value).
cat > v2.ns <<'EOF'
fn main() {
    let base = 100;
    let w = || base + 1;
    let h1 = thread_spawn(w);
    let h2 = thread_spawn(w);
    print(thread_join(h1));
    print(thread_join(h2));
}
EOF
printf '101\n101\n' > v2.want
run_ok v2

# 3. Atomic counter 4 thread (total deterministik).
cat > v3.ns <<'EOF'
fn main() {
    let total = atomic_new(0);
    let i = 0;
    while i < 4 {
        let t = thread_spawn(|| atomic_add(total, 1));
        thread_join(t);
        i = i + 1;
    }
    print(atomic_get(total));
    atomic_free(total);
}
EOF
printf '4\n' > v3.want
run_ok v3

# 4. Channel pipeline: thread kirim 1..5, main jumlahkan (FIFO).
cat > v4.ns <<'EOF'
fn produce(c: i64) -> i64 {
    chan_send(c, 1);
    chan_send(c, 2);
    chan_send(c, 3);
    chan_send(c, 4);
    chan_send(c, 5);
    return 0;
}
fn main() {
    let c = chan_new();
    let h = thread_spawn(|| produce(c));
    let sum = 0;
    let i = 0;
    while i < 5 {
        sum = sum + chan_recv(c);
        i = i + 1;
    }
    thread_join(h);
    print(sum);
    chan_free(c);
}
EOF
printf '15\n' > v4.want
run_ok v4

# 5. Mutex try_lock sekuens + print terproteksi (himpunan terurut).
cat > v5.ns <<'EOF'
fn main() {
    let m = mutex_new();
    print(mutex_try_lock(m));
    print(mutex_try_lock(m));
    mutex_unlock(m);
    print(mutex_try_lock(m));
    mutex_unlock(m);
    let c = chan_new();
    let h1 = thread_spawn(|| guarded(m, c, 1));
    let h2 = thread_spawn(|| guarded(m, c, 2));
    print(chan_recv(c) + chan_recv(c));
    thread_join(h1);
    thread_join(h2);
    mutex_free(m);
    chan_free(c);
}
fn guarded(m: i64, c: i64, v: i64) -> i64 {
    mutex_lock(m);
    chan_send(c, v);
    mutex_unlock(m);
    return 0;
}
EOF
printf '1\n0\n1\n3\n' > v5.want
run_ok v5

# 6. Penolakan: spawn bukan fn()->i64.
cat > e1.ns <<'EOF'
fn main() {
    let h = thread_spawn(1);
}
EOF
expect_fail e1 "requires a 'fn() -> i64' argument"

cat > e2.ns <<'EOF'
fn f(x: i64) -> i64 {
    return x;
}
fn main() {
    let h = thread_spawn(f);
}
EOF
expect_fail e2 "requires a 'fn() -> i64' argument"

# 7. Penolakan: arity + handle.
cat > e3.ns <<'EOF'
fn main() {
    mutex_lock(1, 2);
}
EOF
expect_fail e3 "wrong number of arguments for 'mutex_lock'"

cat > e4.ns <<'EOF'
fn main() {
    print(chan_recv("c"));
}
EOF
expect_fail e4 "an 'i64' handle argument"

cat > e5.ns <<'EOF'
fn main() {
    let a = atomic_new("x");
}
EOF
expect_fail e5 "an 'i64' initial value"

# 8. sleep_ms smoke (selesai tanpa asersi timing).
cat > v6.ns <<'EOF'
fn main() {
    sleep_ms(5);
    print(1);
}
EOF
printf '1\n' > v6.want
run_ok v6

# 9. Channel bertag 2.0: string + bool + i64 tetap + mismatch runtime.
cat > v7.ns <<'EOF'
fn main() {
    let cs = chan_new_str();
    chan_send_str(cs, "hello");
    print(chan_recv_str(cs));
    let cb = chan_new_bool();
    chan_send_bool(cb, true);
    print(chan_recv_bool(cb));
    let ci = chan_new();
    chan_send(ci, 40 + 2);
    print(chan_recv(ci));
}
EOF
printf 'hello\ntrue\n42\n' > v7.want
run_ok v7

cat > e6.ns <<'EOF'
fn main() {
    let c = chan_new();
    chan_send_str(c, "x");
}
EOF
if timeout 15 "$NUSA_BIN_ABS" run e6.ns > e6.got 2>&1; then echo "E2E FAIL [e6]"; exit 1; fi
grep -q "channel type mismatch" e6.got || { cat e6.got; echo "E2E FAIL [e6]"; exit 1; }
pass "e6"

# Phase 118 (3.19.0): backpressure — bounded + try_send.
cat > e7.ns <<'EOF'
fn main() {
    let c = chan_new_bounded(2);
    print(chan_try_send(c, 1));
    print(chan_try_send(c, 2));
    print(chan_try_send(c, 3));
    print(chan_recv(c));
    print(chan_try_send(c, 4));
    print(chan_len(c));
    chan_free(c);
}
EOF
printf 'true\ntrue\nfalse\n1\ntrue\n2\n' > e7.want
run_ok e7

# Kapasitas tak-positif: fail-fast bersih (bukan rendezvous diam).
cat > e8.ns <<'EOF'
fn main() {
    let c = chan_new_bounded(0);
    print(1);
}
EOF
if timeout 15 "$NUSA_BIN_ABS" run e8.ns > e8.got 2>&1; then echo "E2E FAIL [e8]"; exit 1; fi
grep -q "channel capacity must be positive" e8.got || { cat e8.got; echo "E2E FAIL [e8]"; exit 1; }
pass "e8"

# try_send di channel tak-terbatas: selalu true.
cat > e9.ns <<'EOF'
fn main() {
    let c = chan_new();
    print(chan_try_send(c, 9));
    print(chan_recv(c));
    chan_free(c);
}
EOF
printf 'true\n9\n' > e9.want
run_ok e9

echo "E2E CONCURRENCY PASS ($PASS_N cases)"
