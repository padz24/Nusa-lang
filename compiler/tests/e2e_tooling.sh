#!/usr/bin/env bash
# Integration test v0.9.0: tooling — check/test/fmt/doc.
set -u
NUSA_BIN="${1:?usage: e2e_tooling.sh <nusa-binary>}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. check: OK + gagal
printf 'fn main() { print(1); }\n' > ok.ns
"$NUSA_BIN" check ok.ns > check.log 2>&1 || { echo "E2E FAIL [check-ok]"; cat check.log; exit 1; }
grep -q "no errors" check.log || { echo "E2E FAIL [check-ok]:"; cat check.log; exit 1; }
printf 'fn main() { print(x); }\n' > bad.ns
if "$NUSA_BIN" check bad.ns > /dev/null 2>&1; then echo "E2E FAIL [check-bad]"; exit 1; fi
echo "case check OK"

# 2. test: campuran pass/fail/timeout/build-error
mkdir -p t
printf 'fn main() { assert(1 + 1 == 2); }\n' > t/a_test.ns
printf 'fn main() { assert(1 == 2); }\n' > t/b_test.ns
printf 'fn main() { print("bukan test"); }\n' > t/not_a_test.txt
printf 'fn main() { while true { } }\n' > t/c_test.ns
printf 'fn main() { print(x); }\n' > t/d_test.ns
OUT="$("$NUSA_BIN" test t 2>&1)"; RC=$?
echo "$OUT" | grep -q "PASS t/a_test.ns" || { echo "E2E FAIL [test-pass]:"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q "FAIL t/b_test.ns (exit 1)" || { echo "E2E FAIL [test-fail]:"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q "FAIL t/c_test.ns (timeout" || { echo "E2E FAIL [test-timeout]:"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q "FAIL t/d_test.ns (build failed)" || { echo "E2E FAIL [test-build]:"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q "1 passed, 3 failed" || { echo "E2E FAIL [test-summary]:"; echo "$OUT"; exit 1; }
[ $RC -ne 0 ] || { echo "E2E FAIL [test-rc]"; exit 1; }
# kosong: tidak ada test
mkdir -p empty && OUT2="$("$NUSA_BIN" test empty/ 2>&1)"; RC2=$?
[ $RC2 -ne 0 ] || { echo "E2E FAIL [test-empty]"; exit 1; }
echo "$OUT2" | grep -q "no test files" || { echo "E2E FAIL [test-empty-msg]:"; echo "$OUT2"; exit 1; }
echo "case test OK"

# 2b. build -O: flag optimasi fungsional (bukan asersi performa).
printf 'fn main() {\n    print(6 * 7);\n}\n' > opt.ns
"$NUSA_BIN" build opt.ns -o opt2 -O2 > opt.log 2>&1 || { echo "E2E FAIL [opt-build]"; cat opt.log; exit 1; }
[ "$(./opt2)" = "42" ] || { echo "E2E FAIL [opt-run]"; exit 1; }
if "$NUSA_BIN" build opt.ns -o optx -O9 > /dev/null 2>&1; then echo "E2E FAIL [opt-bad]"; exit 1; fi
echo "case opt OK"

# 3. fmt: format + idempoten + --check
printf 'fn   main(){print(   "hi" );}\n' > messy.ns
if "$NUSA_BIN" fmt --check messy.ns > /dev/null 2>&1; then echo "E2E FAIL [fmt-check-dirty]"; exit 1; fi
"$NUSA_BIN" fmt messy.ns > fmt.log 2>&1 || { echo "E2E FAIL [fmt]"; cat fmt.log; exit 1; }
grep -q "formatted messy.ns" fmt.log || { echo "E2E FAIL [fmt-msg]:"; cat fmt.log; exit 1; }
"$NUSA_BIN" fmt --check messy.ns > /dev/null 2>&1 || { echo "E2E FAIL [fmt-clean]"; exit 1; }
printf 'fn main() {\n    print("hi");\n}\n' | diff - messy.ns || { echo "E2E FAIL [fmt-content]:"; cat messy.ns; exit 1; }
echo "case fmt OK"

# 4. doc: stdout + -o + error
printf '/// Adds.\nfn add(a: i64, b: i64) -> i64 {\n    return a + b;\n}\n' > api.ns
"$NUSA_BIN" doc api.ns > api.md 2>api.log || { echo "E2E FAIL [doc]"; cat api.log; exit 1; }
grep -q '### `add(a: i64, b: i64) -> i64`' api.md || { echo "E2E FAIL [doc-sig]:"; cat api.md; exit 1; }
grep -q "Adds." api.md || { echo "E2E FAIL [doc-text]:"; cat api.md; exit 1; }
"$NUSA_BIN" doc api.ns -o api2.md || { echo "E2E FAIL [doc-o]"; exit 1; }
diff api.md api2.md > /dev/null || { echo "E2E FAIL [doc-o-diff]"; exit 1; }
printf 'fn main() { @ }\n' > docbad.ns
if "$NUSA_BIN" doc docbad.ns > /dev/null 2>&1; then echo "E2E FAIL [doc-bad]"; exit 1; fi
echo "case doc OK"

# 5. doc --format json: struktur + -o + format-asing + error
"$NUSA_BIN" doc api.ns --format json > api.json 2>apij.log || { echo "E2E FAIL [doc-json]"; cat apij.log; exit 1; }
grep -q '"name":"add"' api.json || { echo "E2E FAIL [doc-json-fn]:"; cat api.json; exit 1; }
grep -q '{"name":"a","type":"i64"}' api.json || { echo "E2E FAIL [doc-json-param]:"; cat api.json; exit 1; }
grep -q '"ret":"i64"' api.json || { echo "E2E FAIL [doc-json-ret]:"; cat api.json; exit 1; }
"$NUSA_BIN" doc api.ns --format json -o api2.json || { echo "E2E FAIL [doc-json-o]"; exit 1; }
diff api.json api2.json > /dev/null || { echo "E2E FAIL [doc-json-o-diff]"; exit 1; }
if "$NUSA_BIN" doc api.ns --format yaml > /dev/null 2>&1; then echo "E2E FAIL [doc-json-badfmt]"; exit 1; fi
if "$NUSA_BIN" doc docbad.ns --format json > /dev/null 2>&1; then echo "E2E FAIL [doc-json-bad]"; exit 1; fi
echo "case doc-json OK"

echo "E2E TOOLING PASS"
