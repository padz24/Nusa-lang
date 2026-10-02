#!/usr/bin/env bash
# Memory-safety conformance v2.5.0 (Phase 54, milestone 1).
# Registry handle fail-fast: misuse SEQUENSIAL (ganda/basi) harus
# exit 1 + pesan `invalid handle` — BUKAN abort/sinyal (rc > 128).
# Program valid harus tetap hijau (join/free tepat sekali).
set -u
NUSA_BIN="${1:?usage: e2e_memsafe.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

expect_invalid_handle() {
  local name="$1"
  timeout 30 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err"
  local rc=$?
  if crashed "$rc"; then cat "$name.err"; fail "$name" "CRASH/SINYAL (rc=$rc), harap exit 1 + pesan"; fi
  [ $rc -eq 1 ] || { cat "$name.err"; fail "$name" "harap exit 1 (rc=$rc)"; }
  grep -q "invalid handle" "$name.err" || { cat "$name.err"; fail "$name" "pesan 'invalid handle' hilang"; }
  pass "$name"
}

run_ok() {
  local name="$1"
  timeout 30 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" || { cat "$name.err"; fail "$name" "run gagal"; }
  cmp -s "$name.got" "$name.want" || { echo "--- got ---"; cat "$name.got"; fail "$name" "output salah"; }
  pass "$name"
}

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1

echo "== misuse sekuensial (fail-fast) =="
cat > double_join.ns <<'EOF'
fn work() -> i64 {
    return 40 + 2;
}
fn main() {
    let h = thread_spawn(work);
    print(thread_join(h));
    print(thread_join(h));
}
EOF
expect_invalid_handle double_join

cat > send_after_free.ns <<'EOF'
fn main() {
    let c = chan_new();
    chan_free(c);
    chan_send(c, 1);
}
EOF
expect_invalid_handle send_after_free

cat > double_mutex_free.ns <<'EOF'
fn main() {
    let m = mutex_new();
    mutex_lock(m);
    mutex_unlock(m);
    mutex_free(m);
    mutex_free(m);
}
EOF
expect_invalid_handle double_mutex_free

cat > atomic_after_free.ns <<'EOF'
fn main() {
    let a = atomic_new(1);
    atomic_free(a);
    print(atomic_get(a));
}
EOF
expect_invalid_handle atomic_after_free

cat > lock_after_free.ns <<'EOF'
fn main() {
    let m = mutex_new();
    mutex_free(m);
    mutex_lock(m);
}
EOF
expect_invalid_handle lock_after_free

echo "== program valid (tetap hijau) =="
cat > valid_all.ns <<'EOF'
fn work() -> i64 {
    return 21;
}
fn main() {
    let h = thread_spawn(work);
    print(thread_join(h));
    let c = chan_new();
    chan_send(c, 7);
    print(chan_recv(c));
    chan_free(c);
    let m = mutex_new();
    mutex_lock(m);
    mutex_unlock(m);
    mutex_free(m);
    let a = atomic_new(3);
    print(atomic_add(a, 4));
    atomic_free(a);
}
EOF
printf '21\n7\n7\n' > valid_all.want
run_ok valid_all

# Phase 115 (3.16.0), diperbarui Phase 123: cakupan OOM — tiap
# alokasi heap di IR (`nusa_alloc`; pra-3.24: `malloc` langsung)
# wajib punya satu definisi cek `%oom.null* = icmp` (emitOomCheck
# fail-closed). Ketidakseimbangan = situs alokasi baru tanpa pelindung.
cat > oomcov.ns <<'EOF'
fn apply(f: fn(i64) -> i64, x: i64) -> i64 {
    return f(x);
}
fn main() {
    let c = |x: i64| x * 2;
    print(apply(c, 21));
    print(str_concat("a", "b"));
    print(int_to_str(7));
}
EOF
timeout 60 "$NUSA_BIN" build oomcov.ns --emit-ir -o oomcov.ll >oomcov.log 2>&1 \
  || { cat oomcov.log; fail "oom-coverage" "build gagal"; }
NMALLOC=$(grep -cE 'call ptr @(nusa_alloc|malloc)' oomcov.ll)
NOOM=$(grep -cE '%oom\.null[0-9]* = icmp' oomcov.ll)
[ "$NMALLOC" -gt 0 ] || fail "oom-coverage" "tanpa alokasi (uji tumpul)"
[ "$NMALLOC" -eq "$NOOM" ] || fail "oom-coverage" "malloc=$NMALLOC tak sama oom-check=$NOOM"
pass "oom-coverage"

echo "E2E MEMSAFE PASS ($PASS_N cases)"
