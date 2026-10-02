#!/usr/bin/env bash
# Async runtime conformance v2.8.0 (Phase 57, milestone 1).
# Fondasi library (`std/tasks.ns`, tanpa sintaks baru) — SEMUA kasus
# deterministik via await/join (tanpa asersi timing).
set -u
NUSA_BIN="${1:?usage: e2e_async.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

run_ok() {
  local name="$1"
  timeout 60 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run gagal"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "output salah"; }
  pass "$name"
}

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== future: await + poll =="
cat > a_await.ns <<'EOF'
import std.tasks;
fn fortytwo() -> i64 {
    return 40 + 2;
}
fn main() {
    let fu = task_spawn(fortytwo);
    while future_poll(fu) == false {
        future_poll(fu);
    }
    print(future_await(fu));
}
EOF
printf '42\n' > a_await.want
run_ok a_await

echo "== cancellation kooperatif + shutdown =="
cat > a_cancel.ns <<'EOF'
import std.tasks;
fn waiter(tok: Cancel, c: i64) -> i64 {
    while is_cancelled(tok) == false {
        is_cancelled(tok);
    }
    chan_send(c, 777);
    return 0;
}
fn main() {
    let tok = cancel_new();
    let c = chan_new();
    let h = thread_spawn(|| waiter(tok, c));
    cancel(tok);
    print(chan_recv(c));
    thread_join(h);
    chan_free(c);
    cancel_free(tok);
}
EOF
printf '777\n' > a_cancel.want
run_ok a_cancel

cat > a_shutdown.ns <<'EOF'
import std.tasks;
fn serve(tok: Cancel, c: i64, tag: i64) -> i64 {
    while is_cancelled(tok) == false {
        is_cancelled(tok);
    }
    chan_send(c, tag);
    return 0;
}
fn main() {
    let tok = cancel_new();
    let c = chan_new();
    let h1 = thread_spawn(|| serve(tok, c, 1));
    let h2 = thread_spawn(|| serve(tok, c, 2));
    cancel(tok);
    print(chan_recv(c) + chan_recv(c));
    thread_join(h1);
    thread_join(h2);
    print(0);
    chan_free(c);
    cancel_free(tok);
}
EOF
printf '3\n0\n' > a_shutdown.want
run_ok a_shutdown

echo "== konvensi error + stress =="
cat > a_error.ns <<'EOF'
import std.tasks;
fn may_fail() -> i64 {
    return 0 - 1;
}
fn main() {
    let fu = task_spawn(may_fail);
    let code = future_await(fu);
    if code < 0 {
        print(1);
    } else {
        print(0);
    }
}
EOF
printf '1\n' > a_error.want
run_ok a_error

cat > a_stress.ns <<'EOF'
import std.tasks;
fn claim_work(a: i64) -> i64 {
    let i = atomic_add(a, 1) - 1;
    return i * 10;
}
fn main() {
    let next = atomic_new(0);
    let f0 = task_spawn(|| claim_work(next));
    let f1 = task_spawn(|| claim_work(next));
    let f2 = task_spawn(|| claim_work(next));
    let f3 = task_spawn(|| claim_work(next));
    let f4 = task_spawn(|| claim_work(next));
    let f5 = task_spawn(|| claim_work(next));
    let f6 = task_spawn(|| claim_work(next));
    let f7 = task_spawn(|| claim_work(next));
    let sum = future_await(f0) + future_await(f1) + future_await(f2) + future_await(f3);
    sum = sum + future_await(f4) + future_await(f5) + future_await(f6) + future_await(f7);
    print(sum);
    atomic_free(next);
}
EOF
printf '280\n' > a_stress.want
run_ok a_stress

echo "== misuse + tooling =="
cat > a_double_await.ns <<'EOF'
import std.tasks;
fn fortytwo() -> i64 {
    return 42;
}
fn main() {
    let fu = task_spawn(fortytwo);
    print(future_await(fu));
    print(future_await(fu));
}
EOF
timeout 30 "$NUSA_BIN" run a_double_await.ns >a_double_await.got 2>a_double_await.err
rc=$?
if crashed "$rc"; then cat a_double_await.err; fail "a_double_await" "CRASH (rc=$rc)"; fi
[ $rc -eq 1 ] || fail "a_double_await" "harap exit 1 (rc=$rc)"
grep -q "invalid handle" a_double_await.err || { cat a_double_await.err; fail "a_double_await" "pesan hilang"; }
pass "a_double_await"

"$NUSA_BIN" fmt --check "$SRC_DIR/std/tasks.ns" >/dev/null 2>&1 || fail "a_fmt" "std/tasks.ns tak bersih"
pass "a_fmt"
"$NUSA_BIN" doc "$SRC_DIR/std/tasks.ns" 2>/dev/null | grep -q "task_spawn" || fail "a_doc" "doc hilang"
pass "a_doc"

echo "E2E ASYNC PASS ($PASS_N cases)"
