#!/usr/bin/env bash
# Integration test v2.30.0 (Phase 79): distributed foundation.
#  - `nusa test` rpc/retry/timeout/discovery.
#  - RPC Nusa<->Nusa localhost (server binary bg + client golden).
#  - Negatif: port tutup.
set -u
NUSA_BIN="${1:?usage: e2e_dist.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
SRV_PID=""
cleanup() {
  if [ -n "$SRV_PID" ]; then kill "$SRV_PID" 2>/dev/null; fi
  rm -rf "$TMPDIR"
}
trap cleanup EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# 1. `nusa test` modul distributed.
for t in rpc_test dist_test; do
  timeout 120 "$NUSA_BIN" test "$ROOT/tests/$t.ns" >"$t.tlog" 2>&1 || { cat "$t.tlog"; fail "test-$t" "nusa test rc!=0"; }
  pass "test-$t"
done
rm -f counter.txt

# 2. RPC Nusa<->Nusa: server bg + client golden. Port dipilih
# dinamis (probe tutup) agar kebal stray/collision paralel.
PORT=0
for p in $(seq 18963 18972); do
  if (echo > /dev/tcp/127.0.0.1/$p) 2>/dev/null; then
    continue
  else
    PORT=$p
    break
  fi
done
[ "$PORT" -ne 0 ] || fail "port" "no free port in range"
echo "using port $PORT"
cat > rpcsrv.ns <<'EOF'
import rpc;
import json;
import string;
fn handle(req: string) -> string {
    let m = rpc_method(req);
    if str_eq(m, "add") {
        let p = rpc_params(req);
        let s = json_get_int(p, "a") + json_get_int(p, "b");
        return rpc_response(rpc_id(req), int_to_str(s));
    }
    return rpc_error(rpc_id(req), "unknown method");
}
fn main() {
    let port = parse_int(process_argv(1));
    print(rpc_serve_once(port, handle));
    print(rpc_serve_once(port, handle));
}
EOF
cat > rpccli.ns <<'EOF'
import rpc;
import json;
import string;
fn call_wait(host: string, port: i64, method: string, params: string) -> string {
    let r = "";
    let k = 0;
    while k < 50 {
        r = rpc_call(host, port, method, params);
        if str_eq(r, "") == false {
            k = 50;
        } else {
            sleep_ms(100);
            k = k + 1;
        }
    }
    return r;
}
fn main() {
    let port = parse_int(process_argv(1));
    print(call_wait("127.0.0.1", port, "add", "{\"a\":20,\"b\":22}"));
    print(call_wait("127.0.0.1", port, "nope", "{}"));
    print(rpc_call("127.0.0.1", 18999, "add", "{}"));
}
EOF
"$NUSA_BIN" build rpcsrv.ns -o rpcsrv > /dev/null 2>&1 || fail "rpc-build" "server build"
./rpcsrv "$PORT" > rpcsrv.got 2>&1 &
SRV_PID=$!
timeout 60 "$NUSA_BIN" run rpccli.ns "$PORT" >rpccli.got 2>rpccli.err || { cat rpccli.err; fail "rpc-cli" "run failed"; }
# Tunggu server selesai BOUNDED (jangan `wait` polos: klien yang
# gagal-diam + server-di-accept = gantung selamanya).
for _ in $(seq 1 100); do
  if kill -0 $SRV_PID 2>/dev/null; then sleep 0.2; else break; fi
done
if kill -0 $SRV_PID 2>/dev/null; then
  echo "--- srv state ---"
  head -n 3 /proc/$SRV_PID/status 2>/dev/null
  kill $SRV_PID 2>/dev/null
  SRV_PID=""
  echo "--- rpccli.got ---"
  cat rpccli.got
  echo "--- rpcsrv.got ---"
  cat rpcsrv.got
  fail "rpc-srv" "server still alive (client made too few connections?)"
fi
wait $SRV_PID 2>/dev/null || true
SRV_PID=""
printf '{"jsonrpc":"2.0","id":1,"result":42}\n{"jsonrpc":"2.0","id":1,"error":"unknown method"}\n\n' | diff - rpccli.got || { cat rpccli.got; fail "rpc-cli" "wrong output"; }
pass "rpc-call"
printf '0\n0\n' | diff - rpcsrv.got || { cat rpcsrv.got; fail "rpc-srv" "wrong output"; }
pass "rpc-serve"

echo "E2E DIST PASS ($PASS_N cases)"
