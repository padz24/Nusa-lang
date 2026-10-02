#!/usr/bin/env bash
# Conformance test v1.20.0 (Phase 40): Nusa Language Specification 1.0.
# Mengunci: 13 topik ada, split 3-dokumen dinyatakan, setiap topik
# menunjuk file uji pengunci yang ada, + spot-check normatif dieksekusi.
set -u
NUSA_BIN="${1:?usage: e2e_spec.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
SPEC="$ROOT/docs/language-spec-1.0.md"
[ -f "$SPEC" ] || fail "spec" "missing docs/language-spec-1.0.md"

# 1. Penanda normatif + versi.
grep -q "Status: \*\*normative\*\*" "$SPEC" || fail "spec" "not normative"
grep -q "Language Specification 1.0" "$SPEC" || fail "spec" "no version"
grep -q "1.20.0" "$SPEC" || fail "spec" "no compiler version"
echo "case markers OK"

# 2. 13 topik §1–§13 (header eksak, berurutan).
i=0
for t in "Lexical grammar" "Syntax" "AST semantics" "Type system" \
         "Memory model" "Evaluation rules" "Modules and packages" \
         "Generics" "Traits" "Pattern matching" "Concurrency" \
         "Foreign function interface" "Standard library"; do
  i=$((i + 1))
  grep -q "^## $i\. $t\$" "$SPEC" || fail "spec" "missing section $i ($t)"
done
echo "case sections-13 OK"

# 3. Split 3-dokumen dinyatakan (nama file eksak).
for d in "docs/language-spec-1.0.md" "docs/compiler.md" \
         "docs/standard-library.md"; do
  grep -q "$d" "$SPEC" || fail "split" "missing $d"
done
echo "case split OK"

# 4. Traceability: setiap baris `Tests:` menunjuk file e2e yang ADA.
grep -o 'e2e_[a-z0-9_]*\.sh' "$SPEC" | sort -u | while read -r t; do
  [ -f "$ROOT/compiler/tests/$t" ] || { echo "E2E FAIL [trace]: missing $t"; exit 1; }
done || fail "trace" "dangling Tests pointer"
echo "case traceability OK"

# 5. Spot-check normatif dieksekusi (§2/§6/§10: precedence, match, exhaustiveness).
cat > probe_ok.ns <<'EOF'
fn main() {
    let x = 10 + 20 * 3;
    print(x);
    match x {
        70 => { print("seventy"); },
        _ => { print("other"); },
    }
}
EOF
"$NUSA_BIN" run probe_ok.ns > probe_ok.got 2>probe_ok.log || { cat probe_ok.log; fail "probe" "valid program rejected"; }
printf '70\nseventy\n' | diff - probe_ok.got || fail "probe" "wrong semantics"
printf 'fn main() {\n    match true {\n        true => { print("t"); },\n    }\n}\n' > probe_bad.ns
"$NUSA_BIN" check probe_bad.ns > /dev/null 2>&1 && fail "probe" "non-exhaustive match accepted"
echo "case probes OK"

echo "E2E SPEC PASS (5 cases)"
