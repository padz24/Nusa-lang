#!/usr/bin/env bash
# Bangun paket .deb Nusa untuk Termux (install global via apt/dpkg).
#
#   packaging/deb.sh [--version V] [--build-dir DIR] [--output FILE]
#                    [--prefix PREFIX]
#
# Layout (relatif ke /): Termux $PREFIX (default
# /data/data/com.termux/files/usr):
#   <PREFIX>/bin/nusa            compiler (resolusi std/runtime
#   <PREFIX>/bin/nusa_rt.o        relatif ke exe — tanpa postinst)
#   <PREFIX>/runtime/nusa_rt.c   fallback sumber runtime (agregator
#   <PREFIX>/runtime/rt_*.c|h     Runtime 2.0: subsistem + platform)
#   <PREFIX>/std/*.ns            standard library (16 modul)
#   <PREFIX>/share/doc/nusa/     README + CHANGELOG + copyright
#
# Depends: clang (driver memanggil `clang`; libLLVM/libc++_shared
# ikut transitif karena clang sendiri menautnya — diverifikasi
# readelf NEEDED; tanpa hardcode nama paket turunan).
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION=""
BUILD_DIR="$ROOT/build"
OUTPUT=""
PREFIX="/data/data/com.termux/files/usr"

while [ $# -gt 0 ]; do
  case "$1" in
    --version) VERSION="$2"; shift 2;;
    --build-dir) BUILD_DIR="$2"; shift 2;;
    --output) OUTPUT="$2"; shift 2;;
    --prefix) PREFIX="$2"; shift 2;;
    -h|--help)
      sed -n '2,16p' "$0"; exit 0;;
    *) echo "error: unknown option '$1'" >&2; exit 1;;
  esac
done

if [ -z "$VERSION" ]; then
  VERSION="$(sed -nE 's/^project\(nusa VERSION ([0-9.]+).*/\1/p' \
    "$ROOT/CMakeLists.txt" | head -n 1)"
  [ -n "$VERSION" ] || { echo "error: cannot read version" >&2; exit 1; }
fi
ARCH="$(uname -m)"
if [ "$ARCH" != "aarch64" ]; then
  echo "warning: ARCH=$ARCH bukan aarch64 (paket Termux menarget aarch64)" >&2
fi
if [ -z "$OUTPUT" ]; then
  OUTPUT="$ROOT/website/downloads/nusa_${VERSION}-1_${ARCH}.deb"
fi
for f in "$BUILD_DIR/nusa" "$BUILD_DIR/nusa_rt.o" "$ROOT/runtime/nusa_rt.c"; do
  [ -f "$f" ] || { echo "error: missing $f (build dulu)" >&2; exit 1; }
done
command -v dpkg-deb >/dev/null 2>&1 \
  || { echo "error: dpkg-deb tidak ada" >&2; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
PKG="$STAGE/pkg"
REL="${PREFIX#/}"  # tanpa leading / (deb menyimpan path absolut)
mkdir -p "$PKG/$REL/bin" "$PKG/$REL/runtime" "$PKG/$REL/std" \
         "$PKG/$REL/share/doc/nusa" "$PKG/DEBIAN"

cp "$BUILD_DIR/nusa" "$PKG/$REL/bin/nusa"
cp "$BUILD_DIR/nusa_rt.o" "$PKG/$REL/bin/nusa_rt.o"
cp "$ROOT"/runtime/nusa_rt.c "$ROOT"/runtime/rt_*.c "$ROOT"/runtime/rt_*.h "$PKG/$REL/runtime/"
cp "$ROOT"/std/*.ns "$PKG/$REL/std/"
cp "$ROOT/README.md" "$ROOT/CHANGELOG.md" "$PKG/$REL/share/doc/nusa/"
cat > "$PKG/$REL/share/doc/nusa/copyright" <<'EOF'
Nusa programming language — compiled with LLVM.
Dibuat oleh Padil (Banyumas, Jawa Tengah).
Lihat CHANGELOG.md untuk riwayat per versi.
EOF
chmod 755 "$PKG/$REL/bin/nusa"
chmod 644 "$PKG/$REL/bin/nusa_rt.o" "$PKG/$REL/runtime/"*.c "$PKG/$REL/runtime/"*.h \
          "$PKG/$REL"/std/*.ns "$PKG/$REL"/share/doc/nusa/*

cat > "$PKG/DEBIAN/control" <<'EOF'
Package: nusa
Version: __VERSION__-1
Architecture: __ARCH__
Maintainer: Padil (Nusa)
Depends: clang
Description: Nusa programming language (compiled .ns to native)
 Nusa adalah bahasa pemrograman compiled dengan backend LLVM,
 target utama Android AArch64 / Termux. Paket ini memasang
 compiler (nusa), runtime precompiled, standard library, dan
 dokumentasi agar `nusa` tersedia global di Termux.
 .
 Install: apt install ./nusa___VERSION__-1___ARCH__.deb
Homepage: local (lihat README.md di share/doc/nusa)
EOF
sed -i "s/__VERSION__/$VERSION/g; s/__ARCH__/$ARCH/g" "$PKG/DEBIAN/control"

mkdir -p "$(dirname "$OUTPUT")"
dpkg-deb -b "$PKG" "$OUTPUT" >/dev/null \
  || { echo "error: dpkg-deb gagal" >&2; exit 1; }
echo "built $OUTPUT"
dpkg-deb -f "$OUTPUT" Package Version Architecture Depends
