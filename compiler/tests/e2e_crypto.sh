#!/usr/bin/env bash
# Integration test v2.27.0 (Phase 76, RFC-0011 slice 1): kriptografi.
# NIST vectors (hash), entropi OS (keygen), secure compare,
# file hash, misuse bertipe. Cross-check independen bila ada python3.
set -u
NUSA_BIN="${1:?usage: e2e_crypto.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
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

expect_fail() {
  local name="$1" pattern="$2"
  timeout 60 "$NUSA_BIN" check "$name.ns" >"$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should not check (rc=0)"
  [ $rc -le 128 ] || fail "$name" "CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

# 1. Vektor NIST (kosong/abc/448-bit/55/56/64-byte 'a').
cat > v_empty.ns <<'EOF'
fn main() {
    print(sha256_hex(""));
}
EOF
printf 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n' > v_empty.want
run_ok v_empty
cat > v_abc.ns <<'EOF'
fn main() {
    print(sha256_hex("abc"));
}
EOF
printf 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n' > v_abc.want
run_ok v_abc
cat > v_448.ns <<'EOF'
fn main() {
    print(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
}
EOF
printf '248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1\n' > v_448.want
run_ok v_448
cat > v_pad.ns <<'EOF'
fn main() {
    print(sha256_hex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    print(sha256_hex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    print(sha256_hex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
}
EOF
printf '9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318\nb35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a\nffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb\n' > v_pad.want
run_ok v_pad

# 2. Cross-check independen (python3 hashlib bila ada).
if command -v python3 >/dev/null 2>&1; then
  RND="$(python3 -c "import random,string; print(''.join(random.choice(string.ascii_letters) for _ in range(64)))")"
  python3 -c "import hashlib; print(hashlib.sha256('$RND'.encode()).hexdigest())" > x.want
  printf 'fn main() {\n print(sha256_hex("%s"));\n}\n' "$RND" > x.ns
  run_ok x
else
  echo "SKIP-xcheck: no python3"
fi

# 3. Keygen: panjang + heks-valid + dua kunci beda + n=0 eksplisit.
cat > k_gen.ns <<'EOF'
import crypto;
import encoding;
fn main() {
    let k1 = crypto_key_hex(16);
    let k2 = crypto_key_hex(16);
    print(str_len(k1));
    print(hex_valid_any(k1));
    print(hex_valid(k1));
    print(crypto_compare(k1, k2));
    print(crypto_rand_hex(0));
    print(str_len(crypto_rand_hex(4)));
}
EOF
printf '32\ntrue\nfalse\nfalse\n\n8\n' > k_gen.want
run_ok k_gen

# 4. Compare: sama/beda/panjang-beda (semua rc 0).
cat > c_cmp.ns <<'EOF'
import crypto;
fn main() {
    print(crypto_compare("abc", "abc"));
    print(crypto_compare("abc", "abd"));
    print(crypto_compare("", ""));
    print(crypto_compare("abc", "abcd"));
}
EOF
printf 'true\nfalse\ntrue\nfalse\n' > c_cmp.want
run_ok c_cmp

# 5. File hash + fail-closed keygen anak (rc!=0, tanpa crash).
printf 'hello\n' > f_data.txt
cat > f_hash.ns <<'EOF'
import crypto;
fn main() {
    print(sha256_file("f_data.txt"));
}
EOF
python3 -c "import hashlib; print(hashlib.sha256(open('f_data.txt','rb').read()).hexdigest())" > f_hash.want 2>/dev/null \
  || printf '5891b5b522d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6d\n' > f_hash.want
run_ok f_hash
cat > k_bad.ns <<'EOF'
import crypto;
fn main() {
    print(crypto_key_hex(0 - 5));
}
EOF
timeout 60 "$NUSA_BIN" run k_bad.ns >k_bad.log 2>&1
[ $? -ne 0 ] || fail "k-bad" "should not run (rc=0)"
grep -q "FAIL crypto_key_hex" k_bad.log || { cat k_bad.log; fail "k-bad" "fail-closed message missing"; }
pass "k-bad"

# 6. Misuse bertipe.
printf 'fn main() { print(sha256_hex(1)); }\n' > m1.ns
expect_fail m1 "a 'string' argument"
printf 'fn main() { print(crypto_rand_hex("16")); }\n' > m2.ns
expect_fail m2 "an 'i64' argument"

echo "E2E CRYPTO PASS ($PASS_N cases)"
