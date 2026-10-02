#!/usr/bin/env bash
# Integration test v0.9.0: stdlib — builtins runtime + modul std/*.ns.
# Dijalankan dari direktori repo agar `import <std>` via exe-resolve
# bisa diverifikasi dari direktori kerja lain (TMPDIR) juga.
set -u
NUSA_BIN="${1:?usage: e2e_stdlib.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

run_case() {
  local name="$1" expected="$2" stdin_data="${3:-}"
  if [ -n "$stdin_data" ]; then
    printf '%s' "$stdin_data" | timeout 15 "$NUSA_BIN_ABS" run "$name.ns" > "$name.got" 2>"$name.log" || {
      echo "E2E FAIL [$name]: exit $?"; cat "$name.log"; exit 1;
    }
  else
    timeout 15 "$NUSA_BIN_ABS" run "$name.ns" > "$name.got" 2>"$name.log" || {
      echo "E2E FAIL [$name]: exit $?"; cat "$name.log"; exit 1;
    }
  fi
  if ! diff <(printf '%s\n' "$expected") "$name.got" > /dev/null; then
    echo "E2E FAIL [$name]:"; echo "--- expected ---"; printf '%s\n' "$expected"
    echo "--- got ---"; cat "$name.got"; exit 1
  fi
  echo "case $name OK"
}

# 1. Builtins inti: nilai runtime
cat > core.ns <<'EOF'
fn main() {
    print(str_len("hello"));
    print(str_concat("foo", "bar"));
    print(parse_int("42"));
    print(parse_int("0 - 7"));
    let a: [i64] = [3, 1, 2];
    print(array_sum(a));
    print(array_min(a));
    print(array_max(a));
    assert(array_sum(a) == 6);
}
EOF
run_case core "$(printf '5\nfoobar\n42\n0\n6\n1\n3')"

# 2. input(): baca baris stdin (tanpa newline), EOF -> ""
cat > echo.ns <<'EOF'
fn main() {
    let line = input();
    print(line);
    print(str_len(line));
}
EOF
run_case echo "$(printf 'hi there\n8')" "hi there
"

# 3. File roundtrip
cat > files.ns <<'EOF'
import fs;

fn main() {
    print(file_exists("data.txt"));
    print(file_write("data.txt", "abc"));
    print(file_exists("data.txt"));
    print(file_read("data.txt"));
    print(file_size("data.txt"));
    print(file_size("nope-missing.txt"));
}
EOF
run_case files "$(printf 'false\ntrue\ntrue\nabc\n3\n0')"

# 3b. Regresi Phase 85 (F4): file_exists di loop WAJIB baca ulang —
# slot hasil tak-boleh basi dari iterasi sebelumnya.
cat > filesloop.ns <<'EOF'
import fs;

fn main() {
    print(file_write("loop.txt", "ab"));
    let i = 0;
    while i < 4 {
        if i == 1 {
            print(file_exists("loop-nope.txt"));
        } else {
            print(file_exists("loop.txt"));
        }
        i = i + 1;
    }
}
EOF
run_case filesloop "$(printf 'true\ntrue\nfalse\ntrue\ntrue')"

# 4. Modul std via import (resolve lewat exe -> ../std)
cat > usestd.ns <<'EOF'
import math;
import string;
import fs;
import system;

fn main() {
    print(fact(5));
    print(gcd(12, 8));
    print(isqrt(17));
    print(str_len(str_repeat("ab", 3)));
    print(str_len(str_pad_left("7", 3, "0")));
    print(str_index_of("hello", "ll", 0));
    print(str_index_of("hello", "z", 0));
    print(str_contains("hello", "ell"));
    print(int_to_str(2026));
    print(copy_file("data2.txt", "data3.txt"));
    print(file_read("data2.txt"));
}
EOF
printf 'seed' > data2.txt
run_case usestd "$(printf '120\n4\n4\n6\n3\n2\n-1\ntrue\n2026\ntrue\nseed')"

# 5. assert gagal -> exit != 0 + pesan (ke stderr executable? ke stdout run)
printf 'fn main() {\n    assert(1 == 2);\n}\n' > boom.ns
if timeout 15 "$NUSA_BIN_ABS" run boom.ns > boom.got 2>boom.log; then
  echo "E2E FAIL [assert]: should fail"; exit 1
fi
grep -q "assertion failed at" boom.got boom.log || { echo "E2E FAIL [assert]:"; cat boom.got boom.log; exit 1; }
echo "case assert-fail OK"

echo "E2E STDLIB PASS"
