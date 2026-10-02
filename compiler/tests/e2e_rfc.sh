#!/usr/bin/env bash
# Consistency test v1.20.0 (Phase 41): RFC system.
# Struktur proses (bukan compiler), jadi verifikasi = konsistensi:
# template, bagian wajib, status sah, penomoran, indeks README.
set -u
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$ROOT" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }

# 1. Kerangka proses ada.
[ -f rfcs/README.md ] || fail "frame" "rfcs/README.md missing"
[ -f rfcs/TEMPLATE.md ] || fail "frame" "rfcs/TEMPLATE.md missing"
grep -q "Proposed" rfcs/README.md || fail "frame" "no lifecycle"
echo "case frame OK"

# 2. Setiap RFC: bagian wajib + status sah + nama file.
N=0
prev=0
for f in rfcs/RFC-*.md; do
  [ -f "$f" ] || fail "rfc" "no RFC files"
  base="$(basename "$f")"
  num="$(printf '%s' "$base" | sed -E 's/^RFC-([0-9]{4})-.*\.md$/\1/')"
  [[ "$num" =~ ^[0-9]{4}$ ]] || fail "name" "$base violates RFC-NNNN-slug.md"
  num10=$((10#$num))
  [ "$num10" -eq $((prev + 1)) ] || fail "numbering" "gap after $prev ($base)"
  prev=$num10
  for sec in "## Summary" "## Motivation" "## Design" "## Alternatives" \
             "## Compatibility" "## Tests" "## Specification" \
             "## Discussion"; do
    grep -q "^$sec\$" "$f" || fail "section" "$base missing $sec"
  done
  status="$(grep -m1 '^- Status:' "$f" | sed 's/^- Status: *//')"
  case "$status" in
    Proposed|Accepted|Implemented|Rejected|Superseded) ;;
    *) fail "status" "$base has invalid status '$status'" ;;
  esac
  # Indeks README wajib mencantumkan nomornya.
  grep -q "RFC-$num" rfcs/README.md || fail "index" "$base not in README index"
  N=$((N + 1))
done
[ "$N" -ge 4 ] || fail "rfc" "expected >= 4 RFCs, got $N"
echo "case rfcs-$N OK"

# 3. Contoh master ada: generics/traits/ownership implemented, async proposed.
grep -q "^- Status: Implemented" rfcs/RFC-0001-generics.md || fail "content" "0001"
grep -q "^- Status: Implemented" rfcs/RFC-0002-traits.md || fail "content" "0002"
grep -q "^- Status: Implemented" rfcs/RFC-0003-ownership.md || fail "content" "0003"
grep -q "^- Status: Proposed" rfcs/RFC-0004-async.md || fail "content" "0004"
echo "case master-examples OK"

echo "E2E RFC PASS (3 cases)"
