#!/usr/bin/env bash
# Integration test v2.28.0 (Phase 77): data tooling.
#  - Serialisasi murni-Nusa: csv/json/toml (`nusa test` + golden).
#  - SQLite via paket ekosistem (guard libsqlite3): alur pkg
#    (init/add-path/update/build) + CRUD :memory: + negatif.
set -u
NUSA_BIN="${1:?usage: e2e_data.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }

run_ok() {
  local name="$1"
  timeout 60 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run failed"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "wrong output"; }
  pass "$name"
}

# 1. `nusa test` atas modul serialisasi.
for t in csv_test json_test toml_test; do
  timeout 60 "$NUSA_BIN" test "$ROOT/tests/$t.ns" >"$t.tlog" 2>&1 || { cat "$t.tlog"; fail "test-$t" "nusa test rc!=0"; }
  pass "test-$t"
done

# 2. Golden CSV (quote, koma-dalam-field, CRLF).
cat > d_csv.ns <<'EOF'
import csv;
fn main() {
    let t = "a,b\r\n\"x,x\",2\r\nplain,\"q\"\"q\"";
    print(csv_num_rows(t));
    print(csv_row(t, 1));
    print(csv_field(csv_row(t, 1), 0));
    print(csv_field(csv_row(t, 2), 1));
    print(csv_num_fields(csv_row(t, 0)));
}
EOF
printf '3\n"x,x",2\nx,x\nq"q\n2\n' > d_csv.want
run_ok d_csv

# 3. Golden JSON (nested-skip, escape, tipe-salah).
cat > d_json.ns <<'EOF'
import json;
fn main() {
    let o = "{\"a\": {\"k\": 1}, \"k\": 2, \"s\": \"a\\\"b\", \"t\": true}";
    print(json_get_int(o, "k"));
    print(json_get_string(o, "s"));
    print(json_get_bool(o, "t"));
    print(json_get_int(o, "s"));
    print(json_has_key(o, "zz"));
    print(json_valid("{\"a\": [1, 2]}"));
    print(json_valid("{\"a\": "));
}
EOF
printf '2\na"b\ntrue\n0\nfalse\ntrue\nfalse\n' > d_json.want
run_ok d_json

# 4. Golden TOML (section, komentar, inline).
cat > d_toml.ns <<'EOF'
import toml;
fn main() {
    let d = "# top\ntitle = \"Hi\" # inline\nn = 41 # c\non = false\n[db]\nhost = \"h\"\nport = 5432\n";
    print(toml_get_string(d, "", "title"));
    print(toml_get_int(d, "", "n"));
    print(toml_get_bool(d, "", "on"));
    print(toml_get_string(d, "db", "host"));
    print(toml_get_int(d, "db", "port"));
    print(toml_get_int(d, "db", "n"));
}
EOF
printf 'Hi\n41\nfalse\nh\n5432\n0\n' > d_toml.want
run_ok d_toml

# 5. SQLite via paket (guard: compiler + header + lib).
if ! command -v cc >/dev/null 2>&1; then
  echo "SKIP-sqlite: no cc"
elif [ ! -f /usr/include/sqlite3.h ] && [ ! -f "$ROOT/packages/sqlite/shim.c" ]; then
  echo "SKIP-sqlite: no sqlite3"
elif ! echo 'int main(){return 0;}' | cc -x c - -lsqlite3 -o /tmp/nusa_sqlite_probe 2>/dev/null; then
  echo "SKIP-sqlite: cannot link libsqlite3"
  rm -f /tmp/nusa_sqlite_probe
else
  rm -f /tmp/nusa_sqlite_probe
  cp -r "$ROOT/packages/sqlite" sqlite-pkg
  cc -c sqlite-pkg/shim.c -o sqlite-pkg/shim.o || fail "sqlite-cc" "shim build failed"
  "$NUSA_BIN" pkg init sqlapp --dir app >pkg.log 2>&1 || { cat pkg.log; fail "sqlite-init" "pkg init failed"; }
  cp sqlite-pkg/shim.o app/shim.o
  cd app || exit 1
  "$NUSA_BIN" pkg add sqlite --path ../sqlite-pkg >pkg.log 2>&1 || { cat pkg.log; fail "sqlite-add" "pkg add failed"; }
  "$NUSA_BIN" pkg update >pkg.log 2>&1 || { cat pkg.log; fail "sqlite-update" "pkg update failed"; }
  cat >> nusa.toml <<'EOF'

[link]
files = ["shim.o"]
libraries = ["sqlite3"]

[capabilities]
native = true
EOF
  cat > main.ns <<'EOF'
import sqlite;
import csv;
fn main() {
    let db = db_open(":memory:");
    print(db_exec(db, "CREATE TABLE t(id INTEGER, name TEXT)"));
    print(db_exec(db, "INSERT INTO t VALUES (1, 'nusa')"));
    print(db_exec(db, "INSERT INTO t VALUES (2, 'a,b')"));
    print(db_query(db, "SELECT * FROM t ORDER BY id", "out.csv"));
    let c = file_read("out.csv");
    print(csv_num_rows(c));
    print(csv_field(csv_row(c, 0), 1));
    print(csv_field(csv_row(c, 2), 1));
    print(db_exec(db, "NOT SQL AT ALL"));
    print(db_query(db, "SELECT * FROM nosuchtable", "bad.csv"));
    print(db_open("/nonexistent-dir-xyz/db.sqlite"));
    print(db_close(db));
}
EOF
  "$NUSA_BIN" pkg build >pkg.log 2>&1 || { cat pkg.log; fail "sqlite-build" "pkg build failed"; }
  [ -x target/sqlapp ] || fail "sqlite-build" "target binary missing"
  ./target/sqlapp > app.got 2>app.err || { cat app.err; fail "sqlite-run" "run failed"; }
  printf '0\n1\n1\n2\n3\nname\na,b\n-1\n-1\n-1\n0\n' | diff - app.got || fail "sqlite-run" "wrong output"
  pass "sqlite-pkg"
  cd "$TMPDIR" || exit 1
fi

echo "E2E DATA PASS ($PASS_N cases)"
