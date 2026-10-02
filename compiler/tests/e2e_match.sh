#!/usr/bin/env bash
# Integration test v1.6.0 (Phase 25): pattern matching end-to-end.
#  - Literal/bind/wildcard/struct/guard/Option/Result (nilai runtime).
#  - Penolakan: non-exhaustive/unreachable/dup/mismatch/guard-nonbool.
#  - fmt match (arm commas + idempoten).
set -u
NUSA_BIN="${1:?usage: e2e_match.sh <nusa-binary>}"
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

# 1. Literal int + wildcard + bind (nilai runtime).
cat > v1.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        1 => { return 10; },
        2 => { return 20; },
        _ => { return 30; },
    }
    return 0;
}
fn g(x: i64) -> i64 {
    match x {
        n => { return n; },
    }
    return 0;
}
fn main() {
    print(f(1));
    print(f(2));
    print(f(9));
    print(g(42));
}
EOF
printf '10\n20\n30\n42\n' > v1.want
run_ok v1

# 2. Bool/string/guard (nilai runtime).
cat > v2.ns <<'EOF'
fn bs(b: bool) -> i64 {
    match b {
        true => { return 1; },
        false => { return 0; },
    }
    return 9;
}
fn ss(s: string) -> i64 {
    match s {
        "a" => { return 1; },
        _ => { return 2; },
    }
    return 9;
}
fn g(x: i64) -> i64 {
    match x {
        n if n > 10 => { return 1; },
        _ => { return 0; },
    }
    return 9;
}
fn main() {
    print(bs(true));
    print(bs(false));
    print(ss("a"));
    print(ss("z"));
    print(g(20));
    print(g(5));
}
EOF
printf '1\n0\n1\n2\n1\n0\n' > v2.want
run_ok v2

# 3. Struct pattern + binding field (nilai runtime).
cat > v3.ns <<'EOF'
struct Point { x: i64; y: i64; }
fn q(p: Point) -> i64 {
    match p {
        Point { x: 0, y } => { return y; },
        Point { x, y: 0 } => { return x; },
        _ => { return 99; },
    }
    return 7;
}
fn main() {
    print(q(Point { x: 0, y: 5 }));
    print(q(Point { x: 3, y: 0 }));
    print(q(Point { x: 3, y: 4 }));
}
EOF
printf '5\n3\n99\n' > v3.want
run_ok v3

# 4. Option/Result std (nilai runtime).
cat > v4.ns <<'EOF'
import option;
import result;
fn descr(o: Option<i64>) -> i64 {
    match o {
        Some(v) => { return v; },
        None => { return 0 - 1; },
    }
    return 0;
}
fn rlen(r: Result<i64, string>) -> i64 {
    match r {
        Ok(v) => { return v; },
        Err(_) => { return 0 - 9; },
    }
    return 0;
}
fn main() {
    print(descr(some(41)));
    print(descr(Option<i64>.None()));
    print(rlen(Result<i64, string>.Ok(7)));
    print(rlen(Result<i64, string>.Err("boom")));
}
EOF
printf '41\n-1\n7\n-9\n' > v4.want
run_ok v4

# 5. Single-statement arm tanpa blok (nilai runtime).
cat > v5.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        0 => return 1;,
        _ => return 2;,
    }
    return 0;
}
fn main() {
    print(f(0));
    print(f(5));
}
EOF
printf '1\n2\n' > v5.want
run_ok v5

# 6. Penolakan: non-exhaustive.
cat > e1.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        1 => { return 1; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e1 "non-exhaustive match"

# 7. Penolakan: unreachable sesudah wildcard.
cat > e2.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        _ => { return 1; },
        2 => { return 2; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e2 "unreachable pattern"

# 8. Penolakan: literal ganda.
cat > e3.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        1 => { return 1; },
        1 => { return 2; },
        _ => { return 3; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e3 "unreachable pattern"

# 9. Penolakan: bool tak lengkap menyebut sisi hilang.
cat > e4.ns <<'EOF'
fn f(b: bool) -> i64 {
    match b {
        true => { return 1; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e4 "missing pattern for 'false'"

# 10. Penolakan: pola vs tipe subject.
cat > e5.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        "a" => { return 1; },
        _ => { return 2; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e5 "invalid pattern"

# 11. Penolakan: guard non-bool.
cat > e6.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        n if n => { return 1; },
        _ => { return 2; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e6 "must be of type 'bool'"

# 12. Penolakan: varian tak dikenal.
cat > e7.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        Red(1) => { return 1; },
        _ => { return 2; },
    }
    return 0;
}
fn main() {
}
EOF
expect_fail e7 "cannot match variant 'Red'"

# 13. fmt match: koma arm + idempoten.
cat > messy.ns <<'EOF'
fn f(x: i64) -> i64 {
    match x {
        1 => { return 10; },
        _ => { return 30; },
    }
    return 0;
}
fn main() {
}
EOF
"$NUSA_BIN" fmt messy.ns > fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
"$NUSA_BIN" fmt --check messy.ns > /dev/null 2>&1 || fail "fmt" "not stable after fmt"
grep -q "1 => {" messy.ns || fail "fmt" "arm body moved"
grep -q "        }," messy.ns || fail "fmt" "arm comma not attached"
pass "fmt"

echo "E2E MATCH PASS ($PASS_N cases)"
