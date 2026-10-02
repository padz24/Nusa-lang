#!/usr/bin/env bash
# Integration test v1.10.0 (Phase 29): stdlib 2.0 end-to-end.
#  - Builtin baru: string/time/random/process/args.
#  - Modul std baru via `import std.x`: time/random/process/path/thread/sync.
#  - Net: TCP localhost (server python latar, client Nusa).
set -u
NUSA_BIN="${1:?usage: e2e_stdlib2.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

run_ok() {
  local name="$1"; shift
  timeout 60 "$NUSA_BIN" run "$name.ns" "$@" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
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

# 1. String baru: char_at + slice (batas dijepit).
cat > v1.ns <<'EOF'
fn main() {
    print(str_char_at("hello", 1));
    print(str_char_at("hi", 9));
    print(str_char_at("hi", 0 - 1));
    print(str_slice("hello", 1, 4));
    print(str_slice("hello", 3, 99));
    print(str_slice("hello", 2, 2));
}
EOF
printf 'e\n\n\nell\nlo\n\n' > v1.want
run_ok v1

# 2. Time + random (sifat, bukan nilai pasti).
cat > v2.ns <<'EOF'
import std.time;
import std.random;
fn main() {
    print(time_now_ms() > 0);
    print(time_now_s() > 0);
    print(elapsed_ms(now_ms()) < 60000);
    let s = rand_seed();
    print(rand_below(s, 100) < 100);
    print(rand_below(s, 100) < 0);
    print(rand_range(s, 5, 10));
    print(rand_bool(s));
}
EOF
# (nilai acak: hanya cek bentuk — bandingkan pola, bukan isi pasti)
timeout 60 "$NUSA_BIN" run v2.ns > v2.got 2>v2.err || { cat v2.err; fail "v2" "run failed"; }
[ "$(wc -l < v2.got)" = "7" ] || { cat v2.got; fail "v2" "want 7 lines"; }
head -n 3 v2.got | grep -q "true" || { cat v2.got; fail "v2" "time lines"; }
sed -n '4p;5p' v2.got | grep -q "true" || { cat v2.got; fail "v2" "rand range"; }
sed -n '6p' v2.got | grep -qE "^[5-9]$" || { cat v2.got; fail "v2" "rand_range outside [5,10)"; }
grep -q "false" v2.got || { cat v2.got; fail "v2" "want a false (below<0)"; }
pass "v2"

# 3. Process: argv diteruskan + env + cwd.
cat > v3.ns <<'EOF'
import std.process;
fn main() {
    print(args_count());
    print(arg(1));
    print(arg(99));
    print(str_eq(prog_name(), ""));
    print(getenv_or("NUSA_DEFINITELY_UNSET_VAR", "fb"));
}
EOF
printf '3\na\n\nfalse\nfb\n' > v3.want
run_ok v3 a b

# 4. Path murni-Nusa.
cat > v4.ns <<'EOF'
import std.path;
fn main() {
    print(path_basename("a/b/c"));
    print(path_basename("/a/"));
    print(path_basename("/"));
    print(path_dirname("a/b/c"));
    print(path_dirname("c"));
    print(path_dirname("/c"));
    print(path_ext("a.tar.gz"));
    print(path_ext("a"));
    print(path_strip_ext("a.tar.gz"));
    print(path_join("a", "b"));
    print(path_join("a/", "b"));
    print(path_join("a", "/b"));
    print(path_normalize("a/./b/../c"));
    print(path_normalize("/x/../y"));
    print(path_is_absolute("/a"));
    print(path_is_absolute("a"));
}
EOF
printf 'c\na\n\na/b\n.\n/\ngz\n\na.tar\na/b\na/b\n/b\na/c\n/y\ntrue\nfalse\n' > v4.want
run_ok v4

# 5. Thread/sync wrappers + par_map2.
cat > v5.ns <<'EOF'
import std.thread;
import std.sync;
fn main() {
    print(par_map2(|x: i64| x + 1, |x: i64| x * 2, 10, 20));
    let m = lock_new();
    print(with_lock(m, || 42));
    lock_free(m);
    let a = atomic_new(0);
    print(atomic_inc(a));
    print(atomic_dec(a));
    atomic_free(a);
}
EOF
printf '51\n42\n1\n0\n' > v5.want
run_ok v5

# 6. Net TCP localhost (server python, client Nusa).
cat > netserv.py <<'EOF'
import socket, threading
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 18931))
srv.listen(1)
srv.settimeout(25)
def run():
    try:
        conn, _ = srv.accept()
        conn.settimeout(10)
        data = conn.recv(100)
        conn.sendall(b"hi:" + data)
        conn.close()
    finally:
        srv.close()
t = threading.Thread(target=run, daemon=True)
t.start()
t.join(timeout=20)
EOF
cat > v6.ns <<'EOF'
import std.net;
fn main() {
    let fd = dial("127.0.0.1", 18931);
    print(fd >= 0);
    print(send_str(fd, "yo"));
    print(recv_some(fd, 64));
    print(recv_line(fd));
    close_conn(fd);
    print(dial("127.0.0.1", 18932) < 0);
}
EOF
printf 'true\ntrue\nhi:yo\n\ntrue\n' > v6.want
nohup python3 netserv.py > netserv.log 2>&1 &
SRV_PID=$!
sleep 1
run_ok v6
wait $SRV_PID 2>/dev/null || true

# 6b. Net TCP localhost, arah sebaliknya (server Nusa, client python).
cat > v6b.ns <<'EOF'
import std.net;
fn main() {
    let srv = listen_on(18943);
    print(srv >= 0);
    let c = accept_one(srv);
    print(c >= 0);
    print(recv_some(c, 64));
    close_conn(c);
    close_conn(srv);
}
EOF
printf 'true\ntrue\nyo\n' > v6b.want
cat > netcli.py <<'EOF'
import socket, time
for _ in range(100):
    try:
        s = socket.create_connection(("127.0.0.1", 18943), timeout=5)
        break
    except OSError:
        time.sleep(0.1)
else:
    raise SystemExit("connect failed")
s.sendall(b"yo")
s.close()
EOF
"$NUSA_BIN" build v6b.ns -o v6b_srv > /dev/null 2>&1 || fail "v6b" "build"
./v6b_srv > v6b.got 2>&1 &
NSRV_PID=$!
python3 netcli.py || { kill $NSRV_PID 2>/dev/null; fail "v6b" "client"; }
wait $NSRV_PID || fail "v6b" "server rc"
cmp -s v6b.got v6b.want || { cat v6b.got; fail "v6b" "wrong output"; }
pass "v6b"

# 7. Penolakan: tipe builtin baru.
cat > e1.ns <<'EOF'
fn main() {
    print(str_char_at(1, 0));
}
EOF
expect_fail e1 "a 'string' argument"

cat > e2.ns <<'EOF'
fn f(x: i64) -> i64 {
    return x;
}
fn main() {
    let h = thread_spawn(f);
}
EOF
expect_fail e2 "requires a 'fn() -> i64' argument"

cat > e3.ns <<'EOF'
fn main() {
    print(tcp_send(1, 2));
}
EOF
expect_fail e3 "a 'string' data argument"

echo "E2E STDLIB2 PASS ($PASS_N cases)"
