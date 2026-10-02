#!/usr/bin/env bash
# Bindgen conformance v2.17.0 (Phase 66, milestone 1).
# Subset C → `extern fn`: golden eksak, fmt-clean, ujung-ke-ujung
# (header → binding → import → link → run), penolakan.
set -u
NUSA_BIN="${1:?usage: e2e_bindgen.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== golden eksak + fmt =="
cat > sample.h <<'EOF'
#ifndef SAMPLE_H
#define SAMPLE_H
#include <stddef.h>

long long answer(void);
long long twice(long long x);
int isupper(int c);
void tick(void);
unsigned count(unsigned n);
size_t length(const char *s);
const char *name(void);
float ratio(float x);
int printf(const char *fmt, ...);
struct Point { int x; int y; };
static inline int one(void) { return 1; }

#ifdef __cplusplus
extern "C" {
#endif
long absval(long v);
#ifdef __cplusplus
}
#endif
#endif
EOF
"$NUSA_BIN" bindgen sample.h -o sample.ns >gen.log 2>&1 || { cat gen.log; fail "golden" "rc != 0"; }
cat > sample.want <<'EOF'
extern fn answer() -> i64;
extern fn twice(x: i64) -> i64;
extern fn isupper(c: i64) -> i64;
extern fn tick();
extern fn count(n: i64) -> i64;
extern fn length(s: string) -> i64;
extern fn absval(v: i64) -> i64;
EOF
cmp -s sample.ns sample.want || { echo "--- got ---"; cat sample.ns; fail "golden" "output beda"; }
pass "golden"
grep -q "skip name" gen.log || fail "golden-warn" "warning string-return hilang"
grep -q "skip ratio" gen.log || fail "golden-warn" "warning float hilang"
grep -q "skip printf" gen.log || fail "golden-warn" "warning variadik hilang"
pass "golden-warn"
"$NUSA_BIN" fmt --check sample.ns >/dev/null 2>&1 || fail "golden-fmt" "output tak fmt-clean"
pass "golden-fmt"

echo "== ujung-ke-ujung (header → run) =="
cat > mini.h <<'EOF'
long long answer(void);
long long twice(long long x);
EOF
cat > mini.c <<'EOF'
long long answer(void) { return 42; }
long long twice(long long x) { return x * 2; }
EOF
clang -c mini.c -o mini.o || fail "e2e-cc" "clang gagal"
"$NUSA_BIN" bindgen mini.h -o ans.ns >bgen.log 2>&1 || { cat bgen.log; fail "e2e-gen" "rc != 0"; }
cat > main.ns <<'EOF'
import ans;
fn main() {
    print(answer());
    print(twice(21));
}
EOF
"$NUSA_BIN" build main.ns -o app --link ./mini.o >build.log 2>&1 || { cat build.log; fail "e2e-build" "build gagal"; }
[ "$(./app)" = "42
42" ] || fail "e2e-build" "run salah"
pass "e2e-build"

echo "== penolakan =="
printf 'struct S { int x; };\n' > onlystruct.h
"$NUSA_BIN" bindgen onlystruct.h -o os.ns >os.log 2>&1
rc=$?
if crashed "$rc"; then cat os.log; fail "empty" "CRASH"; fi
[ $rc -ne 0 ] || fail "empty" "nol-fungsi harus gagal"
pass "empty"
"$NUSA_BIN" bindgen nofile.h >nf.log 2>&1
[ $? -ne 0 ] || fail "nofile" "harus gagal"
pass "nofile"
printf 'int f(void);\n' > bad.txt
"$NUSA_BIN" bindgen bad.txt >bt.log 2>&1
[ $? -ne 0 ] || fail "badext" "harus gagal"
pass "badext"

echo "E2E BINDGEN PASS ($PASS_N cases)"
