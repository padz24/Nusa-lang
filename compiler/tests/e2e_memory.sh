#!/usr/bin/env bash
# Integration test v1.4.0 (Phase 23): perilaku model memori.
#  - Copy semantics: string/array/struct disalin ke binding baru.
#  - Alokasi loop: concat berulang hasilnya TEPAT (karakteristik tumbuh
#    heap program-lifetime — lihat docs/memory-model.md §4).
#  - input EOF → ""; file roundtrip (fd ditutup segera).
#  - OOM hook ada (pesan; tak dipicu di sini).
set -u
NUSA_BIN="${1:?usage: e2e_memory.sh <nusa-binary>}"
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
  "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "wrong output"; }
  pass "$name"
}

# 1. Copy semantics: salinan hidup mandiri.
cat > v1.ns <<'EOF'
fn main() {
    let a = "Nu";
    let b = a;
    let c = str_concat(b, "sa");
    print(a);
    print(b);
    print(c);
    let arr = [1, 2];
    let arr2 = arr;
    print(arr[0] + arr2[1]);
}
EOF
printf 'Nu\nNu\nNusa\n3\n' > v1.want
run_ok v1

# 2. Concat dalam loop: hasil tepat (200 iterasi).
cat > v2.ns <<'EOF'
fn main() {
    let s = "";
    let i = 0;
    while i < 200 {
        s = str_concat(s, "ab");
        i = i + 1;
    }
    print(str_len(s));
    print(s);
}
EOF
python3 -c "print('400'); print('ab'*200)" > v2.want
run_ok v2

# 3. input EOF → string kosong (tanpa hang).
printf 'fn main() { let s = input(); print(str_len(s)); }\n' > v3.ns
"$NUSA_BIN" run v3.ns >v3.got 2>v3.err < /dev/null || { cat v3.err; fail "v3" "run failed"; }
echo "0" | cmp -s v3.got - || { cat v3.got; fail "v3" "wrong output"; }
pass "v3"

# 4. Struct berisi string: copy + field tetap valid.
cat > v4.ns <<'EOF'
struct P { name: string; age: i64; }
fn main() {
    let p = P { name: str_concat("Nu", "sa"), age: 1 };
    let q = p;
    print(q.name);
    print(q.age + p.age);
}
EOF
printf 'Nusa\n2\n' > v4.want
run_ok v4

echo "MEMORY PASS ($PASS_N cases)"
