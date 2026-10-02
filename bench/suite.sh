#!/usr/bin/env bash
# Benchmark suite resmi v3.0.0-beta (Phase 86): 8 dimensi performa.
# Semua di TMPDIR (sumber disalin; repo tak tersentuh); tanpa network.
# Output: tabel manusia + baris `perf:<dim>=<nilai>` untuk e2e.
# Pakai: bash bench/suite.sh <nusa-binary>
set -u
NUSA_BIN="${1:?usage: suite.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

us_now() { echo "${EPOCHREALTIME/./}"; }
us_fmt() { printf "%d.%02d" "$(($1 / 1000000))" "$((($1 / 10000) % 100))"; }
ms_fmt() { printf "%d.%01d" "$(($1 / 1000))" "$((($1 / 100) % 10))"; }
perf() { printf 'perf:%s=%s\n' "$1" "$2"; }

echo "== 1. compile time =="
cp "$SRC_DIR/examples/hello.ns" "$SRC_DIR/examples/nusa10.ns" .
{ printf 'fn main() {\n'; for i in $(seq 1 2000); do printf '    let v%d = %d * 2 + 1;\n    print(v%d);\n' "$i" "$i" "$i"; done; printf '}\n'; } > perf2000.ns
for c in "hello hello.ns" "nusa10 nusa10.ns" "perf2000 perf2000.ns"; do
  set -- $c
  t0=$(us_now)
  "$NUSA_BIN" build "$2" -o "$1.out" >"$1.log" 2>&1 || { echo "SUITE FAIL [$1]"; cat "$1.log"; exit 1; }
  t1=$(us_now)
  dt=$((t1 - t0))
  sz=$(wc -c < "$1.out")
  printf "%-10s build %8s s   exe %7d bytes\n" "$1" "$(us_fmt "$dt")" "$sz"
  perf "compile_${1}_s" "$(us_fmt "$dt")"
  perf "size_${1}_bytes" "$sz"
done
t0=$(us_now)
"$NUSA_BIN" build "$SRC_DIR/selfhost/parse.ns" -o parse.out >parse.log 2>&1 \
  || { echo "SUITE FAIL [parse]"; cat parse.log; exit 1; }
t1=$(us_now)
dt=$((t1 - t0))
sz=$(wc -c < parse.out)
printf "%-10s build %8s s   exe %7d bytes\n" "parse" "$(us_fmt "$dt")" "$sz"
perf "compile_parse_s" "$(us_fmt "$dt")"
perf "size_parse_bytes" "$sz"

echo "== 2. incremental compile =="
mkdir -p incr && cp "$SRC_DIR/examples/hello.ns" incr/
t0=$(us_now)
(cd incr && "$NUSA_BIN_ABS" build hello.ns -o h.out >h1.log 2>&1) || { echo "SUITE FAIL [incr-cold]"; exit 1; }
t1=$(us_now)
cold=$((t1 - t0))
t0=$(us_now)
(cd incr && "$NUSA_BIN_ABS" build hello.ns -o h.out >h2.log 2>&1) || { echo "SUITE FAIL [incr-warm]"; exit 1; }
t1=$(us_now)
warm=$((t1 - t0))
# Best-of-3 (praktik baku; tahan distorsi beban paralel).
for i in 2 3; do
  t0=$(us_now)
  (cd incr && "$NUSA_BIN_ABS" build hello.ns -o h.out >h2.log 2>&1) || { echo "SUITE FAIL [incr-warm]"; exit 1; }
  t1=$(us_now)
  dt=$((t1 - t0))
  if [ "$dt" -lt "$warm" ]; then warm=$dt; fi
done
[ "$warm" -le "$cold" ] || { echo "SUITE FAIL [incr] warm>cold"; exit 1; }
printf "hello cold %8s s   warm %8s s\n" "$(us_fmt "$cold")" "$(us_fmt "$warm")"
perf "incr_cold_s" "$(us_fmt "$cold")"
perf "incr_warm_s" "$(us_fmt "$warm")"

echo "== 3. binary size =="
cbin=$(wc -c < "$NUSA_BIN")
printf "nusa-compiler %15d bytes\n" "$cbin"
perf "size_compiler_bytes" "$cbin"

echo "== 4. startup time (hello exe, min 5x) =="
best=0
for i in 1 2 3 4 5; do
  t0=$(us_now)
  ./hello.out > /dev/null 2>&1 || { echo "SUITE FAIL [startup]"; exit 1; }
  t1=$(us_now)
  dt=$((t1 - t0))
  if [ "$best" -eq 0 ] || [ "$dt" -lt "$best" ]; then best=$dt; fi
done
printf "hello startup min %8s ms\n" "$(ms_fmt "$best")"
perf "startup_hello_ms" "$(ms_fmt "$best")"

echo "== 5/6. runtime + memory (nusa bench -n 3) =="
for prog in fib loopsum strcat matchdispatch closureapply; do
  cp "$SRC_DIR/bench/programs/$prog.ns" .
  line=$("$NUSA_BIN" bench "$prog.ns" -n 3 2>&1 | grep "^bench:") \
    || { echo "SUITE FAIL [$prog]"; exit 1; }
  echo "$prog $line"
  mm=$(echo "$line" | sed -E 's/.*min ([0-9.]+)ms.*/\1/')
  md=$(echo "$line" | sed -E 's/.*median ([0-9.]+)ms.*/\1/')
  mx=$(echo "$line" | sed -E 's/.*max ([0-9.]+)ms.*/\1/')
  rss=$(echo "$line" | sed -E 's/.*maxrss ([0-9]+)KiB.*/\1/')
  perf "run_${prog}_min_ms" "$mm"
  perf "run_${prog}_median_ms" "$md"
  perf "run_${prog}_max_ms" "$mx"
  perf "rss_${prog}_kib" "$rss"
done

echo "== 7. package resolution =="
"$NUSA_BIN" new cli-app perfapp --dir proj >/dev/null 2>&1 || { echo "SUITE FAIL [pkg-new]"; exit 1; }
t0=$(us_now)
(cd proj && "$NUSA_BIN_ABS" pkg tree >/dev/null 2>&1) || { echo "SUITE FAIL [pkg-tree]"; exit 1; }
t1=$(us_now)
dt=$((t1 - t0))
printf "pkg-tree %8s s\n" "$(us_fmt "$dt")"
perf "pkg_tree_s" "$(us_fmt "$dt")"
t0=$(us_now)
(cd proj && "$NUSA_BIN_ABS" pkg build >/dev/null 2>&1) || { echo "SUITE FAIL [pkg-build]"; exit 1; }
t1=$(us_now)
dt=$((t1 - t0))
printf "pkg-build %8s s\n" "$(us_fmt "$dt")"
perf "pkg_build_s" "$(us_fmt "$dt")"

echo "== 8. LSP latency (initialize, cold) =="
ms=$(python3 - "$NUSA_BIN_ABS" <<'PYEOF'
import subprocess, json, sys, time
body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                   "params": {"rootUri": None, "capabilities": {}}}).encode()
msg = b"Content-Length: %d\r\n\r\n" % len(body) + body
t0 = time.time()
p = subprocess.Popen([sys.argv[1], "lsp"], stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
p.stdin.write(msg)
p.stdin.flush()
out = b""
while b"\r\n\r\n" not in out:
    ch = p.stdout.read(1)
    if not ch:
        break
    out += ch
dt = (time.time() - t0) * 1000.0
p.kill()
if not out.startswith(b"Content-Length:"):
    sys.exit(1)
print("%.1f" % dt)
PYEOF
) || { echo "SUITE FAIL [lsp]"; exit 1; }
printf "lsp-initialize %8s ms\n" "$ms"
perf "lsp_init_ms" "$ms"

echo "SUITE DONE"
