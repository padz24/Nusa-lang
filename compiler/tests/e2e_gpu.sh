#!/usr/bin/env bash
# GPU conformance v3.21.0 (Phase 120, milestone 1): paket `opencl`
# (shim dlopen + wrapper) — query angka perangkat via FFI.
# Guard toolchain (cermin sqlite e2e_data); kontrak struktural agar
# portabel (device boleh absen di mesin lain → SKIP-device, tetap PASS).
set -u
NUSA_BIN="${1:?usage: e2e_gpu.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

if ! command -v cc >/dev/null 2>&1; then
  echo "SKIP-gpu: no cc"
  echo "E2E GPU PASS ($PASS_N cases)"
  exit 0
fi
cp -r "$SRC_DIR/packages/opencl" ocl-pkg
cc -c ocl-pkg/shim.c -o ocl-pkg/shim.o >cc.log 2>&1 \
  || { cat cc.log; fail "gpu-cc" "shim build failed"; }
pass "gpu-cc"

"$NUSA_BIN" pkg init gpuapp --dir app >pkg.log 2>&1 \
  || { cat pkg.log; fail "gpu-init" "pkg init failed"; }
cp ocl-pkg/shim.o app/shim.o
cd app || exit 1
"$NUSA_BIN" pkg add opencl --path ../ocl-pkg >pkg.log 2>&1 \
  || { cat pkg.log; fail "gpu-add" "pkg add failed"; }
"$NUSA_BIN" pkg update >pkg.log 2>&1 \
  || { cat pkg.log; fail "gpu-update" "pkg update failed"; }
cat >> nusa.toml <<'EOF'

[link]
files = ["shim.o"]
libraries = ["dl"]

[capabilities]
native = true
EOF
cat > main.ns <<'EOF'
import opencl;
fn main() {
    print(gpu_platforms());
    print(gpu_devices());
    print(gpu_class());
    print(gpu_present());
}
EOF
"$NUSA_BIN" pkg build >pkg.log 2>&1 \
  || { cat pkg.log; fail "gpu-build" "pkg build failed"; }
[ -x target/gpuapp ] || fail "gpu-build" "target binary missing"
./target/gpuapp > app.got 2>app.err || { cat app.err; fail "gpu-run" "run failed"; }
# Kontrak struktural: tiga baris i64 (boleh negatif = gagal query)
# + satu baris bool; nilai mutlak dibiarkan perangkat.
NPLAT=$(sed -n '1p' app.got); NDEV=$(sed -n '2p' app.got)
NCLASS=$(sed -n '3p' app.got); NPRES=$(sed -n '4p' app.got)
[[ "$NPLAT" =~ ^-?[0-9]+$ ]] || { cat app.got; fail "gpu-run" "platforms bukan i64"; }
[[ "$NDEV" =~ ^-?[0-9]+$ ]] || { cat app.got; fail "gpu-run" "devices bukan i64"; }
[[ "$NCLASS" =~ ^-?[0-9]+$ ]] || { cat app.got; fail "gpu-run" "class bukan i64"; }
[ "$NPRES" = "true" ] || [ "$NPRES" = "false" ] || { cat app.got; fail "gpu-run" "present bukan bool"; }
# Konsistensi: ada device => ada platform; kelas nonzero bila ada device.
if [ "$NDEV" -gt 0 ]; then
  [ "$NPLAT" -gt 0 ] || fail "gpu-run" "device tanpa platform";
  [ "$NCLASS" -ne 0 ] || fail "gpu-run" "device tanpa kelas";
  [ "$NPRES" = "true" ] || fail "gpu-run" "present salah";
  echo "gpu-device: platforms=$NPLAT devices=$NDEV class=$NCLASS"
else
  echo "SKIP-device: no OpenCL device here (platforms=$NPLAT)"
fi
pass "gpu-run"

echo "E2E GPU PASS ($PASS_N cases)"
