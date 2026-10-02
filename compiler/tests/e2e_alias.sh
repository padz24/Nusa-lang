#!/usr/bin/env bash
# Integration test v1.2.0 (Phase 21a): type aliases end-to-end.
#  - Nilai runtime via alias (i64, array, struct field, param/return).
#  - Forward reference antar-alias.
#  - Penolakan: duplikat, tabrakan struct, target unknown, siklus,
#    konstruksi via alias, aturan array-param tetap berlaku.
#  - fmt + doc mengenali deklarasi alias.
set -u
NUSA_BIN="${1:?usage: e2e_alias.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

# run_ok <name> — run sukses + output cocok dengan <name>.want.
run_ok() {
  local name="$1"
  "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "wrong output"; }
  pass "$name"
}

# expect_fail <name> <pattern> — check gagal bersih + pesan cocok.
expect_fail() {
  local name="$1" pattern="$2"
  "$NUSA_BIN" check "$name.ns" >"$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should not check (rc=0)"
  [ $rc -le 128 ] || fail "$name" "COMPILER CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

# 1. Alias i64: param/return/struct-field + nilai runtime.
cat > v1.ns <<'EOF'
type Answer = i64;
struct P { age: Answer; }
fn id(x: Answer) -> Answer { return x; }
fn main() { let p = P { age: id(40) + 2 }; print(p.age); }
EOF
echo "42" > v1.want
run_ok v1

# 2. Alias array + forward reference antar-alias.
cat > v2.ns <<'EOF'
type B = A;
type A = i64;
type Nums = [B];
fn main() { let a: Nums = [1, 2]; print(a[0] + a[1]); }
EOF
echo "3" > v2.want
run_ok v2

# 3. Penolakan.
printf 'type A = i64;\ntype A = bool;\nfn main() { }\n' > dup.ns
expect_fail dup "duplicate type 'A'"
printf 'struct S { x: i64; }\ntype S = i64;\nfn main() { }\n' > collide.ns
expect_fail collide "duplicate type 'S'"
printf 'type A = Nope;\nfn main() { }\n' > unknown.ns
expect_fail unknown "unknown type 'Nope'"
printf 'type A = A;\nfn main() { }\n' > selfcyc.ns
expect_fail selfcyc "circular type alias 'A'"
printf 'type A = B;\ntype B = A;\nfn main() { }\n' > mutcyc.ns
expect_fail mutcyc "circular chain: A -> B -> A"
printf 'type P2 = P;\nstruct P { x: i64; }\nfn main() { let p = P2 { x: 1 }; }\n' > lit.ns
expect_fail lit "cannot construct type alias 'P2'"
printf 'type Nums = [i64];\nfn f(a: Nums) -> i64 { return 1; }\nfn main() { }\n' > aparam.ns
expect_fail aparam "array parameters are not supported"

# 4. fmt mengenali deklarasi alias.
printf 'type   Nums=[i64];\nfn main(){let a:Nums=[3,1];print(a[0]);}\n' > f.ns
"$NUSA_BIN" fmt f.ns >fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
grep -q "type Nums = \[i64\];" f.ns || { cat f.ns; fail "fmt" "alias decl misformatted"; }
grep -q "let a: Nums = \[3, 1\];" f.ns || { cat f.ns; fail "fmt" "alias use misformatted"; }
pass "fmt-alias"

# 5. doc merender seksi Type Aliases.
printf '/// Numbers list.\ntype Nums = [i64];\nfn main() { }\n' > d.ns
"$NUSA_BIN" doc d.ns -o d.md >doc.log 2>&1 || { cat doc.log; fail "doc" "doc failed"; }
grep -q '### `type Nums = \[i64\]`' d.md || { cat d.md; fail "doc" "alias section missing"; }
pass "doc-alias"

echo "ALIAS PASS ($PASS_N cases)"
