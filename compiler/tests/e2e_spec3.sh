#!/usr/bin/env bash
# Conformance test 3.0 (Phase 82): Nusa Language Specification 3.0 final.
# Mengunci: 19 topik mandated berurutan + status normatif +
# independensi implementasi (§21) + traceability + probe runtime.
set -u
NUSA_BIN="${1:?usage: e2e_spec3.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
SPEC="$ROOT/docs/language-spec-3.0.md"
[ -f "$SPEC" ] || fail "spec3" "missing docs/language-spec-3.0.md"

# 1. Penanda normatif + versi 3.0 + pengganti 2.0.
grep -q "Status: \*\*normative\*\*" "$SPEC" || fail "spec3" "not normative"
grep -q "Language Specification 3.0" "$SPEC" || fail "spec3" "no version"
grep -q "docs/language-spec-2.0.md" "$SPEC" || fail "spec3" "no supersede pointer"
echo "case markers OK"

# 2. 19 topik mandated (header eksak, berurutan).
i=0
for t in "Lexical grammar" "Syntax" "Types" "Generics" "Traits" "Enums" \
         "Pattern matching" "Memory model" "Ownership" "Concurrency" "Async" \
         "Modules" "Packages" "Foreign function interface" "Compile-time execution" "Macros" \
         "Error handling" "Runtime semantics" "Platform semantics"; do
  i=$((i + 1))
  grep -q "^## $i\. $t\$" "$SPEC" || fail "spec3" "missing section $i ($t)"
done
echo "case sections-19 OK"

# 3. Split 3-dokumen dirujuk.
for d in "docs/language-spec-3.0.md" "docs/compiler.md" \
         "docs/standard-library.md"; do
  grep -q "$d" "$SPEC" || fail "split" "missing $d"
done
echo "case split OK"

# 4. Independensi implementasi: istilah terlarang TAK BOLEH muncul
# sebelum §21 (badan normatif); §21+Appendix hanya berisi aturan/UB.
BODY="$(sed -n '1,/^## 21\./p' "$SPEC" | sed '$d')"
if printf '%s' "$BODY" | grep -qiE "llvm|clang|verifymodule|codegen|typechecker|parser\.cpp|lexer\.cpp|define i32|\.ll\b"; then
  fail "independence" "implementation detail leaked into normative body"
fi
echo "case independence OK"

# 5. Traceability: setiap `e2e_*.sh` yang dirujuk WAJIB ada.
grep -o 'e2e_[a-z0-9_]*\.sh' "$SPEC" | sort -u | while read -r t; do
  [ -f "$ROOT/compiler/tests/$t" ] || { echo "E2E FAIL [trace]: missing $t"; exit 1; }
done || fail "trace" "dangling Tests pointer"
echo "case traceability OK"

# 6. Probe runtime §18: wrap i64 teramati.
cat > wrap.ns <<'EOF'
fn main() {
    print(9223372036854775807 + 1);
}
EOF
OUT="$("$NUSA_BIN" run wrap.ns 2>wrap.log)" || { cat wrap.log; fail "probe" "wrap rejected"; }
[ "$OUT" = "-9223372036854775808" ] || { echo "$OUT"; fail "probe" "no two's-complement wrap"; }
echo "case probe-wrap OK"

# 7. Probe reserved: async/macro/div-nol-konstan DITOLAK check.
printf 'fn main() {\n let async: i64 = 1;\n print(async);\n}\n' > rej_async.ns
"$NUSA_BIN" check rej_async.ns > /dev/null 2>&1 && fail "probe" "async accepted"
printf 'fn main() {\n macro foo;\n}\n' > rej_macro.ns
"$NUSA_BIN" check rej_macro.ns > /dev/null 2>&1 && fail "probe" "macro accepted"
printf 'fn main() {\n print(1 / 0);\n}\n' > rej_div0.ns
"$NUSA_BIN" check rej_div0.ns > /dev/null 2>&1 && fail "probe" "const div-zero accepted"
echo "case probe-reject OK"

echo "E2E SPEC3 PASS (7 cases)"
