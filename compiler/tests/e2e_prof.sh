#!/usr/bin/env bash
# Profiler conformance v2.16.0 (Phase 65, milestone 1).
# `build --stats` (rincian compile) + `bench` (N run, min/median,
# maxRSS, output diteruskan). Bukan benchmark resmi (itu `bench/`).
set -u
NUSA_BIN="${1:?usage: e2e_prof.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== build --stats =="
printf 'fn main() {\n    print(6 * 7);\n}\n' > st.ns
"$NUSA_BIN" build st.ns --fresh --stats -o st >st.log 2>&1 || { cat st.log; fail "stats" "build gagal"; }
grep -q "build-stats:" st.log || { cat st.log; fail "stats" "baris hilang"; }
grep -q "frontend .*ms codegen .*ms clang .*ms total .*ms ir .*b exe .*b" st.log || { cat st.log; fail "stats" "format salah"; }
[ "$(./st)" = "42" ] || fail "stats" "run salah"
# Konsistensi ukuran terhadap berkas nyata.
IRB=$(wc -c < st.ns.ll)
EXEB=$(wc -c < st)
grep -q "ir ${IRB}b exe ${EXEB}b" st.log || { cat st.log; fail "stats" "ukuran tak cocok ($IRB/$EXEB)"; }
pass "stats"
# Tanpa --stats: tak ada baris (dan cache dipakai seperti biasa).
"$NUSA_BIN" build st.ns -o st2 >st2.log 2>&1 || { cat st2.log; fail "stats-off" "build gagal"; }
if grep -q "build-stats:" st2.log; then fail "stats-off" "bocor tanpa flag"; fi
pass "stats-off"

echo "== bench =="
printf 'fn main() {\n    print(40 + 2);\n}\n' > b.ns
"$NUSA_BIN" bench b.ns -n 3 >b.out 2>b.err || { cat b.err; fail "bench" "rc != 0"; }
grep -q "^42$" b.out || { cat b.out; fail "bench" "output program hilang/rusak"; }
grep -Eq "bench: 3 runs min [0-9]+\.[0-9]ms median [0-9]+\.[0-9]ms max [0-9]+\.[0-9]ms" b.err || { cat b.err; fail "bench" "format salah"; }
grep -Eq "maxrss [0-9]+KiB" b.err || { cat b.err; fail "bench" "maxrss hilang"; }
pass "bench"
# Default N=5 + teruskan argumen program.
printf 'fn main() {\n    print(process_argc());\n}\n' > barg.ns
"$NUSA_BIN" bench barg.ns -- a b >barg.out 2>barg.err || { cat barg.err; fail "bench-args" "rc != 0"; }
grep -q "^3$" barg.out || { cat barg.out; fail "bench-args" "argv tak terus"; }
grep -q "bench: 5 runs" barg.err || { cat barg.err; fail "bench-args" "default N salah"; }
pass "bench-args"
# N invalid + program gagal.
"$NUSA_BIN" bench b.ns -n 0 >bn.log 2>&1
rc=$?
if crashed "$rc"; then cat bn.log; fail "bench-badn" "CRASH"; fi
[ $rc -ne 0 ] || fail "bench-badn" "N=0 harus gagal"
pass "bench-badn"
printf 'fn main() {\n    print(x);\n}\n' > bbad.ns
"$NUSA_BIN" bench bbad.ns -n 2 >bbad.out 2>bbad.err
rc=$?
if crashed "$rc"; then cat bbad.err; fail "bench-badprog" "CRASH"; fi
[ $rc -ne 0 ] || fail "bench-badprog" "program rusak harus rc != 0"
pass "bench-badprog"

echo "== pkg build --stats =="
"$NUSA_BIN" pkg init statapp --dir statapp >/dev/null 2>&1 || fail "stats-pkg" "init gagal"
cd statapp || exit 1
"$NUSA_BIN" pkg build --stats >pb.log 2>&1 || { cat pb.log; fail "stats-pkg" "build gagal"; }
grep -q "build-stats:" pb.log || { cat pb.log; fail "stats-pkg" "baris hilang"; }
pass "stats-pkg"
cd "$TMPDIR" || exit 1

echo "E2E PROF PASS ($PASS_N cases)"
