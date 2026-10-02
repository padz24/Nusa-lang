#!/usr/bin/env bash
# Bench compiler v1.1.0: waktu `nusa build` + ukuran executable.
# Baseline performa (deviasi wajar antar-run di perangkat yang sama).
# Pakai: bash bench/bench.sh <nusa-binary>
set -u
NUSA_BIN="${1:?usage: bench.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# Waktu integer mikrodetik (murni bash; tanpa awk/date).
us_now() { echo "${EPOCHREALTIME/./}"; }
us_fmt() { # us_fmt <mikrodetik> → "S.CC dtk"
  printf "%d.%02d" "$(($1 / 1000000))" "$((($1 / 10000) % 100))"
}

build_case() { # build_case <nama> <file.ns>
  local t0 t1 dt sz
  t0=$(us_now)
  "$NUSA_BIN" build "$2" -o "$1.out" >"$1.log" 2>&1 || { echo "BENCH FAIL [$1]"; cat "$1.log"; exit 1; }
  t1=$(us_now)
  dt=$((t1 - t0))
  sz=$(wc -c < "$1.out")
  printf "%-10s build %8s s   exe %7d bytes\n" "$1" "$(us_fmt "$dt")" "$sz"
}

cp "$SRC_DIR/examples/hello.ns" .
cp "$SRC_DIR/examples/nusa10.ns" .
{ printf 'fn main() {\n'; for i in $(seq 1 2000); do printf '    let v%d = %d * 2 + 1;\n    print(v%d);\n' "$i" "$i" "$i"; done; printf '}\n'; } > perf2000.ns

echo "== nusa build =="
build_case hello hello.ns
build_case nusa10 nusa10.ns
build_case perf2000 perf2000.ns
echo "== runtime (O0) =="
for prog in fib loopsum strcat matchdispatch closureapply; do
  cp "$SRC_DIR/bench/programs/$prog.ns" .
  t0=$(us_now)
  "$NUSA_BIN" build "$prog.ns" -o "$prog.out" >"$prog.log" 2>&1 || { echo "BENCH FAIL [$prog]"; cat "$prog.log"; exit 1; }
  t1=$(us_now)
  dt=$((t1 - t0))
  sz=$(wc -c < "$prog.out")
  r0=$(us_now)
  ./"$prog.out" > "$prog.got" 2>&1
  r1=$(us_now)
  rdt=$((r1 - r0))
  printf "%-10s build %8s s   exe %7d bytes   run %8s s\n" "$prog" "$(us_fmt "$dt")" "$sz" "$(us_fmt "$rdt")"
done
echo "== compiler =="
printf "nusa-compiler %15d bytes\n" "$(wc -c < "$NUSA_BIN")"
"$NUSA_BIN" --version
