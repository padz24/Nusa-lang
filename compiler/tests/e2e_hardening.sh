#!/usr/bin/env bash
# Integration test v0.9.5 (hardening):
#  A. Matriks negative wajib MASTER PROMPT (9 kasus).
#  B. Robustness: nesting dalam, korpus malformed, rantai import,
#     div-by-zero, CLI edges — compiler tak boleh crash/hang.
#  C. Regression bug terdokumentasi + sanity performa.
# Konvensi: build/run yang "gagal" harus exit != 0 TANPA sinyal
# (rc > 128 atau 134/139/132 = CRASH → FAIL). Tiap kasus dibatasi timeout.
set -u
NUSA_BIN="${1:?usage: e2e_hardening.sh <nusa-binary>}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# crash_rc: 0=ok jujur (rc<=128), 1=crash/sinyal
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

# expect_fail <name> <pattern> — build gagal bersih + pesan cocok.
expect_fail() {
  local name="$1" pattern="$2"
  timeout 20 "$NUSA_BIN" build "$name.ns" -o "$name.out" >"$name.log" 2>&1
  local rc=$?
  if crashed "$rc"; then fail "$name" "COMPILER CRASH (rc=$rc)"; fi
  [ $rc -ne 0 ] || fail "$name" "should not build (rc=0)"
  grep -q "$pattern" "$name.log" || { echo "--- log ---"; cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

echo "== A. matriks negative wajib =="
printf 'fn main() { let x = ; }\n' > neg_syntax.ns
expect_fail neg_syntax "expected expression"

printf 'fn main() { print(undeclared_var); }\n' > neg_undef.ns
expect_fail neg_undef "undefined variable 'undeclared_var'"

printf 'fn main() { let x = 1; let x = 2; }\n' > neg_dup.ns
expect_fail neg_dup "duplicate variable 'x'"

printf 'fn main() { let x: i64 = "s"; }\n' > neg_mismatch.ns
expect_fail neg_mismatch "mismatched types"

printf 'fn add(a: i64, b: i64) -> i64 { return a + b; }\nfn main() { print(add(1)); }\n' > neg_arity.ns
expect_fail neg_arity "wrong number of arguments"

printf 'fn f() -> i64 { let x = 1; }\nfn main() { }\n' > neg_return.ns
expect_fail neg_return "must return a value"

printf 'struct P { x: i64; }\nfn main() { let p = P { x: 1 }; print(p.y); }\n' > neg_field.ns
expect_fail neg_field "unknown field 'y'"

# NB: indeks OOB konstanta lolos statis (bounds = runtime, diuji di C).
# Di sini: indeks boolean → invalid array index statis:
printf 'fn main() { let a = [1]; print(a[true]); }\n' > neg_idx.ns
expect_fail neg_idx "must be of type 'i64'"

printf 'import nosuchmodule_xyz;\nfn main() { }\n' > neg_module.ns
expect_fail neg_module "cannot read module file"

echo "== B. robustness =="
# B1. nesting dalam: tolak bersih, bukan SIGSEGV
printf 'fn main() { print(' > deep.ns
printf '%.0s(' $(seq 1 20000) >> deep.ns
printf '1' >> deep.ns
printf '%.0s)' $(seq 1 20000) >> deep.ns
printf '); }\n' >> deep.ns
timeout 30 "$NUSA_BIN" build deep.ns -o deep.out >deep.log 2>&1
rc=$?
if crashed "$rc"; then fail "deep-expr" "COMPILER CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "deep-expr" "should not build"
grep -q "too deeply nested" deep.log || fail "deep-expr" "guard message missing"
pass "deep-expr"

python3 -c "print('fn main() {' + 'if true {'*5000 + 'print(1);' + '}'*5000 + '}')" > deepblock.ns 2>/dev/null || {
  { printf 'fn main() {'; for _ in $(seq 1 5000); do printf 'if true {'; done; printf 'print(1);'; for _ in $(seq 1 5000); do printf '}'; done; printf '}'; } > deepblock.ns
}
timeout 30 "$NUSA_BIN" build deepblock.ns -o deepblock.out >deepblock.log 2>&1
rc=$?
if crashed "$rc"; then fail "deep-block" "COMPILER CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "deep-block" "should not build"
grep -q "too deeply nested" deepblock.log || fail "deep-block" "guard message missing"
pass "deep-block"

# B1b. rantai biner/unary/index dalam (parser iteratif, walker menolak)
python3 -c "print('fn main() { print(' + '1+'*20000 + '1); }')" > binchain.ns 2>/dev/null || {
  { printf 'fn main() { print('; for _ in $(seq 1 20000); do printf '1+'; done; printf '1); }'; } > binchain.ns
}
timeout 30 "$NUSA_BIN" build binchain.ns -o binchain.out >binchain.log 2>&1
rc=$?
if crashed "$rc"; then fail "bin-chain" "COMPILER CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "bin-chain" "should not build"
grep -q "too deeply nested" binchain.log || fail "bin-chain" "guard message missing"
pass "bin-chain"

# B2. korpus malformed: tiap file harus gagal bersih (tanpa crash/hang)
printf '\x00\x01\x02\xff\xfe' > garbage1.ns
printf 'fn main() { print("unterminated); }\n' > garbage2.ns
printf 'fn main() { let x = 99999999999999999999999; }\n' > garbage3.ns
printf 'fn main() { ((((((((;' > garbage4.ns
printf '{{{{{{{{{{' > garbage5.ns
printf 'fn main() { print(a[0][0][0][0); }\n' > garbage6.ns
printf '\xef\xbb\xbf\xe2\x98\x83 garbage \xe2\x98\x83\n' > garbage7.ns
printf 'fn main() { let %s = 1; }\n' "$(python3 -c 'print("x"*5000)' 2>/dev/null || printf 'xxx')" > garbage8.ns
printf 'fn main() { let = 1; }\n' > garbage8b.ns
printf '' > garbage9.ns
printf '   \n\t // only a comment\n' > garbage10.ns
printf 'import ' > garbage11.ns
printf 'fn main() { "aaaaaaaaaaaaaaaa' > garbage12.ns
for g in garbage1 garbage2 garbage3 garbage4 garbage5 garbage6 garbage7 garbage8b garbage9 garbage10 garbage11 garbage12; do
  timeout 20 "$NUSA_BIN" build "$g.ns" -o "$g.out" >"$g.log" 2>&1
  rc=$?
  if crashed "$rc"; then echo "--- log ---"; cat "$g.log"; fail "$g" "COMPILER CRASH (rc=$rc)"; fi
  [ $rc -ne 0 ] || fail "$g" "should not build (rc=0)"
  pass "$g"
done

# B3. self-import → circular (bukan hang)
printf 'import selfmod;\nfn main() { }\n' > selfmod.ns
printf 'import selfmod;\nfn main() { }\n' > selfmod_check.ns
printf 'import selfloop;\nfn main() { }\n' > selfloop.ns
timeout 20 "$NUSA_BIN" build selfloop.ns -o selfloop.out >selfloop.log 2>&1
rc=$?
if crashed "$rc"; then fail "self-import" "COMPILER CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "self-import" "should not build"
grep -q "circular import detected" selfloop.log || fail "self-import" "cycle message missing"
pass "self-import"

# B4. rantai import 150 dalam → "too deep" bersih (tanpa stack overflow)
for i in $(seq 0 149); do printf 'import chain%d;\n' $((i + 1)) > "chain$i.ns"; done
printf 'fn helper() -> i64 { return 1; }\n' > chain150.ns
printf 'import chain0;\nfn main() { print(1); }\n' > chainmain.ns
timeout 60 "$NUSA_BIN" build chainmain.ns -o chainmain.out >chainmain.log 2>&1
rc=$?
if crashed "$rc"; then fail "deep-chain" "COMPILER CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "deep-chain" "should not build"
grep -q "too deep" chainmain.log || fail "deep-chain" "depth message missing"
pass "deep-chain"
# rantai pendek (5) tetap jalan
for i in $(seq 0 3); do printf 'import ok%d;\n' $((i + 1)) > "ok$i.ns"; done
printf 'fn five() -> i64 { return 5; }\n' > ok4.ns
printf 'import ok0;\nfn main() { print(five()); }\n' > okmain.ns
timeout 20 "$NUSA_BIN" run okmain.ns >okmain.got 2>okmain.log || fail "short-chain" "should run"
[ "$(cat okmain.got)" = "5" ] || fail "short-chain" "wrong output"
pass "short-chain"

# B5. division by zero: statis + runtime
printf 'fn main() { print(1 / 0); }\n' > divstatic.ns
expect_fail divstatic "division by zero"
printf 'fn main() { let z = 0; print(10 / z); }\n' > divdyn.ns
timeout 20 "$NUSA_BIN" run divdyn.ns >divdyn.got 2>divdyn.log
rc=$?
if crashed "$rc"; then fail "div-runtime" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "div-runtime" "should fail at runtime"
grep -q "division by zero" divdyn.got divdyn.log || fail "div-runtime" "message missing"
pass "div-runtime"

# B6. CLI edges
timeout 10 "$NUSA_BIN" frobnicate >cli1.log 2>&1; rc=$?
if crashed "$rc"; then fail "cli-unknown" "CRASH"; fi
[ $rc -ne 0 ] || fail "cli-unknown" "rc should be != 0"
grep -q "unknown command" cli1.log || fail "cli-unknown" "message missing"
pass "cli-unknown"

timeout 10 "$NUSA_BIN" build >cli2.log 2>&1; rc=$?
[ $rc -ne 0 ] || fail "cli-noarg" "rc should be != 0"
pass "cli-noarg"

timeout 10 "$NUSA_BIN" build cli1.log -o cli1.out >cli3.log 2>&1; rc=$?
[ $rc -ne 0 ] || fail "cli-badext" "rc should be != 0"
pass "cli-badext"

printf '\xff\xfe\x00"dangling' > clibin.ns
timeout 10 "$NUSA_BIN" fmt clibin.ns >cli4.log 2>&1; rc=$?
if crashed "$rc"; then fail "cli-fmt-garbage" "CRASH"; fi
[ $rc -ne 0 ] || fail "cli-fmt-garbage" "rc should be != 0"
pass "cli-fmt-garbage"

timeout 10 "$NUSA_BIN" doc clibin.ns >cli5.log 2>&1; rc=$?
if crashed "$rc"; then fail "cli-doc-garbage" "CRASH"; fi
[ $rc -ne 0 ] || fail "cli-doc-garbage" "rc should be != 0"
pass "cli-doc-garbage"

echo "== C. regression + performa =="
# C0. Identifier 5000 char: valid → build & run OK (robustness positif).
timeout 20 "$NUSA_BIN" run garbage8.ns >garbage8.got 2>garbage8.log
rc=$?
if crashed "$rc"; then fail "long-ident" "COMPILER CRASH (rc=$rc)"; fi
[ $rc -eq 0 ] || fail "long-ident" "valid program should run (rc=$rc)"
pass "long-ident"
# C1. regresi ambiguitas `if c {` (bug v0.8.0)
printf 'fn main() { let c = true; if c { print(1); } else { print(0); } }\n' > ambig.ns
timeout 20 "$NUSA_BIN" run ambig.ns >ambig.got 2>ambig.log || fail "ambig-if" "should run"
[ "$(cat ambig.got)" = "1" ] || fail "ambig-if" "wrong output"
pass "ambig-if"

# C2. sanity performa: 2000 statement terkompilasi wajar (< 120 dtk incl. clang)
{ printf 'fn main() {\n'; for i in $(seq 1 2000); do printf '    let v%d = %d * 2 + 1;\n    print(v%d);\n' "$i" "$i" "$i"; done; printf '}\n'; } > perf.ns
START=$(date +%s)
timeout 110 "$NUSA_BIN" build perf.ns -o perf.out >perf.log 2>&1 || fail "perf-build" "should build in time"
END=$(date +%s)
echo "perf-build took $((END - START))s"
timeout 30 ./perf.out >perf.got 2>&1 || fail "perf-run" "should run"
[ "$(wc -l < perf.got)" = "2000" ] || fail "perf-run" "wrong line count"
pass "perf-build-run"

echo "HARDENING PASS ($PASS_N cases)"
