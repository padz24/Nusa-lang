#!/usr/bin/env bash
# Architecture conformance v2.2.0 (Phase 51): batas lapisan mesin.
# Backend (codegen/) tidak boleh include Semantic Layer (semantic/).
# Registry builtin hidup di Frontend (frontend/Builtin.h) — dipakai
# bersama codegen + semantic tanpa kopling balik.
set -u
NUSA_BIN="${1:?usage: e2e_arch.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# 1. Backend tanpa include Semantic Layer (Semantic.h/TypeChecker.h).
#    (`semantic/Scopes.h` = template utilitas header-only, dikecualikan
#    eksplisit — kandidat pindah ke support/ pada milestone berikut.)
if grep -rn 'semantic/Semantic\.h\|semantic/TypeChecker\.h' "$SRC_DIR/compiler/codegen/" 2>/dev/null | grep -q .; then
  grep -rn 'semantic/Semantic\.h\|semantic/TypeChecker\.h' "$SRC_DIR/compiler/codegen/"
  fail "no-semantic-in-backend" "codegen/ masih include Semantic Layer"
fi
pass "no-semantic-in-backend"

# 2. Registry tunggal: isBuiltin didefinisikan tepat sekali (frontend/).
DEF_N=$(grep -rln 'bool isBuiltin' "$SRC_DIR/compiler/" --include=*.cpp | wc -l)
[ "$DEF_N" -eq 1 ] || fail "single-builtin-def" "definisi isBuiltin = $DEF_N (harap 1)"
grep -q 'bool isBuiltin' "$SRC_DIR/compiler/frontend/Builtin.cpp" || fail "single-builtin-def" "bukan di frontend/"
pass "single-builtin-def"

# 3. Semantic.h tidak mendeklarasikan isBuiltin (batas bersih).
if grep -q 'isBuiltin' "$SRC_DIR/compiler/semantic/Semantic.h"; then
  fail "semantic-h-clean" "Semantic.h masih menyebut isBuiltin"
fi
pass "semantic-h-clean"

# 4. Perilaku builtin tak berubah: program len/assert4028 tetap jalan.
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1
cat > arch_builtin.ns <<'EOF'
fn main() {
    let a: [i64] = [1, 2, 3];
    print(len(a));
    assert(len(a) == 3);
    print(str_len("Nusa"));
}
EOF
timeout 30 "$NUSA_BIN" run arch_builtin.ns >arch_builtin.got 2>arch_builtin.err || { cat arch_builtin.err; fail "builtin-behavior" "run gagal"; }
printf '3\n4\n' | cmp -s arch_builtin.got - || { cat arch_builtin.got; fail "builtin-behavior" "output salah"; }
pass "builtin-behavior"

echo "E2E ARCH PASS ($PASS_N cases)"
