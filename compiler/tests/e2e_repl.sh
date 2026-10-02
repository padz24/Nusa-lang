#!/usr/bin/env bash
# REPL conformance v3.7.0 (Phase 106, milestone 1): AOT-loop di atas
# runBuild (tanpa bahasa kedua, tanpa JIT). stdin terpipa = skrip
# sesi; prompt hanya di TTY sehingga output terpipa bersih.
set -u
NUSA_BIN="${1:?usage: e2e_repl.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

# 1. Statement + ekspresi + state lintas baris + blok multi-baris.
# (Semantik putar-ulang AOT: tiap giliran sukses mengeksekusi seluruh
# sesi — "30" terulang per giliran; lihat docs/repl-3.7.md.)
printf 'let x = 10;\nx + 20\nlet t = 0;\nif t == 0 {\nt = 5;\n}\nt\n:quit\n' \
  | timeout 300 "$NUSA_BIN" repl >sess.got 2>sess.err \
  || { cat sess.err; fail "repl-session" "rc != 0"; }
printf '30\n30\n30\n30\n5\n' | cmp -s sess.got - \
  || { cat sess.got; fail "repl-session" "output salah"; }
pass "repl-session"

# 2. Baris rusak tak meracuni sesi (diagnostik bersih + state utuh).
printf 'let a = 1;\nlet b = ;\na + 100\n:quit\n' \
  | timeout 300 "$NUSA_BIN" repl >pois.got 2>pois.err \
  || { cat pois.err; fail "repl-poison" "rc != 0"; }
printf '101\n' | cmp -s pois.got - \
  || { cat pois.got; fail "repl-poison" "sesi teracuni"; }
grep -q "error" pois.err || { cat pois.err; fail "repl-poison" "tanpa diagnostik"; }
pass "repl-poison"

# 3. :reset membuang state; perintah tak dikenal ditolak; bantuan ada.
printf 'let q = 7;\n:reset\nq\n:help\n:bogus\n:quit\n' \
  | timeout 300 "$NUSA_BIN" repl >rst.got 2>rst.err \
  || { cat rst.err; fail "repl-reset" "rc != 0"; }
grep -q "session reset" rst.got \
  || { cat rst.got; fail "repl-reset" "tanpa konfirmasi reset"; }
grep -q "commands" rst.got || { cat rst.got; fail "repl-reset" "tanpa :help"; }
grep -Eq "^[0-9]+$" rst.got && { cat rst.got; fail "repl-reset" "nilai bocor pasca-reset"; }
grep -q "unknown repl command" rst.err \
  || { cat rst.err; fail "repl-reset" ":bogus tak ditolak"; }
pass "repl-reset"

# 4. Definisi fn (bukan statement) ditolak bersih tanpa meracuni sesi.
printf 'fn bad() {\n}\nlet z = 3;\nz * 2\n:quit\n' \
  | timeout 300 "$NUSA_BIN" repl >fn.got 2>fn.err \
  || { cat fn.err; fail "repl-fndef" "rc != 0"; }
printf '6\n' | cmp -s fn.got - \
  || { cat fn.got; fail "repl-fndef" "sesi rusak oleh fn"; }
pass "repl-fndef"

# 5. Argumen ditolak; sesi sementara tak bocor ke cwd.
printf ':quit\n' | timeout 60 "$NUSA_BIN" repl extra >arg.got 2>arg.err \
  && fail "repl-args" "rc=0 untuk argumen"
grep -q "takes no arguments" arg.err \
  || { cat arg.err; fail "repl-args" "pesan tak jelas"; }
LEAK="$(ls | grep -E '^(session|.*\.ns\.ll|\.nusa-cache|nusa-repl)' || true)"
[ -z "$LEAK" ] || { echo "$LEAK"; fail "repl-clean" "artefak sesi bocor"; }
pass "repl-args"

echo "E2E REPL PASS ($PASS_N cases)"
