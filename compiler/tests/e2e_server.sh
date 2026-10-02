#!/usr/bin/env bash
# Integration test v2.29.0 (Phase 78): server foundation.
#  - config/metrics (`nusa test` + golden).
#  - async smoke (tasks) + serve_once smoke.
#  - TLS via paket ekosistem (guard openssl/py-ssl/libssl):
#    CA+server-cert lokal, verifikasi penuh + negatif.
set -u
NUSA_BIN="${1:?usage: e2e_server.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
SRV_PID=""
TLS_PID=""
cleanup() {
  if [ -n "$SRV_PID" ]; then kill "$SRV_PID" 2>/dev/null; fi
  if [ -n "$TLS_PID" ]; then kill "$TLS_PID" 2>/dev/null; fi
  rm -rf "$TMPDIR"
}
trap cleanup EXIT
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

# 1. `nusa test` config + metrics.
for t in config_test metrics_test; do
  timeout 60 "$NUSA_BIN" test "$ROOT/tests/$t.ns" >"$t.tlog" 2>&1 || { cat "$t.tlog"; fail "test-$t" "nusa test rc!=0"; }
  pass "test-$t"
done

# 2. Golden config (env > TOML > fallback).
cat > s_cfg.ns <<'EOF'
import config;
fn main() {
    let d = "[s]\na = \"f\"\nn = 3\nb = true\n";
    print(config_str(d, "s", "a", "NUSA_SRV_NOPE_A", "fb"));
    print(config_int(d, "s", "n", "NUSA_SRV_NOPE_B", 1));
    print(config_bool(d, "s", "b", "NUSA_SRV_NOPE_C", false));
    print(config_str(d, "s", "zz", "NUSA_SRV_NOPE_D", "fb"));
}
EOF
printf 'f\n3\ntrue\nfb\n' > s_cfg.want
run_ok s_cfg
cat > s_cfg_env.ns <<'EOF'
import config;
fn main() {
    let d = "[s]\na = \"f\"\nn = 3\nb = true\n";
    print(config_str(d, "s", "a", "NUSA_SRV_ENV_A", "fb"));
    print(config_int(d, "s", "n", "NUSA_SRV_ENV_B", 1));
    print(config_bool(d, "s", "b", "NUSA_SRV_ENV_C", true));
}
EOF
printf 'envwin\n11\nfalse\n' > s_cfg_env.want
NUSA_SRV_ENV_A=envwin NUSA_SRV_ENV_B=11 NUSA_SRV_ENV_C=false timeout 60 "$NUSA_BIN" run s_cfg_env.ns >s_cfg_env.got 2>s_cfg_env.err || { cat s_cfg_env.err; fail "s_cfg_env" "run failed"; }
cmp -s s_cfg_env.got s_cfg_env.want || fail "s_cfg_env" "wrong output"
pass "s_cfg_env"

# 3. Golden metrics (render Prometheus; uptime dinormalisasi).
cat > s_met.ns <<'EOF'
import metrics;
import string;
fn main() {
    let m = metrics_new();
    m = m_req(m);
    m = m_req(m);
    m = m_err(m);
    m = m_bytes(m, 40);
    m = m_bytes(m, 2);
    let r = render_prometheus(m, "app");
    let cut = str_index_of(r, "app_uptime_ms", 0);
    print(str_slice(r, 0, cut));
    print(m_elapsed_ms(m) >= 0);
}
EOF
printf '# TYPE app_requests counter\napp_requests 2\n# TYPE app_errors counter\napp_errors 1\n# TYPE app_bytes counter\napp_bytes 42\n# TYPE \ntrue\n' > s_met.want
run_ok s_met

# 4. Async smoke (tasks deterministik).
cat > s_task.ns <<'EOF'
import tasks;
import thread;
fn one() -> i64 {
    return 1;
}
fn add1(x: i64) -> i64 {
    return x + 1;
}
fn dbl(x: i64) -> i64 {
    return x * 2;
}
fn main() {
    let f = task_spawn(one);
    print(future_await(f));
    print(par_map2(add1, dbl, 10, 20));
}
EOF
printf '1\n51\n' > s_task.want
run_ok s_task

# 5. Serve_once smoke (server Nusa + client python).
if command -v python3 >/dev/null 2>&1; then
  cat > s_srv.ns <<'EOF'
import http;
import string;
fn main() {
    print(http_serve_once(18959, "hi-srv"));
}
EOF
  cat > srvcli.py <<'EOF'
import socket, time
for _ in range(100):
    try:
        s = socket.create_connection(("127.0.0.1", 18959), timeout=5)
        break
    except OSError:
        time.sleep(0.1)
else:
    raise SystemExit("connect failed")
s.sendall(b"GET / HTTP/1.0\r\n\r\n")
time.sleep(1)
s.close()
EOF
  "$NUSA_BIN" build s_srv.ns -o s_srv > /dev/null 2>&1 || fail "s_srv" "build"
  ./s_srv > s_srv.got 2>&1 &
  SRV_PID=$!
  python3 srvcli.py || fail "s_srv" "client"
  wait $SRV_PID || fail "s_srv" "server rc"
  SRV_PID=""
  printf '0\n' | diff - s_srv.got || fail "s_srv" "wrong output"
  pass "s_srv"
else
  echo "SKIP-s_srv: no python3"
fi

# 6. TLS via paket (guard: openssl CLI + py-ssl + link).
if ! command -v openssl >/dev/null 2>&1; then
  echo "SKIP-tls: no openssl CLI"
elif ! python3 -c "import ssl" >/dev/null 2>&1; then
  echo "SKIP-tls: no python ssl"
elif ! echo 'int main(){return 0;}' | cc -x c - -lssl -lcrypto -o /tmp/nusa_tls_probe 2>/dev/null; then
  echo "SKIP-tls: cannot link libssl"
  rm -f /tmp/nusa_tls_probe
else
  rm -f /tmp/nusa_tls_probe
  openssl req -x509 -newkey rsa:2048 -keyout ca.key -out ca.crt -days 1 -nodes -subj "/CN=TestCA" 2>/dev/null || fail "tls-cert" "CA failed"
  openssl req -newkey rsa:2048 -keyout srv.key -out srv.csr -nodes -subj "/CN=localhost" 2>/dev/null || fail "tls-cert" "CSR failed"
  printf 'subjectAltName=DNS:localhost,IP:127.0.0.1\n' > san.ext
  openssl x509 -req -in srv.csr -CA ca.crt -CAkey ca.key -CAcreateserial -days 1 -extfile san.ext -out srv.crt 2>/dev/null || fail "tls-cert" "sign failed"
  openssl req -x509 -newkey rsa:2048 -keyout ca2.key -out ca2.crt -days 1 -nodes -subj "/CN=EvilCA" 2>/dev/null || fail "tls-cert" "CA2 failed"
  cp -r "$ROOT/packages/tls" tls-pkg
  cc -c tls-pkg/shim.c -o tls-pkg/shim.o || fail "tls-cc" "shim build failed"
  cat > tlsserv.py <<'EOF'
import socket, ssl
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain("srv.crt", "srv.key")
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", 18973))
srv.listen(5)
srv.settimeout(120)
print("ready", flush=True)
for _ in range(4):
    try:
        raw, _ = srv.accept()
    except Exception:
        break
    try:
        c = ctx.wrap_socket(raw, server_side=True)
        c.recv(4096)
        c.sendall(b"HTTP/1.0 200 OK\r\nContent-Length: 5\r\n\r\nhello")
        c.shutdown(socket.SHUT_RDWR)
        c.close()
    except Exception:
        try: raw.close()
        except Exception: pass
EOF
  "$NUSA_BIN" pkg init tlsapp --dir app >pkg.log 2>&1 || fail "tls-init" "pkg init failed"
  cp tls-pkg/shim.o app/shim.o
  cd app || exit 1
  "$NUSA_BIN" pkg add tls --path ../tls-pkg >pkg.log 2>&1 || { cd ..; cat app/pkg.log; fail "tls-add" "pkg add failed"; }
  "$NUSA_BIN" pkg update >pkg.log 2>&1 || { cd ..; fail "tls-update" "pkg update failed"; }
  cat >> nusa.toml <<'EOF'

[link]
files = ["shim.o"]
libraries = ["ssl", "crypto"]

[capabilities]
native = true
EOF
  cat > main.ns <<'EOF'
import tls;
import http;
import string;
fn main() {
    print(tls_request("localhost", 18973, "../ca.crt", "GET / HTTP/1.0\r\n\r\n", "resp.txt"));
    print(http_body(file_read("resp.txt")));
    print(tls_request("localhost", 18973, "../ca.crt", "GET / HTTP/1.0\r\n\r\n", "r2.txt"));
    let id = tls_dial("localhost", 18973, "../ca.crt");
    print(id);
    print(tls_close(id));
    print(tls_dial("localhost", 18999, "../ca.crt"));
    print(tls_request("localhost", 18973, "../ca2.crt", "GET / HTTP/1.0\r\n\r\n", "r3.txt"));
}
EOF
  "$NUSA_BIN" pkg build >pkg.log 2>&1 || { cd ..; cat app/pkg.log; fail "tls-build" "pkg build failed"; }
  [ -x target/tlsapp ] || { cd ..; fail "tls-build" "target missing"; }
  cd ..
  nohup python3 tlsserv.py > tlsserv.log 2>&1 &
  TLS_PID=$!
  for _ in $(seq 1 50); do
    if grep -q "ready" tlsserv.log 2>/dev/null; then break; fi
    sleep 0.2
  done
  grep -q "ready" tlsserv.log 2>/dev/null || fail "tls-serv" "server not ready"
  cd app || exit 1
  timeout 120 ./target/tlsapp > app.got 2>app.err || { cd ..; cat app/app.err; fail "tls-run" "run failed"; }
  cd ..
  kill $TLS_PID 2>/dev/null
  TLS_PID=""
  printf '43\nhello\n43\n0\n0\n-1\n-1\n' | diff - app/app.got || { cat app/app.got; fail "tls-run" "wrong output"; }
  pass "tls-pkg"
fi

echo "E2E SERVER PASS ($PASS_N cases)"
