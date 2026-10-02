#!/usr/bin/env bash
# Integration test v2.26.0 (Phase 75): stdlib 3.0 end-to-end.
#  - Modul baru: core/testing/logging/encoding (murni Nusa).
#  - Tambahan: string (starts/ends/trim/ascii-case), io.read_line,
#    fs.write_file/read_file_or.
#  - `nusa test` atas tests/*_test.ns baru + jalur-gagal testing.
set -u
NUSA_BIN="${1:?usage: e2e_stdlib3.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
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
  timeout 60 "$NUSA_BIN" run "$name.ns" >"$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should not run (rc=0)"
  [ $rc -le 128 ] || fail "$name" "CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

# 1. core: nilai + relasi identitas div/mod.
cat > s3_core.ns <<'EOF'
import core;
fn main() {
    print(sign(7));
    print(sign(0 - 3));
    print(ceil_div(7, 3));
    print(mod_euclid(0 - 7, 3));
    print(div_euclid(0 - 7, 3));
    print(powmod(2, 10, 1000));
    print(ilog2(16));
    print(sum_to(100));
    let a = 0 - 27;
    let b = 5;
    print(div_euclid(a, b) * b + mod_euclid(a, b));
}
EOF
printf '1\n-1\n3\n2\n-3\n24\n4\n5050\n-27\n' > s3_core.want
run_ok s3_core

# 2. encoding: roundtrip + invalid + valid.
cat > s3_hex.ns <<'EOF'
import encoding;
fn main() {
    print(hex_encode_i64(305419896));
    print(hex_decode_i64("DeadBEEF"));
    print(hex_decode_i64("zz"));
    print(hex_valid("1a2b"));
    print(hex_valid("xyz"));
    print(hex_encode_i64(hex_decode_i64("ff")));
}
EOF
printf '12345678\n3735928559\n0\ntrue\nfalse\nff\n' > s3_hex.want
run_ok s3_hex

# 3. logging: golden prefix (stdout).
cat > s3_log.ns <<'EOF'
import logging;
fn main() {
    log_debug("d");
    log_info("i");
    log_warn("w");
    log_error("e");
}
EOF
printf '[DEBUG] d\n[INFO] i\n[WARN] w\n[ERROR] e\n' > s3_log.want
run_ok s3_log

# 4. string/io/fs tambahan.
cat > s3_misc.ns <<'EOF'
import string;
import io;
import fs;
fn main() {
    print(str_starts_with("hello", "he"));
    print(str_ends_with("hello", "lo"));
    print(str_trim("  x  "));
    print(str_to_ascii_upper("abc"));
    print(write_file("w.txt", "data42"));
    print(read_file_or("w.txt", "fb"));
    print(read_file_or("missing.txt", "fb"));
    print(file_size("w.txt"));
}
EOF
printf 'true\ntrue\nx\nABC\ntrue\ndata42\nfb\n6\n' > s3_misc.want
run_ok s3_misc

# 5. testing: jalur-gagal mati berlabel (rc!=0, tanpa crash).
cat > s3_bad.ns <<'EOF'
import testing;
fn main() {
    test_eq_i64(1, 2, "one-vs-two");
}
EOF
expect_fail s3_bad "FAIL one-vs-two"

# 6. `nusa test` atas file 3.0 dari repo (dijalankan di TMPDIR
# dengan NUSA_PATH ke std repo agar import terresolve).
export NUSA_PATH="$ROOT/std"
for t in core_test encoding_test string2_test testing_logging_test; do
  cp "$ROOT/tests/$t.ns" "$t.ns"
  timeout 60 "$NUSA_BIN" test "$t.ns" >"$t.tlog" 2>&1 || { cat "$t.tlog"; fail "test-$t" "nusa test rc!=0"; }
  pass "test-$t"
done

echo "E2E STDLIB3 PASS ($PASS_N cases)"
