#!/usr/bin/env bash
# Integration test (Phase 45 lanjutan): paket .deb Termux.
# Bangun via packaging/deb.sh dari build tree aktif, verifikasi
# metadata + layout + jalankan biner hasil ekstrak terisolasi
# (tanpa menyentuh $PREFIX asli).
set -u
NUSA_BIN="${1:?usage: e2e_deb.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }

command -v dpkg-deb >/dev/null 2>&1 || { echo "SKIP: no dpkg-deb"; exit 0; }

# 1. Bangun .deb (cepat: arsipkan build tree, tanpa rebuild compiler).
bash "$ROOT/packaging/deb.sh" --output ./nusa-test.deb \
  > build.log 2>&1 || { cat build.log; fail "deb-build" "rc != 0"; }
[ -f nusa-test.deb ] || fail "deb-build" "missing output"
echo "case deb-build OK"

# 2. Metadata kontrol.
[ "$(dpkg-deb -f nusa-test.deb Package)" = "nusa" ] \
  || fail "deb-meta" "Package"
VER_BIN="$("$NUSA_BIN" --version | sed -E 's/^nusa version ([0-9.]+).*/\1/')"
VER_DEB="$(dpkg-deb -f nusa-test.deb Version)"
[ "$VER_DEB" = "$VER_BIN-1" ] \
  || fail "deb-meta" "Version ($VER_DEB != $VER_BIN-1)"
[ "$(dpkg-deb -f nusa-test.deb Architecture)" = "$(uname -m)" ] \
  || fail "deb-meta" "Architecture"
dpkg-deb -f nusa-test.deb Depends | grep -q "clang" \
  || fail "deb-meta" "Depends clang"
echo "case deb-meta OK"

# 3. Layout instalasi ($PREFIX relatif, exe-relative resolution).
PREFIX_REL="data/data/com.termux/files/usr"
dpkg-deb -x nusa-test.deb inst || fail "deb-extract" "rc != 0"
[ -x "inst/$PREFIX_REL/bin/nusa" ] || fail "deb-layout" "bin/nusa"
[ -f "inst/$PREFIX_REL/bin/nusa_rt.o" ] || fail "deb-layout" "nusa_rt.o"
[ -f "inst/$PREFIX_REL/runtime/nusa_rt.c" ] \
  || fail "deb-layout" "runtime src"
N_STD="$(ls "$ROOT"/std/*.ns | wc -l)"
I_STD="$(ls inst/$PREFIX_REL/std/*.ns | wc -l)"
[ "$N_STD" = "$I_STD" ] || fail "deb-layout" "std count $I_STD != $N_STD"
[ -f "inst/$PREFIX_REL/share/doc/nusa/CHANGELOG.md" ] \
  || fail "deb-layout" "docs"
echo "case deb-layout OK"

# 4. Biner hasil ekstrak jalan mandiri (stdlib+runtime ketemu
# relatif ke exe, tanpa $PREFIX asli).
NUSA_PKG="$PWD/inst/$PREFIX_REL/bin/nusa"
"$NUSA_PKG" --version > /dev/null 2>&1 || fail "deb-run" "version"
printf 'fn main() {\n    print("Hello, deb!");\n}\n' > hi.ns
OUT="$("$NUSA_PKG" run hi.ns 2>/dev/null)" || fail "deb-run" "hello"
[ "$OUT" = "Hello, deb!" ] || fail "deb-run" "output '$OUT'"
printf 'import std.math;\nfn main() {\n    print(fact(5));\n}\n' > m.ns
OUT="$("$NUSA_PKG" run m.ns 2>/dev/null)" || fail "deb-run" "stdlib"
[ "$OUT" = "120" ] || fail "deb-run" "stdlib output '$OUT'"
echo "case deb-run OK"

# 5. Halaman unduhan (layout scripts/gen.py): ada + menyebut versi
# biner aktif + semua tautan .deb-nya menunjuk berkas yang ADA.
PAGE="$ROOT/website/downloads/index.html"
[ -f "$PAGE" ] || fail "deb-page" "missing downloads/index.html"
grep -q "nusa_${VER_BIN}" "$PAGE" \
  || fail "deb-page" "current version $VER_BIN not listed"
grep -hoE 'href="[^"]+\.deb"' "$PAGE" \
  | sed 's/^href="//; s/"$//' | sort -u | while read -r rel; do
  case "$rel" in
    http*|/*) continue;; # absolut/eksternal: di luar kontrak file
  esac
  [ -f "$ROOT/website/downloads/$rel" ] \
    || [ -f "$ROOT/website/$rel" ] \
    || { echo "E2E FAIL [deb-page]: missing $rel"; exit 1; }
done || fail "deb-page" "dangling deb link"
echo "case deb-page OK"

echo "E2E DEB PASS (5 cases)"
