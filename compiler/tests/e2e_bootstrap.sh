#!/usr/bin/env bash
# Integration test v1.20.0 (Phase 39): rantai bootstrap.
# Stage 0 (C++) -> Stage 1 (biner Nusa) -> Stage 2 (output) +
# determinisme + kasus negatif CLI.
# Phase 73 (v2.24.0, bootstrap 2.0): rantai ganda — lexer (lex)
# + parser (ast via --as).
set -u
NUSA_BIN="${1:?usage: e2e_bootstrap.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
LEX="$ROOT/selfhost/lex.ns"

# 1. Rantai penuh harus PASS dan mencetak tiga stage.
"$NUSA_BIN" bootstrap "$LEX" -o stage > boot.log 2>&1 || { cat boot.log; fail "bootstrap" "rc != 0"; }
grep -q "Stage 0" boot.log || fail "bootstrap" "missing Stage 0"
grep -q "Stage 1" boot.log || fail "bootstrap" "missing Stage 1"
grep -q "Stage 2" boot.log || fail "bootstrap" "missing Stage 2"
grep -q "PASS" boot.log || fail "bootstrap" "missing PASS"
echo "case bootstrap-pass OK"

# 2. Artefak Stage 1: executable asli yang bekerja mandiri.
[ -x stage/lex_stage1 ] || fail "stage1" "not executable"
printf 'fn main() {\n}\n' > tiny.ns
"$NUSA_BIN" lex tiny.ns > ref.out 2>&1 || fail "lex" "rc != 0"
./stage/lex_stage1 tiny.ns > s1.out 2>&1 || fail "stage1" "run rc != 0"
cmp -s ref.out s1.out || { diff ref.out s1.out; fail "stage1" "output != nusa lex"; }
echo "case stage1-standalone OK"

# 3. Determinisme: dua run artefak identik.
./stage/lex_stage1 "$LEX" > d1.out 2>&1 || fail "det" "run1"
./stage/lex_stage1 "$LEX" > d2.out 2>&1 || fail "det" "run2"
cmp -s d1.out d2.out || fail "det" "non-deterministic"
echo "case determinism OK"

# 4. Kasus negatif: input hilang + ekstensi salah + flag asing.
"$NUSA_BIN" bootstrap /nonexistent.ns -o neg > /dev/null 2>&1 && fail "neg" "missing input should fail"
printf 'x' > bad.txt
"$NUSA_BIN" bootstrap bad.txt -o neg > /dev/null 2>&1 && fail "neg" "bad extension should fail"
"$NUSA_BIN" bootstrap "$LEX" --bogus > /dev/null 2>&1 && fail "neg" "bad flag should fail"
"$NUSA_BIN" bootstrap "$LEX" --as bogus -o neg > /dev/null 2>&1 && fail "neg" "bad --as should fail"
echo "case negative OK"

# 5. Rantai parser (bootstrap 2.0): parse.ns --as ast.
PARSE="$ROOT/selfhost/parse.ns"
"$NUSA_BIN" bootstrap "$PARSE" --as ast -o stage-parse > boot-parse.log 2>&1 || { cat boot-parse.log; fail "bootstrap-parse" "rc != 0"; }
grep -q "Stage 0" boot-parse.log || fail "bootstrap-parse" "missing Stage 0"
grep -q "Stage 1" boot-parse.log || fail "bootstrap-parse" "missing Stage 1"
grep -q "Stage 2" boot-parse.log || fail "bootstrap-parse" "missing Stage 2"
grep -q "PASS" boot-parse.log || fail "bootstrap-parse" "missing PASS"
echo "case bootstrap-parse-pass OK"

# 6. Artefak parse_stage1 mandiri: == `nusa ast` + deterministik.
[ -x stage-parse/parse_stage1 ] || fail "stage1-parse" "not executable"
"$NUSA_BIN" ast tiny.ns > ref-ast.out 2>&1 || fail "ast" "rc != 0"
./stage-parse/parse_stage1 tiny.ns > s1-ast.out 2>&1 || fail "stage1-parse" "run rc != 0"
cmp -s ref-ast.out s1-ast.out || { diff ref-ast.out s1-ast.out; fail "stage1-parse" "output != nusa ast"; }
echo "case stage1-parse-standalone OK"
./stage-parse/parse_stage1 "$PARSE" > pa1.out 2>&1 || fail "det-parse" "run1"
./stage-parse/parse_stage1 "$PARSE" > pa2.out 2>&1 || fail "det-parse" "run2"
cmp -s pa1.out pa2.out || fail "det-parse" "non-deterministic"
echo "case determinism-parse OK"

# 7. Stage 3 lexer: rebuild + banding D2.
"$NUSA_BIN" bootstrap "$LEX" -o stage-re --rebuild stage-re2 > boot-re.log 2>&1 || { cat boot-re.log; fail "rebuild" "rc != 0"; }
grep -q "Stage 3" boot-re.log || fail "rebuild" "missing Stage 3"
grep -q "REBUILD PASS" boot-re.log || fail "rebuild" "missing REBUILD PASS"
[ -x stage-re2/lex_stage1 ] || fail "rebuild" "no stage3 artifact"
echo "case rebuild-lex OK"

# 8. Stage 3 parser: rebuild + banding D2.
"$NUSA_BIN" bootstrap "$PARSE" --as ast -o stage-parse-re --rebuild stage-parse-re2 > boot-parse-re.log 2>&1 || { cat boot-parse-re.log; fail "rebuild-parse" "rc != 0"; }
grep -q "Stage 3" boot-parse-re.log || fail "rebuild-parse" "missing Stage 3"
grep -q "REBUILD PASS" boot-parse-re.log || fail "rebuild-parse" "missing REBUILD PASS"
[ -x stage-parse-re2/parse_stage1 ] || fail "rebuild-parse" "no stage3 artifact"
echo "case rebuild-parse OK"

# 9. Artefak rebuild mandiri (keluar rantai): == Stage 0.
./stage-re2/lex_stage1 "$LEX" > r1.out 2>&1 || fail "rebuild-standalone" "run rc != 0"
"$NUSA_BIN" lex "$LEX" > r0.out 2>&1 || fail "rebuild-standalone" "lex rc != 0"
cmp -s r1.out r0.out || fail "rebuild-standalone" "rebuild output != stage0"
echo "case rebuild-standalone OK"

echo "E2E BOOTSTRAP PASS (10 cases)"
