#!/usr/bin/env bash
# Real-world validation v3.0.0-beta (Phase 85): 6 proyek nyata di
# realworld/ — build + golden. Network loopback port dinamis
# 19011-19020 (hindari suite paralel); server kuota-bounded.
set -u
NUSA_BIN="${1:?usage: e2e_realworld.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RW="$ROOT/realworld"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

pick_port() {
  for p in $(seq 19011 19020); do
    if (echo > /dev/tcp/127.0.0.1/$p) 2>/dev/null; then
      continue
    else
      echo "$p"
      return 0
    fi
  done
  return 1
}

http_get() { # $1=port $2=path -> body only
  python3 -c "
import socket,sys
s = socket.create_connection(('127.0.0.1', $1), timeout=15)
s.sendall(('GET $2 HTTP/1.0\r\nHost: x\r\n\r\n').encode())
d = b''
while True:
    ch = s.recv(1024)
    if not ch: break
    d += ch
s.close()
sys.stdout.write(d.decode().split('\r\n\r\n', 1)[1])
"
}

tcp_cmd() { # $1=port $2=line -> reply stripped
  python3 -c "
import socket
s = socket.create_connection(('127.0.0.1', $1), timeout=15)
s.sendall(('$2\n').encode())
d = b''
while True:
    ch = s.recv(64)
    if not ch: break
    d += ch
s.close()
print(d.decode().strip())
"
}

# 1. wordcount (CLI).
printf 'hello world\nfoo bar baz\n' > wc1.txt
printf '' > wc0.txt
cp "$RW/wordcount/wc.ns" ./wc.ns
timeout 60 "$NUSA_BIN" run ./wc.ns wc1.txt wc0.txt wc-nope.txt >wc.got 2>wc.err \
  || { cat wc.err; fail "rw-wc" "run failed"; }
printf '2 5 24 wc1.txt\n0 0 0 wc0.txt\n0 0 0 wc-nope.txt: no such file\n' \
  | cmp -s wc.got - || { cat wc.got; fail "rw-wc" "wrong output"; }
pass "rw-wc"

# 2. todohttp (HTTP server, 3 request lalu exit).
PORT="$(pick_port)" || fail "rw-http" "no free port"
"$NUSA_BIN" build "$RW/todohttp/main.ns" -o todosrv > /dev/null 2>&1 \
  || fail "rw-http" "build failed"
./todosrv "$PORT" 3 >srv.log 2>&1 &
SRV_PID=$!
[ "$(http_get "$PORT" /health)" = "ok" ] || { kill $SRV_PID 2>/dev/null; fail "rw-http" "health"; }
[ "$(http_get "$PORT" /todos)" = '[{"id":1,"task":"write spec"},{"id":2,"task":"ship beta"}]' ] \
  || { kill $SRV_PID 2>/dev/null; fail "rw-http" "todos"; }
[ "$(http_get "$PORT" /nope)" = "not found" ] || { kill $SRV_PID 2>/dev/null; fail "rw-http" "404"; }
wait $SRV_PID 2>/dev/null
grep -q "done" srv.log || fail "rw-http" "server tak exit-kuota"
pass "rw-http"

# 3. kvstore (network app, 6 request lalu exit).
PORT="$(pick_port)" || fail "rw-kv" "no free port"
"$NUSA_BIN" build "$RW/kvstore/main.ns" -o kvsrv > /dev/null 2>&1 \
  || fail "rw-kv" "build failed"
./kvsrv "$PORT" 6 >kv.log 2>&1 &
SRV_PID=$!
[ "$(tcp_cmd "$PORT" 'GET a')" = "nil" ] || { kill $SRV_PID 2>/dev/null; fail "rw-kv" "get-nil"; }
[ "$(tcp_cmd "$PORT" 'SET a 1')" = "OK" ] || { kill $SRV_PID 2>/dev/null; fail "rw-kv" "set"; }
[ "$(tcp_cmd "$PORT" 'GET a')" = "1" ] || { kill $SRV_PID 2>/dev/null; fail "rw-kv" "get"; }
[ "$(tcp_cmd "$PORT" 'SET a 2')" = "OK" ] || { kill $SRV_PID 2>/dev/null; fail "rw-kv" "overwrite"; }
[ "$(tcp_cmd "$PORT" 'GET a')" = "2" ] || { kill $SRV_PID 2>/dev/null; fail "rw-kv" "get2"; }
[ "$(tcp_cmd "$PORT" 'BOGUS')" = "ERR" ] || { kill $SRV_PID 2>/dev/null; fail "rw-kv" "err"; }
wait $SRV_PID 2>/dev/null
grep -q "done" kv.log || fail "rw-kv" "server tak exit-kuota"
pass "rw-kv"

# 4. nsvet (developer tool).
printf 'fn main() {\n\tprint(1); \n// TODO: fix\nprint(2);' > v1.ns
printf 'fn ok() {\n return 1;\n}\n' > v2.ns
cp "$RW/nsvet/main.ns" ./nsvet.ns
timeout 60 "$NUSA_BIN" run ./nsvet.ns v1.ns v2.ns v-nope.ns >vet.got 2>vet.err \
  || { cat vet.err; fail "rw-vet" "run failed"; }
printf 'v1.ns: 3 issue(s), 1 todo(s)\nv2.ns: 0 issue(s), 0 todo(s)\nv-nope.ns: no such file\ntotal: 3 issue(s), 1 todo(s)\n' \
  | cmp -s vet.got - || { cat vet.got; fail "rw-vet" "wrong output"; }
pass "rw-vet"

# 5. nstok (compiler tool).
printf '// hello\nfn main() {\n let x: i64 = 41 + 1;\n print("n=");\n}\n' > t1.ns
cp "$RW/nstok/main.ns" ./nstok.ns
timeout 60 "$NUSA_BIN" run ./nstok.ns t1.ns >tok.got 2>tok.err \
  || { cat tok.err; fail "rw-tok" "run failed"; }
printf 't1.ns: 3 kw, 3 id, 2 int, 1 str, 1 cmt, 11 sym\n' \
  | cmp -s tok.got - || { cat tok.got; fail "rw-tok" "wrong output"; }
pass "rw-tok"

# 6. csvsum (data processing).
printf 'name,score\nana,10\nbud,20\ncit,xx\n' > s.csv
cp "$RW/csvsum/main.ns" ./csvsum.ns
timeout 60 "$NUSA_BIN" run ./csvsum.ns s.csv score >csv.got 2>csv.err \
  || { cat csv.err; fail "rw-csv" "run failed"; }
printf 'rows=2 sum=30 min=10 max=20 avg_x1000=15000 skipped=1\n' \
  | cmp -s csv.got - || { cat csv.got; fail "rw-csv" "wrong output"; }
pass "rw-csv"

# 7. fmt bersih + build semua proyek.
"$NUSA_BIN" fmt --check "$RW"/wordcount/wc.ns "$RW"/todohttp/main.ns \
  "$RW"/kvstore/main.ns "$RW"/nsvet/main.ns "$RW"/nstok/main.ns \
  "$RW"/csvsum/main.ns >fmt.log 2>&1 \
  || { cat fmt.log; fail "rw-fmt" "realworld not fmt-clean"; }
i=0
for f in "$RW"/wordcount/wc.ns "$RW"/todohttp/main.ns "$RW"/kvstore/main.ns \
         "$RW"/nsvet/main.ns "$RW"/nstok/main.ns "$RW"/csvsum/main.ns; do
  i=$((i + 1))
  timeout 180 "$NUSA_BIN" build "$f" -o "rw$i" >"rw$i.log" 2>&1 \
    || { cat "rw$i.log"; fail "rw-build" "build failed: $f"; }
done
pass "rw-fmt-build"

find "$ROOT/realworld" -name "*.ns.ll" -delete
find "$ROOT/realworld" -name "*.run.out" -delete
echo "E2E REALWORLD PASS ($PASS_N cases)"
