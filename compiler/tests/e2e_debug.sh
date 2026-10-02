#!/usr/bin/env bash
# Integration test v1.18.0 (Phase 37): debug info DWARF end-to-end.
#  - IR: metadata CU/SP/variabel/lokasi + flag versi (tanpa debugger).
#  - Biner: llvm-dwarfdump menemukan subprogram/variabel/line-table.
#  - Tanpa -g: tak ada DWARF. (lldb/gdb tak tersedia di CI ini;
#    pemakaian interaktif didokumentasikan di docs/debugging.md.)
set -u
NUSA_BIN="${1:?usage: e2e_debug.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

printf 'fn add(a: i64, b: i64) -> i64 {\n    let c = a + b;\n    return c;\n}\nfn main() {\n    let x = 40;\n    print(add(x, 2));\n}\n' > dbg.ns

# 1. IR ber-debug: CU/SP/variabel/lokasi/flag.
"$NUSA_BIN" build dbg.ns --emit-ir -o dbg.ll -g > ir.log 2>&1 || { cat ir.log; fail "ir" "build failed"; }
grep -q "DICompileUnit" dbg.ll || fail "ir" "no CU"
grep -q 'DISubprogram(name: "add"' dbg.ll || fail "ir" "no SP add"
grep -q 'DISubprogram(name: "main"' dbg.ll || fail "ir" "no SP main"
grep -q 'DILocalVariable(name: "c"' dbg.ll || fail "ir" "no var c"
grep -q 'DILocalVariable(name: "x"' dbg.ll || fail "ir" "no var x"
grep -q "dbg_declare" dbg.ll || fail "ir" "no declares"
grep -q "Debug Info Version" dbg.ll || fail "ir" "no version flag"
pass "ir"

# 2. Biner: DWARF utuh menurut llvm-dwarfdump.
"$NUSA_BIN" build dbg.ns -o dbg -g > b.log 2>&1 || { cat b.log; fail "build" "build failed"; }
[ "$(./dbg)" = "42" ] || fail "build" "wrong output"
llvm-dwarfdump --debug-info dbg > dwarf.log 2>&1 || fail "dwarf" "dwarfdump failed"
grep -q 'DW_TAG_subprogram' dwarf.log || fail "dwarf" "no subprogram DIE"
grep -q 'DW_AT_name.*"add"' dwarf.log || fail "dwarf" "no add DIE"
grep -q 'DW_AT_name.*"main"' dwarf.log || fail "dwarf" "no main DIE"
grep -q 'DW_TAG_variable' dwarf.log || fail "dwarf" "no variable DIE"
grep -q 'DW_AT_name.*"c"' dwarf.log || fail "dwarf" "no var c DIE"
llvm-dwarfdump --debug-line dbg > line.log 2>&1 || fail "line" "dwarfdump failed"
grep -q "dbg.ns" line.log || fail "line" "no source file"
pass "dwarf"

# 3. Tanpa -g: bersih dari DWARF.
"$NUSA_BIN" build dbg.ns -o dbg0 > b0.log 2>&1 || { cat b0.log; fail "nodebug" "build failed"; }
llvm-dwarfdump --debug-info dbg0 > dwarf0.log 2>&1 || fail "nodebug" "dwarfdump failed"
grep -q "DW_TAG_subprogram" dwarf0.log && fail "nodebug" "unexpected DWARF"
pass "nodebug"

# 4. -g + -O2 tetap valid (lokasi boleh meleset — yang penting lolos verify).
"$NUSA_BIN" build dbg.ns -o dbgO -g -O2 > bo.log 2>&1 || { cat bo.log; fail "opt" "build failed"; }
[ "$(./dbgO)" = "42" ] || fail "opt" "wrong output"
pass "opt"

# 5. DWARF composite v2.15.0 (Phase 64): struct + member, formal
# parameter, array bound — di IR dan biner.
printf 'struct Person {\n    name: string;\n    age: i64;\n}\nfn greet(p: Person, n: i64) -> i64 {\n    print(p.age);\n    return n + p.age;\n}\nfn main() {\n    let p = Person { name: "Nusa", age: 30 };\n    let a: [i64] = [1, 2, 3];\n    print(greet(p, a[0]));\n}\n' > dbg2.ns
"$NUSA_BIN" build dbg2.ns --emit-ir -o dbg2.ll -g > ir2.log 2>&1 || { cat ir2.log; fail "composite-ir" "build failed"; }
grep -q "DW_TAG_structure_type" dbg2.ll || fail "composite-ir" "no struct type"
grep -q "DW_TAG_member" dbg2.ll || fail "composite-ir" "no member"
grep -q 'DILocalVariable(name: "p", arg: 1' dbg2.ll || fail "composite-ir" "no formal param"
grep -q "DW_TAG_array_type" dbg2.ll || fail "composite-ir" "no array type"
grep -q "DISubrange" dbg2.ll || fail "composite-ir" "no subrange"
pass "composite-ir"
"$NUSA_BIN" build dbg2.ns -o dbg2 -g > b2.log 2>&1 || { cat b2.log; fail "composite" "build failed"; }
[ "$(./dbg2)" = "30
31" ] || fail "composite" "wrong output"
llvm-dwarfdump --debug-info dbg2 > dwarf2.log 2>&1 || fail "composite" "dwarfdump failed"
grep -q 'DW_TAG_structure_type' dwarf2.log || fail "composite" "no struct DIE"
grep -q 'DW_AT_name.*"Person"' dwarf2.log || fail "composite" "no Person DIE"
grep -q 'DW_AT_name.*"age"' dwarf2.log || fail "composite" "no member DIE"
grep -q 'DW_TAG_formal_parameter' dwarf2.log || fail "composite" "no param DIE"
grep -q 'DW_TAG_variable' dwarf2.log || fail "composite" "no variable DIE"
grep -q 'DW_TAG_array_type' dwarf2.log || fail "composite" "no array DIE"
pass "composite"

echo "E2E DEBUG PASS ($PASS_N cases)"
