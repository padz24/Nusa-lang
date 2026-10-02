#!/usr/bin/env bash
# Conformance test 2.0 (Phase 45): Nusa Language Specification 2.0.
# Cermin e2e_spec.sh (1.0, tetap hijau sebagai riwayat) + probe
# BREAKING B1 dua arah (natif diterima, bentuk lama ditolak).
set -u
NUSA_BIN="${1:?usage: e2e_spec2.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
SPEC="$ROOT/docs/language-spec-2.0.md"
[ -f "$SPEC" ] || fail "spec" "missing docs/language-spec-2.0.md"

# 1. Penanda normatif + versi 2.0.
grep -q "Status: \*\*normative\*\*" "$SPEC" || fail "spec" "not normative"
grep -q "Language Specification 2.0" "$SPEC" || fail "spec" "no version"
grep -q "2.0.0" "$SPEC" || fail "spec" "no compiler version"
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

# 3. Split 3-dokumen + migrasi dirujuk.
for d in "docs/language-spec-2.0.md" "docs/compiler.md" \
         "docs/standard-library.md" "docs/migration-2.0.md"; do
  grep -q "$d" "$SPEC" || fail "split" "missing $d"
done
echo "case split OK"

# 4. Traceability: setiap `Tests:` menunjuk file e2e yang ADA.
grep -o 'e2e_[a-z0-9_]*\.sh' "$SPEC" | sort -u | while read -r t; do
  [ -f "$ROOT/compiler/tests/$t" ] || { echo "E2E FAIL [trace]: missing $t"; exit 1; }
done || fail "trace" "dangling Tests pointer"
echo "case traceability OK"

# 5. Probe B1: natif diterima (match + ? + ctor eksplisit).
cat > probe_ok.ns <<'EOF'
import option;
import result;
fn get(s: string) -> Result<i64, string> {
    match parse_int_opt(s) {
        Some(v) => { return Result<i64, string>.Ok(v); },
        None => { return Result<i64, string>.Err("bad"); },
    }
    return Result<i64, string>.Err("bad");
}
fn get2(s: string) -> Result<i64, string> {
    let n = get(s)?;
    return Result<i64, string>.Ok(n + 1);
}
fn main() {
    print(res_unwrap_or(get2("41"), 0));
    print(res_unwrap_or(get2("xx"), 0));
    print(opt_unwrap_or(opt_map(parse_int_opt("21"), |x| x * 2), 0));
}
EOF
OUT="$("$NUSA_BIN" run probe_ok.ns 2>probe_ok.log)" || { cat probe_ok.log; fail "probe" "native rejected"; }
[ "$OUT" = "$(printf '42\n0\n42')" ] || { echo "$OUT"; fail "probe" "wrong semantics"; }
echo "case probe-native OK"

# 6. Probe B1: bentuk lama DITOLAK (dummy + field).
printf 'import option;\nfn main() { print(none(0)); }\n' > probe_old1.ns
"$NUSA_BIN" check probe_old1.ns > /dev/null 2>&1 && fail "probe" "none(dummy) accepted"
printf 'import option;\nfn f(o: Option<i64>) -> i64 {\n    if o.has_value {\n        return o.value;\n    }\n    return 0;\n}\nfn main() {\n}\n' > probe_old2.ns
"$NUSA_BIN" check probe_old2.ns > /dev/null 2>&1 && fail "probe" "field access accepted"
echo "case probe-reject OK"

echo "E2E SPEC2 PASS (6 cases)"
