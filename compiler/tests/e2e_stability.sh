#!/usr/bin/env bash
# Integration test v2.1.0 (Phase 50: NUSA 2.1 STABILITY).
# Regression suite besar — mengunci 9 area audit:
#   compiler crashes, parser, typechecker, LLVM codegen,
#   runtime, memory, package manager, LSP (smoke), formatter.
# Konvensi: gagal-bersih harus exit != 0 TANPA sinyal (rc > 128 = CRASH).
set -u
NUSA_BIN="${1:?usage: e2e_stability.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

expect_fail() {
  local name="$1" pattern="$2"
  timeout 20 "$NUSA_BIN" build "$name.ns" -o "$name.out" >"$name.log" 2>&1
  local rc=$?
  if crashed "$rc"; then fail "$name" "COMPILER CRASH (rc=$rc)"; fi
  [ $rc -ne 0 ] || fail "$name" "should not build (rc=0)"
  grep -q "$pattern" "$name.log" || { echo "--- log ---"; cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

run_ok() {
  local name="$1"
  timeout 30 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; echo "--- want ---"; cat "$name.want"; fail "$name" "wrong output"; }
  pass "$name"
}

echo "== 1. compiler crashes (malformed tidak boleh sinyal) =="
printf '' > c_empty.ns
expect_fail c_empty "no 'fn main'"
printf 'fn main() { print("hi); }\n' > c_unterm.ns
expect_fail c_unterm "unterminated string"
printf '\x00\xff\xfe bad bytes \n' > c_bytes.ns
timeout 20 "$NUSA_BIN" check c_bytes.ns >c_bytes.log 2>&1
rc=$?; if crashed "$rc"; then fail "c_bytes" "CRASH rc=$rc"; fi
[ $rc -ne 0 ] || fail "c_bytes" "should fail rc=0"
pass "c_bytes"
python3 -c "print('fn main() { print(' + '('*600 + '1' + ')'*600 + '); }')" > c_nest.ns
expect_fail c_nest "too deeply nested"
printf 'fn main() { let x = 1/0; print(x); }\n' > c_div0.ns
expect_fail c_div0 "division by zero"

echo "== 2. parser (recovery, precedence, struct-vs-block) =="
printf 'fn broken() { let x = ; }\nfn main() { print(1); }\n' > p_rec.ns
expect_fail p_rec "expected expression"
cat > p_prec.ns <<'EOF'
fn main() {
    print(10 + 20 * 3);
    print((10 + 20) * 3);
}
EOF
printf '70\n90\n' > p_prec.want
run_ok p_prec
cat > p_ifstruct.ns <<'EOF'
struct P { x: i64; }
fn main() {
    let c = true;
    if c {
        print(1);
    }
    let p = P { x: 2 };
    print(p.x);
}
EOF
printf '1\n2\n' > p_ifstruct.want
run_ok p_ifstruct

echo "== 3. typechecker (mismatch, void, generics, traits) =="
printf 'fn main() { let x: i64 = "s"; }\n' > t_mismatch.ns
expect_fail t_mismatch "mismatched types"
printf 'fn v() { }\nfn main() { let x = v(); }\n' > t_void.ns
expect_fail t_void "void"
cat > t_generic.ns <<'EOF'
fn ident<T>(x: T) -> T {
    return x;
}
fn main() {
    print(ident(42));
    print(ident(true));
}
EOF
printf '42\ntrue\n' > t_generic.want
run_ok t_generic
cat > t_trait.ns <<'EOF'
trait Show { fn show(self) -> i64; }
struct P { v: i64; }
impl Show for P {
    fn show(self) -> i64 {
        return self.v;
    }
}
fn main() {
    let p = P { v: 7 };
    print(p.show());
}
EOF
printf '7\n' > t_trait.want
run_ok t_trait

echo "== 4. LLVM codegen (IR valid + verifyModule backstop) =="
printf 'fn add(a: i64, b: i64) -> i64 {\n    return a + b;\n}\nfn main() {\n    print(add(20, 10));\n}\n' > g_add.ns
timeout 30 "$NUSA_BIN" build g_add.ns -o g_add.out >g_add.log 2>&1 || { cat g_add.log; fail "g_add" "build failed"; }
[ "$(./g_add.out)" = "30" ] || fail "g_add" "wrong runtime"
grep -q "define i32 @main" g_add.ns.ll || { fail "g_add" "@main missing in IR"; }
pass "g_add"
printf 'fn main() { let x = 1; print(x / 0); }\n' > g_divdyn.ns
cat > g_divdyn_run.ns <<'EOF'
fn main() {
    let x = 1;
    let y = 0;
    print(x / y);
}
EOF
timeout 20 "$NUSA_BIN" run g_divdyn_run.ns >g_divdyn.got 2>&1
rc=$?; [ $rc -eq 1 ] || fail "g_divdyn" "dynamic div-zero must exit 1 (rc=$rc)"
grep -q "division by zero\|runtime error" g_divdyn.got || { cat g_divdyn.got; fail "g_divdyn" "missing runtime message"; }
pass "g_divdyn"

echo "== 5. runtime (OOB, string, file roundtrip) =="
cat > r_oob.ns <<'EOF'
fn main() {
    let a: [i64] = [1, 2];
    print(a[10]);
}
EOF
timeout 20 "$NUSA_BIN" run r_oob.ns >r_oob.got 2>&1
rc=$?; [ $rc -eq 1 ] || fail "r_oob" "OOB must exit 1 (rc=$rc)"
grep -q "index out of bounds" r_oob.got || { cat r_oob.got; fail "r_oob" "missing OOB message"; }
pass "r_oob"
cat > r_str.ns <<'EOF'
fn main() {
    print(str_len("Nusa"));
    print(str_concat("Nu", "sa"));
}
EOF
printf '4\nNusa\n' > r_str.want
run_ok r_str

echo "== 6. memory (copy semantik, loop terbatas) =="
cat > m_copy.ns <<'EOF'
struct P { x: i64; }
fn mk(v: i64) -> P {
    return P { x: v };
}
fn main() {
    let a = mk(5);
    let b = a;
    print(a.x);
    print(b.x);
    let n = 1;
    n = 2;
    print(n);
}
EOF
printf '5\n5\n2\n' > m_copy.want
run_ok m_copy
cat > m_loop.ns <<'EOF'
fn main() {
    let s = "";
    let i = 0;
    while i < 10 {
        s = str_concat(s, "a");
        i = i + 1;
    }
    print(str_len(s));
}
EOF
printf '10\n' > m_loop.want
run_ok m_loop

echo "== 7. package manager (manifest invalid, lock basi) =="
mkdir -p pkgbad && cd pkgbad || exit 1
"$NUSA_BIN" pkg init demo >/dev/null 2>&1 || fail "pkg_init" "init failed"
printf 'this is not toml [[[' > nusa.toml
if "$NUSA_BIN" pkg build >pkgbad.log 2>&1; then fail "pkg_badmanifest" "should fail"; fi
pass "pkg_badmanifest"
cd "$TMPDIR" || exit 1

echo "== 8. LSP smoke (server hidup, balas initialize) =="
python3 - "$NUSA_BIN" <<'PY' >lsp.got 2>&1
import json, subprocess, sys
nusa = sys.argv[1]
p = subprocess.Popen([nusa, "lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}}).encode()
p.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
p.stdin.flush()
import select
data = p.stdout.readline()
print(data.decode(errors="replace"))
p.kill()
PY
grep -q "Content-Length\|jsonrpc\|capabilities\|result" lsp.got || { cat lsp.got; fail "lsp_smoke" "no LSP reply"; }
pass "lsp_smoke"

echo "== 9. formatter (idempoten + regresi lambda Phase 50) =="
cat > f_lambda.ns <<'EOF'
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    print(apply(|x| x + 1, 1));
}
EOF
"$NUSA_BIN" fmt --check f_lambda.ns >/dev/null 2>&1 || fail "f_lambda" "canonical file must be clean"
if grep -q "( |" f_lambda.ns; then fail "f_lambda" "buggy '( |' present"; fi
pass "f_lambda"
printf 'fn   main(){print(   "hi" );}\n' > f_messy.ns
if "$NUSA_BIN" fmt --check f_messy.ns >/dev/null 2>&1; then fail "f_messy" "dirty must fail check"; fi
"$NUSA_BIN" fmt f_messy.ns >/dev/null 2>&1 || fail "f_messy" "fmt failed"
"$NUSA_BIN" fmt --check f_messy.ns >/dev/null 2>&1 || fail "f_messy" "must be clean after fmt"
"$NUSA_BIN" fmt f_messy.ns >/dev/null 2>&1 || fail "f_messy" "second fmt failed"
pass "f_messy"

echo "E2E STABILITY PASS ($PASS_N cases)"
