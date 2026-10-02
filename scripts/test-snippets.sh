#!/usr/bin/env bash
# test-snippets.sh — REQ-023/030/043: semua .ns web harus run + fmt bersih.
set -u
cd "$(dirname "$0")/.."
FAIL=0
check() {
  if ! ./build/nusa fmt --check "$1" >/dev/null 2>&1; then echo "FMT-FAIL $1"; FAIL=1; fi
}
run() {
  if ! ./build/nusa run "$1" >/dev/null 2>/tmp/nusa_snip.err; then
    echo "RUN-FAIL $1"; head -n 3 /tmp/nusa_snip.err; FAIL=1;
  fi
}
for d in tutorials/*/; do
  check "$d/main.ns"; run "$d/main.ns"
done
for f in examples/*.ns; do
  check "$f"
  # math.ns = modul library tanpa fn main (diimpor main.ns) → fmt saja, by design.
  case "$f" in *math.ns) continue;; esac
  run "$f"
done
# contoh halaman language (inline di sitedata): verifikasi via file temp
python3 - scripts/sitedata.py <<'EOF'
import sys, subprocess, tempfile, os
sys.path.insert(0, 'scripts')
from sitedata import LANG_TOPICS
for t in LANG_TOPICS:
    if t['slug'] == 'modules':
        continue
    with tempfile.NamedTemporaryFile('w', suffix='.ns', delete=False, dir='/tmp') as fh:
        fh.write(t['example_code']); p = fh.name
    r = subprocess.run(['./build/nusa', 'run', p], capture_output=True, text=True, timeout=60)
    os.unlink(p)
    exp = t['example_output']
    if r.returncode != 0 or r.stdout.strip() != exp.strip():
        print('LANG-FAIL %s: rc=%d out=%r exp=%r' % (t['slug'], r.returncode, r.stdout.strip(), exp.strip()))
        sys.exit(1)
print('lang examples OK')
EOF
[ $? -ne 0 ] && FAIL=1
[ $FAIL -eq 0 ] && echo "snippets OK" || { echo "snippets FAILED"; exit 1; }
