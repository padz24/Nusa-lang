#!/usr/bin/env bash
# Verification conformance v2.22.0 (Phase 71, RFC-0010 slice 1).
# Fondasi verifikasi: invarian I-RUN diferensial (O0=O2,
# run=build+exec), determinisme check, tanpa-crash, fuzz berseed.
# Konvensi: rc > 128 = CRASH (gagal); gagal-bersih rc != 0 tanpa sinyal.
set -u
NUSA_BIN="${1:?usage: e2e_verify.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
FUZZ_N="${NUSA_VERIFY_FUZZ_N:-40}"
FUZZ_SEED="${NUSA_VERIFY_FUZZ_SEED:-71}"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# `nusa run` di-retry sekali bila kena timeout tool (rc=124) —
# beban paralel ctest bisa membuat satu invokasi lambat; program
# fuzz bounded-terminating sehingga retry tetap aman (maks 2x).
# Gagal-bersih lain (rc!=0 bukan 124) langsung gagal.
run_prog() {
  local file="$1" out="$2" err="$3"
  timeout 45 "$NUSA_BIN" run "$file" >"$out" 2>"$err"
  local rc=$?
  if [ $rc -eq 124 ]; then
    timeout 45 "$NUSA_BIN" run "$file" >"$out" 2>"$err"
    rc=$?
  fi
  return $rc
}

# Build O0+O2 lalu exec: stdout+exit harus identik (I-RUN).
diff_opt() {
  local name="$1"
  timeout 60 "$NUSA_BIN" build "$name.ns" -O0 -o "$name.o0" >"$name.b0.log" 2>&1
  local rc0=$?
  crashed "$rc0" && fail "$name-O0" "CRASH rc=$rc0"
  [ $rc0 -eq 0 ] || { cat "$name.b0.log"; fail "$name-O0" "build O0 rc=$rc0"; }
  timeout 60 "$NUSA_BIN" build "$name.ns" -O2 -o "$name.o2" >"$name.b2.log" 2>&1
  local rc2=$?
  crashed "$rc2" && fail "$name-O2" "CRASH rc=$rc2"
  [ $rc2 -eq 0 ] || { cat "$name.b2.log"; fail "$name-O2" "build O2 rc=$rc2"; }
  timeout 20 ./"$name.o0" >"$name.r0" 2>"$name.e0"; local e0=$?
  timeout 20 ./"$name.o2" >"$name.r2" 2>"$name.e2"; local e2=$?
  [ $e0 -eq $e2 ] || fail "$name" "exit O0=$e0 vs O2=$e2"
  cmp -s "$name.r0" "$name.r2" || fail "$name" "stdout O0 vs O2 beda"
  pass "$name-opt-equiv"
  run_prog "$name.ns" "$name.rr" "$name.re" || fail "$name" "run rc!=0"
  cmp -s "$name.r0" "$name.rr" || fail "$name" "run vs build+exec beda"
  pass "$name-run-equiv"
}

echo "== 1. diferensial korpus =="
cat > d_arith.ns <<'EOF'
const K: i64 = 7;
fn sq(x: i64) -> i64 {
    return x * x + K;
}
fn main() {
    let s = 0;
    let i = 1;
    while i <= 10 {
        s = s + sq(i);
        i = i + 1;
    }
    print(s);
    print(K * 1000 + s);
}
EOF
diff_opt d_arith
cat > d_match.ns <<'EOF'
enum E { A, B }
fn f(e: E) -> i64 {
    match e {
        A => {
            return 41;
        },
        _ => {
            return 1;
        },
    }
    return 0;
}
fn main() {
    print(f(E.A) + f(E.B));
    if f(E.A) == 41 {
        print("yes");
    } else {
        print("no");
    }
}
EOF
diff_opt d_match
cat > d_str.ns <<'EOF'
fn main() {
    let t = [3, 1, 2];
    print(len(t));
    print(t[0] + t[1] + t[2]);
}
EOF
diff_opt d_str

echo "== 2. determinisme check =="
timeout 20 "$NUSA_BIN" check d_arith.ns >c1.log 2>&1; r1=$?
timeout 20 "$NUSA_BIN" check d_arith.ns >c2.log 2>&1; r2=$?
crashed "$r1" && fail "check-det" "CRASH rc=$r1"
[ $r1 -eq $r2 ] || fail "check-det" "rc $r1 vs $r2"
cmp -s c1.log c2.log || fail "check-det" "diagnostik tak-deterministik"
pass "check-det"
printf 'fn main() { let x: i64 = "boom"; }\n' > d_bad.ns
timeout 20 "$NUSA_BIN" check d_bad.ns >b1.log 2>&1; q1=$?
timeout 20 "$NUSA_BIN" check d_bad.ns >b2.log 2>&1; q2=$?
[ $q1 -ne 0 ] || fail "check-bad" "rc=0"
crashed "$q1" && fail "check-bad" "CRASH rc=$q1"
[ $q1 -eq $q2 ] || fail "check-bad" "rc $q1 vs $q2"
cmp -s b1.log b2.log || fail "check-bad" "diagnostik tak-deterministik"
pass "check-bad-det"

echo "== 3. tanpa-crash input malformed =="
printf '' > x_empty.ns
printf 'fn main() { print("hi); }\n' > x_unterm.ns
printf '\x00\xff\xfe bad \n' > x_bytes.ns
python3 -c "print('fn main() { print(' + '('*600 + '1' + ')'*600 + '); }')" > x_nest.ns
for x in x_empty x_unterm x_bytes x_nest; do
  timeout 20 "$NUSA_BIN" check "$x.ns" >"$x.log" 2>&1; rc=$?
  crashed "$rc" && fail "$x" "CRASH rc=$rc"
  [ $rc -ne 0 ] || fail "$x" "rc=0 atas input malformed"
  pass "$x-nocrash"
done

echo "== 4. fuzz berseed (seed=$FUZZ_SEED n=$FUZZ_N) =="
mkdir -p fuzz && cd fuzz || exit 1
python3 "$SRC_DIR/compiler/tests/fuzz_gen.py" "$FUZZ_SEED" "$FUZZ_N" . >manifest.txt \
  || fail "fuzz-gen" "generator gagal"
while read -r f tag; do
  timeout 20 "$NUSA_BIN" check "$f" >"$f.c1" 2>&1; a=$?
  timeout 20 "$NUSA_BIN" check "$f" >"$f.c2" 2>&1; b=$?
  crashed "$a" && fail "$f" "CRASH check rc=$a"
  [ $a -eq $b ] || fail "$f" "check rc $a vs $b"
  cmp -s "$f.c1" "$f.c2" || fail "$f" "check tak-deterministik"
  if [ "$tag" = bad ]; then
    [ $a -ne 0 ] || fail "$f" "defect lolos check"
    pass "$f-reject"
    continue
  fi
  [ $a -eq 0 ] || { cat "$f.c1"; fail "$f" "ok-tag gagal check"; }
  timeout 60 "$NUSA_BIN" build "$f" -O0 -o "$f.o0" >"$f.b0" 2>&1 \
    || { cat "$f.b0"; fail "$f" "build O0 gagal"; }
  timeout 60 "$NUSA_BIN" build "$f" -O2 -o "$f.o2" >"$f.b2" 2>&1 \
    || { cat "$f.b2"; fail "$f" "build O2 gagal"; }
  timeout 20 ./"$f.o0" >"$f.r0" 2>"$f.e0"; e0=$?
  timeout 20 ./"$f.o2" >"$f.r2" 2>"$f.e2"; e2=$?
  [ $e0 -ne 124 ] || fail "$f" "timeout O0 (non-terminating?)"
  [ $e2 -ne 124 ] || fail "$f" "timeout O2 (non-terminating?)"
  run_prog "$f" "$f.rr" "$f.re"; er=$?
  run_prog "$f" "$f.rr2" "$f.re2"; er2=$?
  [ $e0 -eq $e2 ] && [ $e0 -eq $er ] && [ $er -eq $er2 ] \
    || fail "$f" "exit O0=$e0 O2=$e2 run=$er run2=$er2"
  cmp -s "$f.r0" "$f.r2" || fail "$f" "O0 vs O2 beda"
  cmp -s "$f.r0" "$f.rr" || fail "$f" "run vs build beda"
  cmp -s "$f.rr" "$f.rr2" || fail "$f" "run tak-deterministik"
  pass "$f-fuzz"
done < manifest.txt
cd "$TMPDIR" || exit 1

echo "E2E VERIFY PASS ($PASS_N cases)"
