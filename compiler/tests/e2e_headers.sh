#!/usr/bin/env bash
# Headers conformance v3.11.0 (Phase 110, milestone 1): Nusa -> C.
# Subset jujur-ABI + bukti ujung-ke-ujung: objek Nusa di-link program
# C dan dipanggil lintas batas (resep embedding didokumentasikan di
# header yang dihasilkan).
set -u
NUSA_BIN="${1:?usage: e2e_headers.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Golden: struct + enum + fn (+ -o identik).
cat > api.ns <<'EOF'
struct Point {
    x: i64;
    name: string;
}
enum E {
    Some(i64),
    None,
}
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}
fn mkpoint(v: i64) -> Point {
    return Point { x: v, name: "p" };
}
fn get(e: E) -> i64 {
    match e {
        Some(v) => { return v; },
        None => { return 0; },
    }
    return 0;
}
fn main() {
}
EOF
timeout 60 "$NUSA_BIN" headers api.ns > api.h 2>api.warn \
  || { cat api.warn; fail "hdr-golden" "generate gagal"; }
grep -q "int64_t add(int64_t a, int64_t b);" api.h \
  || { cat api.h; fail "hdr-golden" "fn hilang"; }
grep -q "} Point;" api.h || { cat api.h; fail "hdr-golden" "struct hilang"; }
grep -q "} E;" api.h || { cat api.h; fail "hdr-golden" "enum hilang"; }
grep -q "int64_t v0_0; /\* Some \*/" api.h \
  || { cat api.h; fail "hdr-golden" "slot enum hilang"; }
timeout 60 "$NUSA_BIN" headers api.ns -o api2.h 2>/dev/null \
  || fail "hdr-o" "-o gagal"
diff api.h api2.h > /dev/null || fail "hdr-o" "-o beda dari stdout"
pass "hdr-golden"

# 2. Ujung-ke-ujung C -> Nusa: header + objek + link + run.
# (Objek Nusa membawa main sendiri -> --allow-multiple-definition
# agar main C menang; didokumentasikan di header.)
timeout 180 "$NUSA_BIN" build api.ns -c -o api.o >build.log 2>&1 \
  || { cat build.log; fail "hdr-link" "build -c gagal"; }
cat > caller.c <<'EOF'
#include <stdio.h>
#include "api.h"
int main(void) {
    printf("%lld\n", (long long)add(20, 22));
    Point p = mkpoint(7);
    printf("%lld %s\n", (long long)p.x, p.name);
    E a = {0, 7};
    E b = {1, 0};
    printf("%lld %lld\n", (long long)get(a), (long long)get(b));
    return 0;
}
EOF
clang caller.c api.o -Wl,--allow-multiple-definition -o caller \
  >link.log 2>&1 || { cat link.log; fail "hdr-link" "link C gagal"; }
timeout 60 ./caller >caller.got 2>caller.err \
  || { cat caller.err; fail "hdr-run" "run gagal"; }
printf '42\n7 p\n7 0\n' | cmp -s caller.got - \
  || { cat caller.got; fail "hdr-run" "output lintas-batas salah"; }
pass "hdr-linkrun"

# 3. Penolakan jujur: bool/closure/generik -> warning, rc tetap 0
# selama ada API terekspor; nol terekspor -> rc 1.
cat > mix.ns <<'EOF'
fn isyes(b: bool) -> bool {
    return b;
}
fn keep(n: i64) -> i64 {
    return n;
}
fn main() {
}
EOF
timeout 60 "$NUSA_BIN" headers mix.ns > mix.h 2>mix.warn \
  || { cat mix.warn; fail "hdr-refuse" "rc != 0 padahal ada terekspor"; }
grep -q "keep" mix.h || { cat mix.h; fail "hdr-refuse" "fn valid hilang"; }
grep -q "isyes" mix.h && { cat mix.h; fail "hdr-refuse" "fn bool bocor"; }
grep -q "'bool'" mix.warn || { cat mix.warn; fail "hdr-refuse" "tanpa warning bool"; }
printf 'fn only(b: bool) -> bool {\n    return b;\n}\nfn main() {\n}\n' > boolonly.ns
if timeout 60 "$NUSA_BIN" headers boolonly.ns > /dev/null 2>&1; then
  fail "hdr-empty" "rc=0 tanpa API terekspor"
fi
pass "hdr-refuse"

# 4. Negatif: ekstensi + input rusak.
if timeout 60 "$NUSA_BIN" headers api.h > /dev/null 2>&1; then
  fail "hdr-ext" "rc=0 untuk .h"
fi
printf 'fn main() { @ }\n' > bad.ns
if timeout 60 "$NUSA_BIN" headers bad.ns > /dev/null 2>&1; then
  fail "hdr-bad" "rc=0 untuk input rusak"
fi
pass "hdr-negative"

echo "E2E HEADERS PASS ($PASS_N cases)"
