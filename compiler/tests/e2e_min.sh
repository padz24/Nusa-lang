#!/usr/bin/env bash
# Minimal-runtime conformance v2.19.0 (Phase 68, milestone 1).
# Biner kecil: tanpa-runtime bila tak dipakai + gc-sections.
set -u
NUSA_BIN="${1:?usage: e2e_min.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== biner minimal (tanpa runtime) =="
printf 'fn main() {\n    print("Hello, Nusa!");\n}\n' > tiny.ns
"$NUSA_BIN" build tiny.ns -o tiny >tiny.log 2>&1 || { cat tiny.log; fail "tiny" "build gagal"; }
[ "$(./tiny)" = "Hello, Nusa!" ] || fail "tiny" "run salah"
SZ=$(wc -c < tiny)
[ "$SZ" -lt 9000 ] || fail "tiny" "terlalu besar: $SZ bytes (batas 9000)"
echo "tiny: $SZ bytes"
pass "tiny"

echo "== runtime terpakai tetap penuh =="
cat > worker.ns <<'EOF'
fn work() -> i64 {
    return 40 + 2;
}
fn main() {
    let h = thread_spawn(work);
    print(thread_join(h));
}
EOF
"$NUSA_BIN" build worker.ns -o worker >worker.log 2>&1 || { cat worker.log; fail "worker" "build gagal"; }
[ "$(./worker)" = "42" ] || fail "worker" "run salah"
WSZ=$(wc -c < worker)
[ "$WSZ" -lt 20000 ] || fail "worker" "terlalu besar: $WSZ bytes (batas 20000, baseline 2.18 ~22KB+)"
echo "worker: $WSZ bytes"
pass "worker"

echo "== string/array (libc saja, tanpa runtime) =="
cat > mix.ns <<'EOF'
fn main() {
    let a: [i64] = [1, 2, 3];
    print(len(a));
    print(str_len("Nusa"));
    print(a[0] + a[2]);
}
EOF
"$NUSA_BIN" build mix.ns -o mix >mix.log 2>&1 || { cat mix.log; fail "mix" "build gagal"; }
printf '3\n4\n4\n' | cmp -s <(./mix) - || { ./mix; fail "mix" "run salah"; }
MSZ=$(wc -c < mix)
[ "$MSZ" -lt 9000 ] || fail "mix" "terlalu besar: $MSZ bytes"
pass "mix"

echo "== seam alokator (Phase 123): override weak via --link =="
if ! command -v cc >/dev/null 2>&1; then
  echo "SKIP-alloc: no cc"
else
cat > myalloc.c <<'EOF'
#include <stddef.h>
#include <stdlib.h>
static long long n_alloc = 0;
void *nusa_alloc(size_t n) { n_alloc++; return malloc(n); }
void nusa_free(void *p) { free(p); }
long long alloc_count(void) { return n_alloc; }
EOF
cc -c myalloc.c -o myalloc.o >cc.log 2>&1 || { cat cc.log; fail "alloc-cc" "shim build failed"; }
cat > cnt.ns <<'EOF'
extern fn alloc_count() -> i64;
fn main() {
    print(str_concat("x", "y"));
    print(alloc_count());
}
EOF
timeout 120 "$NUSA_BIN" build cnt.ns -o cnt --link ./myalloc.o >cnt.log 2>&1 \
  || { cat cnt.log; fail "alloc-override" "build gagal"; }
printf 'xy\n1\n' | cmp -s <(./cnt) - || { ./cnt; fail "alloc-override" "override tak jalan"; }
pass "alloc-override"
cat > nullalloc.c <<'EOF'
#include <stddef.h>
void *nusa_alloc(size_t n) { (void)n; return 0; }
void nusa_free(void *p) { (void)p; }
EOF
cc -c nullalloc.c -o nullalloc.o >cc2.log 2>&1 || { cat cc2.log; fail "alloc-null-cc" "shim build failed"; }
cat > cf.ns <<'EOF'
fn main() {
    print(str_concat("x", "y"));
}
EOF
timeout 120 "$NUSA_BIN" build cf.ns -o cntfail --link ./nullalloc.o >cf.log 2>&1 \
  || { cat cf.log; fail "alloc-failfast" "build gagal"; }
if timeout 15 ./cntfail >cntfail.got 2>&1; then fail "alloc-failfast" "rc=0 dengan alokator-NULL"; fi
grep -q "out of memory (str_concat)" cntfail.got || { cat cntfail.got; fail "alloc-failfast" "bukan OOM bersih"; }
pass "alloc-failfast"
fi

echo "E2E MIN PASS ($PASS_N cases)"
