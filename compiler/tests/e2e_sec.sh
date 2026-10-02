#!/usr/bin/env bash
# Security conformance v2.10.0 (Phase 59, milestone 1).
# - `[capabilities]`: extern/[link] tanpa native=true ditolak pkg build;
#   dengan native=true build; ringkasan tercetak.
# - Traversal guard: tarball registry berisi `../` ditolak bersih
#   tanpa file tertanam di luar.
set -u
NUSA_BIN="${1:?usage: e2e_sec.sh <nusa-binary>}"
NUSA_BIN_ABS="$(cd "$(dirname "$NUSA_BIN")" && pwd)/$(basename "$NUSA_BIN")"
NUSA_BIN="$NUSA_BIN_ABS"

PASS_N=0
fail() { echo "E2E FAIL [$1]: $2"; exit 1; }
pass() { echo "case $1 OK"; PASS_N=$((PASS_N + 1)); }
crashed() { [ "$1" -gt 128 ] 2>/dev/null; }

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT
cd "$TMPDIR" || exit 1
# Hermetik Phase 80: cache registry per-suite (lihat e2e_eco2.sh).
export HOME="$TMPDIR/home"
mkdir -p "$HOME"

echo "== capability native (extern) =="
mkdir -p secnat && cd secnat || exit 1
cat > nusa.toml <<'EOF'
[package]
name = "secnat"
version = "0.1.0"
compiler = "2.10.0"
EOF
cat > main.ns <<'EOF'
extern fn strlen(s: string) -> i64;
fn main() {
    print(strlen("abcd"));
}
EOF
"$NUSA_BIN" pkg build >build1.log 2>&1
rc=$?
if crashed "$rc"; then cat build1.log; fail "sec-extern-denied" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "sec-extern-denied" "extern tanpa cap harus ditolak"
grep -q "native" build1.log || { cat build1.log; fail "sec-extern-denied" "pesan native hilang"; }
pass "sec-extern-denied"
cat >> nusa.toml <<'EOF'

[capabilities]
native = true
EOF
"$NUSA_BIN" pkg build >build2.log 2>&1 || { cat build2.log; fail "sec-extern-allowed" "dengan native=true harus build"; }
grep -q "capabilities: native" build2.log || { cat build2.log; fail "sec-extern-allowed" "ringkasan hilang"; }
[ "$(./target/secnat)" = "4" ] || fail "sec-extern-allowed" "run salah"
pass "sec-extern-allowed"
cd "$TMPDIR" || exit 1

echo "== capability native ([link]) =="
mkdir -p seclink && cd seclink || exit 1
cat > nusa.toml <<'EOF'
[package]
name = "seclink"
version = "0.1.0"
compiler = "2.10.0"

[link]
libraries = ["m"]
EOF
printf 'fn main() {\n    print(1);\n}\n' > main.ns
"$NUSA_BIN" pkg build >build3.log 2>&1
rc=$?
if crashed "$rc"; then cat build3.log; fail "sec-link-denied" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "sec-link-denied" "[link] tanpa cap harus ditolak"
grep -q "native" build3.log || { cat build3.log; fail "sec-link-denied" "pesan native hilang"; }
pass "sec-link-denied"
cd "$TMPDIR" || exit 1

echo "== paket bersih (ringkasan none) =="
mkdir -p secclean && cd secclean || exit 1
"$NUSA_BIN" pkg init secclean --dir . >/dev/null 2>&1 || fail "sec-clean" "init gagal"
grep -q "\[capabilities\]" nusa.toml || fail "sec-clean" "template tanpa [capabilities]"
"$NUSA_BIN" pkg build >build4.log 2>&1 || { cat build4.log; fail "sec-clean" "paket bersih harus build"; }
grep -q "capabilities: (none)" build4.log || { cat build4.log; fail "sec-clean" "ringkasan (none) hilang"; }
pass "sec-clean"
cd "$TMPDIR" || exit 1

echo "== adversarial input tanpa-crash (Phase 87) =="
printf 'fn main() {\n print("h\xc3\xa9llo");\n}\n' > adv_utf8.ns
printf 'fn main() {\n print("ab");\n}\n' > adv_ok.ns
printf 'import adv_cyc_b;\nfn main() {\n print(1);\n}\n' > adv_cyc_a.ns
printf 'import adv_cyc_a;\nfn adv_b() -> i64 {\n return 1;\n}\n' > adv_cyc_b.ns
printf '' > adv_empty.ns
for f in adv_utf8 adv_ok adv_empty; do
  timeout 30 "$NUSA_BIN" check "$f.ns" >/dev/null 2>&1
  rc=$?
  if crashed "$rc"; then fail "sec-adv" "CRASH $f (rc=$rc)"; fi
done
timeout 30 "$NUSA_BIN" check adv_cyc_a.ns >/dev/null 2>&1
rc=$?
if crashed "$rc"; then fail "sec-adv" "CRASH cycle (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "sec-adv" "import cycle harus ditolak"
pass "sec-adv"

echo "== dependency confusion ditolak (Phase 87) =="
mkdir -p confmylib confapp && cd confapp || exit 1
"$NUSA_BIN" pkg init confapp --dir . >/dev/null 2>&1 || fail "sec-confusion" "init gagal"
printf 'import mylib;\nfn main() {\n print(1);\n}\n' > main.ns
printf '[package]\nname = "confapp"\nversion = "0.1.0"\ncompiler = "3.0.0-beta"\n\n[dependencies]\nmylib = { path = "../confmylib", version = "0.1.0" }\n' > nusa.toml
"$NUSA_BIN" pkg build >conf.log 2>&1
rc=$?
if crashed "$rc"; then cat conf.log; fail "sec-confusion" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "sec-confusion" "path+version ganda harus ditolak"
grep -q "exactly one" conf.log || { cat conf.log; fail "sec-confusion" "pesan hilang"; }
pass "sec-confusion"
cd "$TMPDIR" || exit 1

echo "== batas nol fail-closed (Phase 87) =="
printf 'fn main() {\n    print(1);\n}\n' > lim.ns
"$NUSA_BIN" sandbox lim.ns --memory-mb 0 >lim.log 2>&1
rc=$?
if crashed "$rc"; then cat lim.log; fail "sec-limits" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "sec-limits" "memory-mb 0 harus ditolak (fail-open)"
grep -q "positive" lim.log || { cat lim.log; fail "sec-limits" "pesan hilang"; }
pass "sec-limits-zero"
timeout 10 "$NUSA_BIN" playground --memory-mb 0 --port 0 >pg0.log 2>&1
rc=$?
if crashed "$rc"; then cat pg0.log; fail "sec-limits" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "sec-limits" "playground memory-mb 0 harus ditolak"
pass "sec-limits-pgzero"

echo "== traversal guard (registry jahat) =="
rm -rf "$HOME/.cache/nusa"
mkdir -p reg/index reg/packages evilsrc
export NUSA_REGISTRY="$TMPDIR/reg"
cat > evilsrc/nusa.toml <<'EOF'
[package]
name = "evilsec"
version = "0.1.0"
EOF
printf 'fn dummy() -> i64 {\n    return 1;\n}\n' > evilsrc/evil.ns
printf 'PLANTED\n' > evil_planted_ns
# Craft via python (GNU tar mensterilkan `../` saat create).
python3 - "$TMPDIR" <<'PYEOF' || fail "sec-evil-setup" "craft gagal"
import sys, tarfile
tmp = sys.argv[1]
with tarfile.open(tmp + "/reg/packages/evilsec-0.1.0.tar.gz", "w:gz") as t:
    t.add(tmp + "/evilsrc/nusa.toml", arcname="nusa.toml")
    t.add(tmp + "/evilsrc/evil.ns", arcname="evil.ns")
    t.add(tmp + "/evil_planted_ns", arcname="../evil_planted_ns")
PYEOF
tar -tzf reg/packages/evilsec-0.1.0.tar.gz | grep -q '\.\./evil_planted' || fail "sec-evil-setup" "tarball uji tak berisi traversal"
# Hapus sumber craft: absen-nya pasca-install membuktikan tak tertanam.
rm -f evil_planted_ns
TBSUM=$(sha256sum reg/packages/evilsec-0.1.0.tar.gz | awk '{print $1}')
cat > reg/index/evilsec.toml <<EOF
# Registry index for 'evilsec'.

[[version]]
version = "0.1.0"
checksum = "sha256:0000000000000000000000000000000000000000000000000000000000000000"
tarball = "sha256:$TBSUM"
EOF
mkdir -p consumer && cd consumer || exit 1
"$NUSA_BIN" pkg init consumer --dir . >/dev/null 2>&1 || fail "sec-evil" "init gagal"
"$NUSA_BIN" install evilsec >evil.log 2>&1
rc=$?
if crashed "$rc"; then cat evil.log; fail "sec-evil" "CRASH (rc=$rc)"; fi
[ $rc -ne 0 ] || fail "sec-evil" "tarball jahat harus ditolak"
grep -q "unsafe tarball entries" evil.log || { cat evil.log; fail "sec-evil" "pesan guard hilang"; }
[ ! -f "$TMPDIR/evil_planted_ns" ] || fail "sec-evil" "file tertanam di luar!"
[ ! -f "$HOME/.cache/nusa/evilsec" ] || fail "sec-evil" "artefak jahat tersisa"
pass "sec-evil"
cd "$TMPDIR" || exit 1
rm -rf "$HOME/.cache/nusa"

echo "E2E SEC PASS ($PASS_N cases)"
