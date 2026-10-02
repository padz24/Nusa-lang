#!/usr/bin/env bash
# Integration test v1.3.0 (Phase 22): traits end-to-end.
#  - Contoh master: trait + impl + method call (nilai runtime).
#  - Bounds: puas/gagal/multi/struct; method di semua posisi.
#  - Primitif, instance generik, lintas modul, privacy.
#  - Penolakan: konformansi/hilang/asing/duplikat/non-self/unknown/
#    ambigu/generik/bare/bounds/dup-trait.
#  - fmt + doc.
set -u
NUSA_BIN="${1:?usage: e2e_traits.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

run_ok() {
  local name="$1"
  "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "wrong output"; }
  pass "$name"
}

expect_fail() {
  local name="$1" pattern="$2"
  "$NUSA_BIN" check "$name.ns" >"$name.log" 2>&1
  local rc=$?
  [ $rc -ne 0 ] || fail "$name" "should not check (rc=0)"
  [ $rc -le 128 ] || fail "$name" "COMPILER CRASH (rc=$rc)"
  grep -q "$pattern" "$name.log" || { cat "$name.log"; fail "$name" "pattern '$pattern' missing"; }
  pass "$name"
}

# 1. Contoh master verbatim.
cat > v1.ns <<'EOF'
trait Printable {
    fn print(self);
    fn describe(self) -> string;
}
struct Person {
    name: string;
}
impl Printable for Person {
    fn print(self) {
        print(self.name);
    }
    fn describe(self) -> string {
        return self.name;
    }
}
fn main() {
    let p = Person { name: "Nusa" };
    p.print();
    print(p.describe());
}
EOF
printf 'Nusa\nNusa\n' > v1.want
run_ok v1

# 2. Bounds + multi-bounds + struct bounds + primitif + instance generik.
cat > v2.ns <<'EOF'
trait Show { fn show(self) -> string; }
trait Count { fn count(self) -> i64; }
struct A { x: i64; }
struct Box<T: Show> { v: T; }
struct Point<T> { x: T; y: T; }
impl Show for A { fn show(self) -> string { return "A"; } }
impl Count for A { fn count(self) -> i64 { return self.x; } }
impl Show for i64 { fn show(self) -> string { return "num"; } }
impl Show for Point<i64> { fn show(self) -> string { return "pt"; } }
fn f<T: Show>(v: T) -> string { return v.show(); }
fn g<T: Show + Count>(v: T) -> i64 { return v.count(); }
fn main() {
    print(f(A { x: 1 }));
    print(g(A { x: 2 }));
    print(f(41));
    let b: Box<A> = Box { v: A { x: 40 } };
    print(b.v.show());
    let p = Point { x: 1, y: 2 };
    print(p.show());
}
EOF
printf 'A\n2\nnum\nA\npt\n' > v2.want
run_ok v2

# 3. Lintas modul + privacy.
mkdir -p m
cat > m/lib.ns <<'EOF'
trait P { fn ok(self) -> i64; fn _h(self) -> i64; }
struct A { x: i64; }
impl P for A {
    fn ok(self) -> i64 { return 2; }
    fn _h(self) -> i64 { return 1; }
}
EOF
cat > m/main.ns <<'EOF'
import lib;
fn main() { let a = A { x: 0 }; print(a.ok()); }
EOF
(cd m && "$NUSA_BIN" run main.ns >main.got 2>main.err) || { cat m/main.err; fail "mod" "run failed"; }
echo "2" | cmp -s m/main.got - || { cat m/main.got; fail "mod" "wrong output"; }
pass "mod"
printf 'import lib;\nfn main() { let a = A { x: 0 }; print(a._h()); }\n' > m/bad.ns
(cd m && "$NUSA_BIN" check bad.ns >bad.log 2>&1)
[ $? -ne 0 ] || fail "priv" "should not check"
grep -q "is private to module" m/bad.log || { cat m/bad.log; fail "priv" "pattern missing"; }
pass "priv"

# 4. Penolakan.
printf 'trait P { fn m(self) -> i64; }\nstruct A { x: i64; }\nimpl P for A { fn m(self) -> string { return "x"; } }\nfn main() { }\n' > mismatch.ns
expect_fail mismatch "mismatches trait"
printf 'trait P { fn m(self) -> i64; fn n(self) -> i64; }\nstruct A { x: i64; }\nimpl P for A { fn m(self) -> i64 { return 1; } }\nfn main() { }\n' > missing.ns
expect_fail missing "missing method 'n'"
printf 'trait P { fn m(self) -> i64; }\nstruct A { x: i64; }\nimpl P for A { fn z(self) -> i64 { return 1; } }\nfn main() { }\n' > extra.ns
expect_fail extra "is not a member of trait"
printf 'trait P { fn m(self) -> i64; }\nstruct A { x: i64; }\nimpl P for A { fn m(self) -> i64 { return 1; } }\nimpl P for A { fn m(self) -> i64 { return 2; } }\nfn main() { }\n' > dupimpl.ns
expect_fail dupimpl "duplicate impl"
printf 'trait A { fn m(self) -> i64; }\ntrait B { fn m(self) -> i64; }\nstruct P { x: i64; }\nimpl A for P { fn m(self) -> i64 { return 1; } }\nimpl B for P { fn m(self) -> i64 { return 2; } }\nfn main() { let p = P { x: 0 }; print(p.m()); }\n' > ambig.ns
expect_fail ambig "ambiguous method"
printf 'trait P { fn m(self) -> i64; }\nstruct A { x: i64; }\nimpl P for A { fn m(self) -> i64 { return 1; } }\nfn main() { let a = A { x: 1 }; print(a.z()); }\n' > unknown.ns
expect_fail unknown "unknown method 'z'"
printf 'trait Printable { fn show(self) -> string; }\nstruct B { y: i64; }\nfn f<T: Printable>(v: T) -> string { return v.show(); }\nfn main() { print(f(B { y: 1 })); }\n' > boundfail.ns
expect_fail boundfail "does not implement trait"
printf 'struct P<T> { x: T; }\nfn main() { let p: P = P { x: 1 }; }\n' > bare.ns
expect_fail bare "used without type arguments"
printf 'trait P { fn m<T>(self, x: T) -> T; }\nstruct A { x: i64; }\nfn main() { }\n' > genmethod.ns
expect_fail genmethod "generic methods are not supported"
printf 'trait P { fn m(self) -> i64; }\ntrait P { fn n(self) -> i64; }\nfn main() { }\n' > duptrait.ns
expect_fail duptrait "duplicate trait"
printf 'struct A { x: i64; }\nimpl Nope for A { fn m(self) -> i64 { return 1; } }\nfn main() { }\n' > notrait.ns
expect_fail notrait "unknown trait 'Nope'"
printf 'trait P { fn get(self) -> i64; }\nstruct A { x: i64; }\nimpl P for A { fn get(self) -> i64 { return self.x; } }\nfn main() { let p = A { x: 5 }; print(p.get); }\n' > fieldhint.ns
expect_fail fieldhint "did you mean to call the method"

# 5. fmt + doc.
printf 'trait Printable{fn print(self);}\nstruct Person{name:string;}\nimpl Printable for Person{fn print(self){print(self.name);}}\nfn main(){let p=Person{name:"N"};p.print();}\n' > f.ns
"$NUSA_BIN" fmt f.ns >fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
grep -q "trait Printable {" f.ns || { cat f.ns; fail "fmt" "trait misformatted"; }
grep -q "impl Printable for Person {" f.ns || { cat f.ns; fail "fmt" "impl misformatted"; }
grep -q "p.print();" f.ns || { cat f.ns; fail "fmt" "method call misformatted"; }
pass "fmt-traits"
printf '/// Printable things.\ntrait Printable {\n    fn print(self);\n}\nfn main() { }\n' > d.ns
"$NUSA_BIN" doc d.ns -o d.md >doc.log 2>&1 || { cat doc.log; fail "doc" "doc failed"; }
grep -q '### `trait Printable`' d.md || { cat d.md; fail "doc" "trait section missing"; }
pass "doc-traits"

echo "TRAITS PASS ($PASS_N cases)"
