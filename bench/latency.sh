#!/usr/bin/env bash
# Latency probe v3.25.0 (Phase 124): jitter run-to-run workload tetap.
# Build sekali, eksekusi K kali, lapor min/median/max/spread.
# BUKAN klaim real-time (lihat docs/latency-3.25.md): harness pengukur,
# bukan penjamin. Baris `lat:<metrik>=<nilai>` untuk e2e.
# Pakai: bash bench/latency.sh <nusa-binary> [runs]
set -u
NUSA_BIN="${1:?usage: latency.sh <nusa-binary> [runs]}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
RUNS="${2:-15}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

lat() { printf 'lat:%s=%s\n' "$1" "$2"; }

# Beban: ping-pong bounded-channel 2000x (runtime nyata: mutex+cond
# + alokasi node) + kerja skalar. Deterministik (output tunggal).
cat > pingpong.ns <<'EOF'
fn main() {
    let c = chan_new_bounded(64);
    let i = 0;
    let s = 0;
    while i < 2000 {
        chan_send(c, i);
        s = s + chan_recv(c);
        i = i + 1;
    }
    print(s);
    chan_free(c);
}
EOF
"$NUSA_BIN_ABS" build pingpong.ns -o pp >build.log 2>&1 \
  || { echo "LATENCY FAIL [build]"; cat build.log; exit 1; }
[ "$(./pp)" = "1999000" ] || { echo "LATENCY FAIL [output]"; exit 1; }

> times.txt
for i in $(seq 1 "$RUNS"); do
  t0=$(date +%s%N)
  ./pp > /dev/null || { echo "LATENCY FAIL [run $i]"; exit 1; }
  t1=$(date +%s%N)
  echo $(( (t1 - t0) / 1000000 )) >> times.txt
done
SRT=$(sort -n times.txt)
MIN=$(echo "$SRT" | head -n 1)
MAX=$(echo "$SRT" | tail -n 1)
N=$(wc -l < times.txt)
MED=$(echo "$SRT" | sed -n "$(( (N + 1) / 2 ))p")
SPREAD=$((MAX - MIN))
echo "pingpong: n=$N min=${MIN}ms median=${MED}ms max=${MAX}ms spread=${SPREAD}ms"
lat "runs" "$N"
lat "min_ms" "$MIN"
lat "median_ms" "$MED"
lat "max_ms" "$MAX"
lat "spread_ms" "$SPREAD"
echo "LATENCY DONE"
