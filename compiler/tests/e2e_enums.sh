#!/usr/bin/env bash
# Integration test v1.7.0 (Phase 26): enums end-to-end.
#  - Deklarasi, ctor qualified, pola, exhaustiveness (nilai runtime).
#  - Generic enums: inferensi + targs eksplisit.
#  - Penolakan: arity/unknown/mismatch/non-exhaustive/forge/print.
#  - fmt + doc enums.
set -u
NUSA_BIN="${1:?usage: e2e_enums.sh <nusa-binary>}"
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

# 1. Unit variants + bare patterns (nilai runtime).
cat > v1.ns <<'EOF'
enum Color { Red, Green, Blue }
fn code(c: Color) -> i64 {
    match c {
        Red => { return 1; },
        Green => { return 2; },
        Blue => { return 3; },
    }
    return 0;
}
fn main() {
    print(code(Color.Red));
    print(code(Color.Green));
    print(code(Color.Blue));
}
EOF
printf '1\n2\n3\n' > v1.want
run_ok v1

# 2. Payload posisional + destruktur (nilai runtime).
cat > v2.ns <<'EOF'
enum Shape { Circle(i64), Rect(i64, i64), Dot }
fn tag(s: Shape) -> i64 {
    match s {
        Circle(r) => { return r; },
        Rect(w, h) => { return w + h; },
        Dot => { return 0 - 1; },
    }
    return 0;
}
fn main() {
    print(tag(Shape.Circle(5)));
    print(tag(Shape.Rect(3, 4)));
    print(tag(Shape.Dot));
}
EOF
printf '5\n7\n-1\n' > v2.want
run_ok v2

# 3. Generic: inferensi + eksplisit + multi-tparam (nilai runtime).
cat > v3.ns <<'EOF'
enum Wrap<T> { Val(T), Empty }
enum Pair<T, E> { Both(T, E), First(T) }
fn get(w: Wrap<i64>) -> i64 {
    match w {
        Val(v) => { return v; },
        Empty => { return 0 - 1; },
    }
    return 0;
}
fn sum(p: Pair<i64, string>) -> i64 {
    match p {
        Both(a, b) => { return a; },
        First(a) => { return a; },
    }
    return 0;
}
fn main() {
    print(get(Wrap.Val(7)));
    print(get(Wrap<i64>.Empty));
    print(sum(Pair.Both(3, "x")));
    print(sum(Pair<i64, string>.First(4)));
}
EOF
printf '7\n-1\n3\n4\n' > v3.want
run_ok v3

# 4. Guard + nested + wildcard di atas enum (nilai runtime).
cat > v4.ns <<'EOF'
enum Shape { Circle(i64), Rect(i64, i64), Dot }
fn big(s: Shape) -> i64 {
    match s {
        Circle(r) if r > 10 => { return 1; },
        Circle(r) => { return 0; },
        _ => { return 0 - 1; },
    }
    return 0;
}
fn main() {
    print(big(Shape.Circle(20)));
    print(big(Shape.Circle(5)));
    print(big(Shape.Dot));
}
EOF
printf '1\n0\n-1\n' > v4.want
run_ok v4

# 5. Enum generik di field struct + payload string (nilai runtime).
cat > v6.ns <<'EOF'
enum Wrap<T> { Val(T), Empty }
struct Box { w: Wrap<i64>; }
fn unbox(b: Box) -> i64 {
    match b.w {
        Val(v) => { return v; },
        Empty => { return 0 - 1; },
    }
    return 0;
}
fn greet(o: Wrap<string>) -> string {
    match o {
        Val(s) => { return s; },
        Empty => { return "none"; },
    }
    return "?";
}
fn main() {
    print(unbox(Box { w: Wrap.Val(11) }));
    print(greet(Wrap.Val("hi")));
    print(greet(Wrap<string>.Empty));
}
EOF
printf '11\nhi\nnone\n' > v6.want
run_ok v6

# 6. Metode trait atas enum (dispatch statis gratis via hidden struct).
cat > v5.ns <<'EOF'
trait Show { fn show(self) -> i64; }
enum Color { Red, Green }
impl Show for Color {
    fn show(self) -> i64 {
        match self {
            Red => { return 1; },
            Green => { return 2; },
        }
        return 0;
    }
}
fn main() {
    print(Color.Red.show());
    print(Color.Green.show());
}
EOF
printf '1\n2\n' > v5.want
run_ok v5

# 6. Penolakan: arity ctor.
cat > e1.ns <<'EOF'
enum Shape { Circle(i64), Dot }
fn main() {
    let s: Shape = Shape.Circle(1, 2);
}
EOF
expect_fail e1 "expects 1 argument(s)"

# 7. Penolakan: varian tak dikenal (ctor).
cat > e2.ns <<'EOF'
enum Color { Red, Green }
fn main() {
    let c: Color = Color.Yellow;
}
EOF
expect_fail e2 "unknown variant 'Yellow'"

# 8. Penolakan: varian tak dikenal (pola).
cat > e3.ns <<'EOF'
enum Color { Red, Green }
fn f(c: Color) -> i64 {
    match c {
        Red => { return 1; },
        Yellow => { return 2; },
        _ => { return 3; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e3 "unknown variant 'Yellow'"

# 9. Penolakan: non-exhaustive menyebut varian hilang.
cat > e4.ns <<'EOF'
enum Color { Red, Green, Blue }
fn f(c: Color) -> i64 {
    match c {
        Red => { return 1; },
        Green => { return 2; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e4 "missing pattern(s) for 'Blue'"

# 10. Penolakan: payload mismatch.
cat > e5.ns <<'EOF'
enum Shape { Circle(i64), Dot }
fn main() {
    let s: Shape = Shape.Circle("x");
}
EOF
expect_fail e5 "mismatched variant payload"

# 11. Penolakan: inferensi gagal mengarahkan ke eksplisit.
cat > e6.ns <<'EOF'
enum Wrap<T> { Val(T), Empty }
fn get(w: Wrap<i64>) -> i64 {
    match w {
        Val(v) => { return v; },
        Empty => { return 0 - 1; },
    }
    return 0;
}
fn main() {
    print(get(Wrap.Empty));
}
EOF
expect_fail e6 "cannot infer type argument 'T'"

# 12. Penolakan: tempa struct + print enum.
cat > e7.ns <<'EOF'
enum Color { Red, Green }
fn main() {
    let c: Color = Color { tag: 0 };
}
EOF
expect_fail e7 "cannot construct enum 'Color' as struct"

cat > e8.ns <<'EOF'
enum Color { Red, Green }
fn main() {
    print(Color.Red);
}
EOF
expect_fail e8 "cannot print value of type 'Color'"

# 13. fmt enum idempoten + doc enums.
cat > messy.ns <<'EOF'
enum Color { Red, Green }
fn main() {
    let c: Color = Color.Red;
}
EOF
"$NUSA_BIN" fmt messy.ns > fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
"$NUSA_BIN" fmt --check messy.ns > /dev/null 2>&1 || fail "fmt" "not stable after fmt"
pass "fmt"
"$NUSA_BIN" doc messy.ns > doc.log 2>&1 || { cat doc.log; fail "doc" "doc failed"; }
grep -q "## Enums" doc.log || { cat doc.log; fail "doc" "'## Enums' missing"; }
grep -q "enum Color" doc.log || { cat doc.log; fail "doc" "'enum Color' missing"; }
pass "doc"

echo "E2E ENUMS PASS ($PASS_N cases)"
