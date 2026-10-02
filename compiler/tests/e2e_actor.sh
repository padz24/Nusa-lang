#!/usr/bin/env bash
# Actor conformance v3.18.0 (Phase 117, milestone 1): model
# message-passing LEVEL-PUSTAKA (tanpa sintaks, tanpa RFC).
# State fungsional + mailbox terkunci; determinisme di bawah
# konkurensi dibuktikan total eksak.
set -u
NUSA_BIN="${1:?usage: e2e_actor.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Berkas uji repo hijau via `nusa test` (counter + isolasi + cast).
timeout 120 "$NUSA_BIN" test "$SRC_DIR/tests/actor_test.ns" >t.log 2>&1 \
  || { cat t.log; fail "actor-test" "rc != 0"; }
grep -q "1 passed, 0 failed" t.log \
  || { cat t.log; fail "actor-test" "ringkasan salah"; }
pass "actor-test"

# 2. Stres deterministik: 4 klien x 25 inc sinkron -> total eksak 100.
# Tanpa mutex-mailbox, kiriman 3-kata antar-klien interleave dan
# total meleset — kasus ini mengunci atomicity protokol.
cat > stress.ns <<'EOF'
import actor;
fn counter(op: i64, arg: i64, reply: i64, st: i64) -> i64 {
    if op == 1 {
        let nst = st + arg;
        chan_send(reply, nst);
        return nst;
    } else {
        chan_send(reply, st);
        return st;
    }
}
fn client(a: ActorRef, n: i64) -> i64 {
    let i = 0;
    while i < n {
        actor_call(a, 1, 1);
        i = i + 1;
    }
    return 0;
}
fn main() {
    let c = actor_spawn(counter, 0);
    let t1 = thread_spawn(|| client(c, 25));
    let t2 = thread_spawn(|| client(c, 25));
    let t3 = thread_spawn(|| client(c, 25));
    let t4 = thread_spawn(|| client(c, 25));
    thread_join(t1);
    thread_join(t2);
    thread_join(t3);
    thread_join(t4);
    print(actor_call(c, 2, 0));
}
EOF
timeout 120 "$NUSA_BIN" run stress.ns >stress.got 2>stress.err \
  || { cat stress.err; fail "actor-stress" "rc != 0"; }
printf '100\n' | cmp -s stress.got - \
  || { cat stress.got; fail "actor-stress" "total tak-eksak (protokol bocor)"; }
pass "actor-stress"

# 3. `import std.actor` kanonis + fmt bersih repo.
printf 'import std.actor;\nfn main() {\n    let c = actor_spawn(counter0, 0);\n    print(actor_call(c, 2, 0));\n}\nfn counter0(op: i64, arg: i64, reply: i64, st: i64) -> i64 {\n    chan_send(reply, st);\n    return st;\n}\n' > canon.ns
timeout 60 "$NUSA_BIN" run canon.ns >canon.got 2>canon.err \
  || { cat canon.err; fail "actor-canon" "import kanonis gagal"; }
printf '0\n' | cmp -s canon.got - \
  || { cat canon.got; fail "actor-canon" "output salah"; }
timeout 60 "$NUSA_BIN" fmt --check "$SRC_DIR/std/actor.ns" \
  >fmt.log 2>&1 || { cat fmt.log; fail "actor-fmt" "stdlib tak fmt-clean"; }
pass "actor-canon"

echo "E2E ACTOR PASS ($PASS_N cases)"
