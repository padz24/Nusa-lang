#!/usr/bin/env bash
# Integration test v1.5.0 (Phase 24): error handling end-to-end.
#  - Option/Result std: konstruktor, kombinator, validasi parse/file.
#  - Propagasi `?`: let/ekspresi/statement + early-return.
#  - Penolakan: non-Result/Option/void/E-beda + builtin misuse.
#  - fmt `?` + parse_int_valid runtime.
set -u
NUSA_BIN="${1:?usage: e2e_error.sh <nusa-binary>}"
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

# 1. Aliran Option + Result + `?` + kombinator (nilai runtime).
cat > v1.ns <<'EOF'
import option;
import result;
import error;
fn parse_age(s: string) -> Result<i64, string> {
    let o = parse_int_opt(s);
    if is_none(o) {
        return Result<i64, string>.Err(str_concat("bad age: ", s));
    }
    match o {
        Some(v) => { return Result<i64, string>.Ok(v); },
        None => { return Result<i64, string>.Err("unreachable"); },
    }
    return Result<i64, string>.Err("unreachable");
}
fn get_age(s: string) -> Result<i64, string> {
    let n = parse_age(s)?;
    return Result<i64, string>.Ok(n + 1);
}
fn double(o: Option<i64>) -> i64 {
    return opt_unwrap_or(o, 0) * 2;
}
fn main() {
    print(res_unwrap_or(get_age("41"), 0));
    print(res_unwrap_or(get_age("abc"), 0));
    print(double(parse_int_opt("21")));
    print(double(parse_int_opt("xx")));
    print(is_some(parse_int_opt("5")));
    print(is_none(parse_int_opt("zz")));
    let r = get_age("7");
    print(res_unwrap_or(r, 0));
}
EOF
printf '42\n0\n42\n0\ntrue\ntrue\n8\n' > v1.want
run_ok v1

# 2. file_read_opt + map combinators.
cat > v2.ns <<'EOF'
import option;
import result;
fn shout(s: string) -> string {
    return str_concat(s, "!");
}
fn ispos(n: i64) -> bool {
    return n > 0;
}
fn main() {
    let o = file_read_opt("/definitely-missing-nusa-test");
    print(is_none(o));
    print(opt_unwrap_or(o, "fallback"));
    let p = parse_int_opt("123");
    let q = opt_map(p, ispos);
    print(opt_unwrap_or(q, false));
    let r: Result<i64, string> = Result<i64, string>.Ok(5);
    let r2 = res_map(r, ispos);
    print(res_unwrap_or(r2, false));
    let e: Result<i64, string> = Result<i64, string>.Err("bad");
    let e2 = res_map_err(e, shout);
    print(res_unwrap_or(e2, 7));
}
EOF
printf 'true\nfallback\ntrue\ntrue\n7\n' > v2.want
run_ok v2

# 3. parse_int_valid runtime.
cat > v3.ns <<'EOF'
fn main() {
    print(parse_int_valid("42"));
    print(parse_int_valid("  -7 "));
    print(parse_int_valid(""));
    print(parse_int_valid("abc"));
    print(parse_int_valid("12x"));
    print(parse_int_valid("0"));
}
EOF
printf 'true\ntrue\nfalse\nfalse\nfalse\ntrue\n' > v3.want
run_ok v3

# 4. Penolakan `?`.
printf 'import option;\nfn main() { let o = some(1); print(o?); }\n' > qopt.ns
expect_fail qopt "cannot propagate 'Option<i64>'"
printf 'import result;\nfn main() { let r: Result<i64, string> = Result<i64, string>.Ok(1); print(r?); }\n' > qvoid.ns
expect_fail qvoid "returning 'void'"
printf 'import result;\nfn f() -> Result<i64, string> { return Result<i64, string>.Ok(1); }\nfn g() -> Result<i64, i64> { let x = f()?; return Result<i64, i64>.Ok(x); }\nfn main() { }\n' > qmismatch.ns
expect_fail qmismatch "in function returning"
printf 'fn main() { let x = 5; print(x?); }\n' > qnonres.ns
expect_fail qnonres "with '?'"
printf 'fn main() { print(parse_int_valid(1)); }\n' > badbuiltin.ns
expect_fail badbuiltin "requires a 'string' argument"

# 5. fmt `?`.
printf 'fn main(){let x = f () ?;print(x ?);}\n' > f.ns
"$NUSA_BIN" fmt f.ns >fmt.log 2>&1 || { cat fmt.log; fail "fmt" "fmt failed"; }
grep -q "let x = f()?;" f.ns || { cat f.ns; fail "fmt" "? misformatted"; }
grep -q "print(x?);" f.ns || { cat f.ns; fail "fmt" "? arg misformatted"; }
pass "fmt-propagate"

echo "ERROR PASS ($PASS_N cases)"
