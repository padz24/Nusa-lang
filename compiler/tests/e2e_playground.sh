#!/usr/bin/env bash
# Integration test v1.20.0 (Phase 42): sandbox + playground HTTP.
# Verdict sandbox (ok/timeout/mem-limit/output-limit/build-fail) +
# API playground (page/run/timeout/bad-request/404) via curl.
set -u
NUSA_BIN="${1:?usage: e2e_playground.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }

# --- Sandbox: 5 verdict ---
printf 'fn main() {\n    print(40 + 2);\n}\n' > ok.ns
OUT="$("$NUSA_BIN" sandbox ok.ns --timeout 5 2>/dev/null)" || fail "sandbox-ok" "rc != 0"
[ "$OUT" = "$(printf '42\n\n[sandbox: verdict=ok code=0]')" ] || { echo "$OUT"; fail "sandbox-ok" "output"; }
echo "case sandbox-ok OK"

printf 'fn main() {\n    while true {\n    }\n}\n' > loop.ns
"$NUSA_BIN" sandbox loop.ns --timeout 2 > loop.got 2>&1 && fail "sandbox-timeout" "should fail"
grep -q "verdict=timeout" loop.got || fail "sandbox-timeout" "no timeout"
echo "case sandbox-timeout OK"

printf 'fn main() {\n    let s = "x";\n    while true {\n        s = str_concat(s, s);\n    }\n}\n' > hog.ns
"$NUSA_BIN" sandbox hog.ns --timeout 20 --memory-mb 32 > hog.got 2>&1 && fail "sandbox-mem" "should fail"
grep -q "verdict=mem-limit" hog.got || fail "sandbox-mem" "no mem-limit"
echo "case sandbox-mem OK"

printf 'fn main() {\n    while true {\n        print("spam");\n    }\n}\n' > spam.ns
"$NUSA_BIN" sandbox spam.ns --timeout 10 --output-kb 8 > spam.got 2>&1 && fail "sandbox-output" "should fail"
grep -q "verdict=output-limit" spam.got || fail "sandbox-output" "no output-limit"
echo "case sandbox-output OK"

printf 'fn main() {\n    print(nope);\n}\n' > bad.ns
"$NUSA_BIN" sandbox bad.ns --timeout 5 > bad.got 2>&1 && fail "sandbox-build" "should fail"
grep -q "verdict=build-fail" bad.got || fail "sandbox-build" "no build-fail"
echo "case sandbox-build OK"

# --- Playground HTTP (curl; server bertahan setelah timeout) ---
"$NUSA_BIN" playground --port 0 --timeout 2 > pg.log 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$TMPDIR"' EXIT
sleep 1
PORT="$(sed -E 's/.*127.0.0.1:([0-9]+).*/\1/' pg.log | head -n 1)"
[[ "$PORT" =~ ^[0-9]+$ ]] || { cat pg.log; fail "pg" "no port"; }

curl -s -m 10 "http://127.0.0.1:$PORT/" | grep -q "Nusa Playground" || fail "pg" "no page"
echo "case pg-page OK"

curl -s -m 60 -X POST "http://127.0.0.1:$PORT/api/run" \
  -H 'Content-Type: application/json' \
  --data '{"source":"fn main() {\n print(7 * 6);\n}\n"}' > run.got || fail "pg" "run curl"
grep -q '"ok":true' run.got && grep -q '42' run.got || { cat run.got; fail "pg" "run"; }
echo "case pg-run OK"

curl -s -m 60 -X POST "http://127.0.0.1:$PORT/api/run" \
  -H 'Content-Type: application/json' \
  --data '{"source":"fn main() { while true {} }\n"}' > tmo.got || fail "pg" "timeout curl"
grep -q '"verdict":"timeout"' tmo.got || { cat tmo.got; fail "pg" "timeout"; }
echo "case pg-timeout OK"

curl -s -m 10 -X POST "http://127.0.0.1:$PORT/api/run" \
  -H 'Content-Type: application/json' --data '{"nope":1}' | grep -q "bad-request" || fail "pg" "bad-request"
curl -s -m 10 "http://127.0.0.1:$PORT/nope" | grep -q "not found" || fail "pg" "404"
echo "case pg-errors OK"

kill $SRV 2>/dev/null
wait 2>/dev/null
trap 'rm -rf "$TMPDIR"' EXIT
echo "E2E PLAYGROUND PASS (9 cases)"
