#!/usr/bin/env bash
# Networking conformance v2.9.0 (Phase 58, milestone 1).
# HTTP primitives murni Nusa (`std/http.ns`): parse unit +
# round-trip hermetik (server Nusa di thread + client Nusa,
# loopback, dial-retry). Deterministik via join.
set -u
NUSA_BIN="${1:?usage: e2e_http.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

run_ok() {
  local name="$1"
  timeout 90 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run gagal"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "output salah"; }
  pass "$name"
}

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== parse unit (tanpa jaringan) =="
cat > h_parse.ns <<'EOF'
import std.http;
import string;
fn main() {
    print(http_status("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi"));
    print(http_status("HTTP/1.0 404 Not Found\n\nnope"));
    print(http_status("sampah tanpa spasi"));
    print(http_body("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi"));
    print(http_body("no-headers"));
}
EOF
printf '200\n404\n-1\nhi\n\n' > h_parse.want
run_ok h_parse

echo "== round-trip hermetik =="
cat > h_round.ns <<'EOF'
import std.http;
import string;
fn srv() -> i64 {
    return http_serve_once(18080, "hi-nusa");
}
fn main() {
    let h = thread_spawn(srv);
    let resp = http_get("127.0.0.1", 18080, "/");
    thread_join(h);
    print(http_status(resp));
    print(http_body(resp));
}
EOF
printf '200\nhi-nusa\n' > h_round.want
run_ok h_round

cat > h_round2.ns <<'EOF'
import std.http;
import string;
fn srv() -> i64 {
    return http_serve_once(18081, "nusa-ok");
}
fn dial_loop(port: i64) -> i64 {
    let i = 0;
    while i < 200 {
        let fd = tcp_connect("127.0.0.1", port);
        if fd != 0 - 1 {
            return fd;
        }
        sleep_ms(5);
        i = i + 1;
    }
    return 0 - 1;
}
fn read_loop(fd: i64) -> string {
    let out = "";
    let i = 0;
    while i < 64 {
        let chunk = tcp_recv(fd, 512);
        if str_eq(chunk, "") {
            return out;
        }
        out = str_concat(out, chunk);
        i = i + 1;
    }
    return out;
}
fn main() {
    let h = thread_spawn(srv);
    let fd = dial_loop(18081);
    print(fd != 0 - 1);
    tcp_send(fd, "GET /x HTTP/1.0\r\nHost: y\r\n\r\n");
    let resp = read_loop(fd);
    tcp_close(fd);
    thread_join(h);
    print(http_status(resp));
    print(http_body(resp));
}
EOF
printf 'true\n200\nnusa-ok\n' > h_round2.want
run_ok h_round2

echo "== gagal dial + tooling =="
cat > h_nodial.ns <<'EOF'
import std.http;
import string;
fn main() {
    print(str_eq(http_get("127.0.0.1", 18082, "/"), ""));
}
EOF
printf 'true\n' > h_nodial.want
run_ok h_nodial

"$NUSA_BIN" fmt --check "$SRC_DIR/std/http.ns" >/dev/null 2>&1 || fail "h_fmt" "std/http.ns tak bersih"
pass "h_fmt"
"$NUSA_BIN" doc "$SRC_DIR/std/http.ns" 2>/dev/null | grep -q "http_serve_once" || fail "h_doc" "doc hilang"
pass "h_doc"

echo "E2E HTTP PASS ($PASS_N cases)"
