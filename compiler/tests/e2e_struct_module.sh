#!/usr/bin/env bash
# Integration test v0.8.0: struct end-to-end + module system
# (resolution, namespace merge, visibility, circular).
set -u
NUSA_BIN="${1:?usage: e2e_struct_module.sh <nusa-binary>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

run_case() {
  local name="$1" expected="$2"
  timeout 10 "$NUSA_BIN" run "$name.ns" > "$name.got" 2>"$name.log" || {
    echo "E2E FAIL [$name]: exit $?"; cat "$name.log"; exit 1;
  }
  if ! diff <(printf '%s\n' "$expected") "$name.got" > /dev/null; then
    echo "E2E FAIL [$name]:"; echo "--- expected ---"; printf '%s\n' "$expected"
    echo "--- got ---"; cat "$name.got"; exit 1
  fi
  echo "case $name OK"
}

fail_case() {
  local name="$1" pattern="$2" src="$3"
  printf '%s' "$src" > "$name.ns"
  if "$NUSA_BIN" build "$name.ns" -o "$name.out" 2>"$name.log"; then
    echo "E2E FAIL [$name]: should not build"; exit 1
  fi
  grep -q "$pattern" "$name.log" || { echo "E2E FAIL [$name]:"; cat "$name.log"; exit 1; }
  echo "case $name OK"
}

# 1. Struct milestone (juga di examples/)
cp "$ROOT/examples/person.ns" ./person.ns
timeout 10 "$NUSA_BIN" run person.ns > person.got 2>person.log || { echo "E2E FAIL [person]"; cat person.log; exit 1; }
printf 'Nusa\n1\n' | diff - person.got || { echo "E2E FAIL [person]: got:"; cat person.got; exit 1; }
grep -q "%Person = type { ptr, i64 }" person.ns.ll || { echo "E2E FAIL [person IR]"; exit 1; }
echo "case person OK"

# 2. Modul milestone (juga di examples/): main.ns + math.ns
mkdir -p proj && cp "$ROOT/examples/main.ns" proj/main.ns && cp "$ROOT/examples/math.ns" proj/math.ns
(cd proj && timeout 10 "$NUSA_BIN" run main.ns > ../main.got 2>../main.log) || { echo "E2E FAIL [modules]"; cat main.log; exit 1; }
printf '6\nAda\n' | diff - main.got || { echo "E2E FAIL [modules]: got:"; cat main.got; exit 1; }
echo "case modules OK"

# 3. Nested struct runtime
cat > nested.ns <<'EOF'
struct Inner { v: i64; }
struct Outer { inner: Inner; n: i64; }

fn main() {
    let o = Outer { inner: Inner { v: 7 }, n: 1 };
    print(o.inner.v);
    print(o.n);
}
EOF
run_case nested "$(printf '7\n1')"

# 4. Struct + fungsi lintas modul
cat > lib.ns <<'EOF'
struct Point { x: i64; y: i64; }

fn origin() -> i64 {
    return 0;
}
EOF
cat > uselib.ns <<'EOF'
import lib;

fn main() {
    let p = Point { x: 3, y: 4 };
    print(p.x + p.y + origin());
}
EOF
run_case uselib "7"

# 5. Privacy: _secret antar-modul ditolak
cat > seclib.ns <<'EOF'
fn _secret() -> i64 {
    return 1;
}
EOF
cat > secuse.ns <<'EOF'
import seclib;

fn main() {
    print(_secret());
}
EOF
if "$NUSA_BIN" build secuse.ns -o secuse.out 2>secuse.log; then echo "E2E FAIL [privacy]"; exit 1; fi
grep -q "is private to module" secuse.log || { echo "E2E FAIL [privacy]:"; cat secuse.log; exit 1; }
echo "case privacy OK"

# 6. Circular import
printf 'import circ_b;\nfn main() { }\n' > circ_a.ns
printf 'import circ_a;\nfn main() { }\n' > circ_b.ns
if "$NUSA_BIN" build circ_a.ns -o circ_a.out 2>circ_a.log; then echo "E2E FAIL [circular]"; exit 1; fi
grep -q "circular import detected" circ_a.log || { echo "E2E FAIL [circular]:"; cat circ_a.log; exit 1; }
echo "case circular OK"

# 7. Import hilang
printf 'import nosuchmod;\nfn main() { }\n' > missing.ns
if "$NUSA_BIN" build missing.ns -o missing.out 2>missing.log; then echo "E2E FAIL [missing]"; exit 1; fi
grep -q "cannot read module file" missing.log || { echo "E2E FAIL [missing]:"; cat missing.log; exit 1; }
echo "case missing OK"

# 8. Duplikat struct
fail_case dupstruct "duplicate struct 'P'" 'struct P { x: i64; }
struct P { y: i64; }
fn main() { }
'

# 9. Tipe field salah
fail_case fieldtype "mismatched field type" 'struct P { age: i64; }
fn main() { let p = P { age: "x" }; }
'

# 10. Struct by-value sebagai parameter + return (Phase 21d).
printf 'struct P { x: i64; }\nfn f(p: P) -> i64 { return p.x; }\nfn mk(v: i64) -> P { return P { x: v }; }\nfn main() { print(f(mk(40)) + 2); }\n' > structbyval.ns
run_case structbyval "$(printf '42\n')"

echo "E2E STRUCT+MODULE PASS"
