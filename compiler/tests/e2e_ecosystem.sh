#!/usr/bin/env bash
# Integration test (Phase 43 lanjutan): ecosystem lengkap.
# Mengunci komponen §28 master yang sempat tertinggal:
# website statis, tutorials 01-08, examples 2.0 — plus regresi
# templates/registry/RFC/playground agar tetap hijau.
set -u
NUSA_BIN="${1:?usage: e2e_ecosystem.sh <nusa-binary>}"
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

run_ns_ok() {
  local name="$1" src="$2" want="$3"
  cp "$src" "$name.ns" || fail "$name" "copy $src"
  timeout 60 "$NUSA_BIN" run "$name.ns" >"$name.got" 2>"$name.err" \
    || { cat "$name.err"; fail "$name" "run failed"; }
  printf '%s' "$want" | cmp -s "$name.got" - \
    || { echo "--- got ---"; cat "$name.got"; fail "$name" "wrong output"; }
  pass "$name"
}

# 1. Website (layout baru scripts/gen.py): file kunci ada.
for f in index.html README.md assets/style.css assets/app.js \
         docs/index.html downloads/index.html search/index.json \
         sitemap.xml manifest.webmanifest playground/index.html \
         learn/index.html; do
  [ -f "$ROOT/website/$f" ] || fail "website-files" "missing website/$f"
done
[ -f "$ROOT/scripts/gen.py" ] || fail "website-files" "missing scripts/gen.py"
pass "website-files"

# 1b. Generator sinkron (drift situs vs sumber = gagal).
python3 "$ROOT/scripts/gen.py" --check > gen.log 2>&1 \
  || { cat gen.log; fail "website-gen" "site drifted from sources"; }
pass "website-gen"

# 2. Website offline: tanpa dependensi eksternal yang DI-LOAD
# (stylesheet/script/img/fetch brat). Tautan konten <a href> dan
# canonical SEO nusa-lang.org BOLEH (tak di-fetch otomatis).
# Loopback 127.x = API lokal (playground), juga boleh.
if grep -rEn '<link[^>]+href="http|<script[^>]+src="http|<img[^>]+src="http|<source[^>]+src="http|<video[^>]+src="http|url\(http|fetch\(["'\'']http|@import[^;]*http|cdn' \
    "$ROOT"/website --include='*.html' --include='*.css' \
    --include='*.js' 2>/dev/null | grep -v 'rel="canonical"' \
  | grep -v '127\.0\.0\.1'; then
  fail "website-offline" "external load dependency found"
fi
pass "website-offline"

# 3. Website: tautan lokal (href/src) menunjuk file yang ADA —
# query (?code=) dan fragmen (#) diabaikan; direktori = index.html.
python3 - "$ROOT/website" > badlinks.txt <<'PYEOF'
import os, re, sys
root = sys.argv[1]
bad = []
for dp, dn, fn in os.walk(root):
    for f in fn:
        if not f.endswith(".html"):
            continue
        p = os.path.join(dp, f)
        s = open(p, encoding="utf-8", errors="replace").read()
        for m in re.finditer(r'''(?:href|src)="([^"]+)"''', s):
            u = m.group(1).split("#")[0].split("?")[0]
            if (not u or u.startswith(("http:", "https:", "mailto:",
                                       "data:"))):
                continue
            tgt = os.path.normpath(os.path.join(dp, u))
            if os.path.isdir(tgt):
                tgt = os.path.join(tgt, "index.html")
            if not os.path.isfile(tgt):
                bad.append("%s -> %s" % (p, u))
if bad:
    print("\n".join(bad[:20]))
    sys.exit(1)
PYEOF
[ -s badlinks.txt ] && { cat badlinks.txt; fail "website-links" "dangling local link"; }
pass "website-links"

# 4. Tutorials 01-08: run output eksak (disalin ke TMPDIR agar
# cache .nusa-cache/.ll tidak mengotori repo).
run_ns_ok tut01 "$ROOT/tutorials/01-hello/main.ns" 'Hello, Nusa!
'
run_ns_ok tut02 "$ROOT/tutorials/02-variables/main.ns" '30
true
Nusa
1
4
'
run_ns_ok tut03 "$ROOT/tutorials/03-functions/main.ns" '30
Nusa
2
'
run_ns_ok tut04 "$ROOT/tutorials/04-generics-traits/main.ns" '42
hi
A
'
run_ns_ok tut05 "$ROOT/tutorials/05-match-enum/main.ns" '1
2
10
30
'
run_ns_ok tut06 "$ROOT/tutorials/06-error-handling/main.ns" '42
0
21
true
'
run_ns_ok tut07 "$ROOT/tutorials/07-closures-concurrency/main.ns" '3
42
15
42
1
'
run_ns_ok tut08 "$ROOT/tutorials/08-package-project/main.ns" 'Hello, package!
'

# 5. Tutorials + examples: fmt bersih (read-only check).
"$NUSA_BIN" fmt --check "$ROOT"/tutorials/*/main.ns >fmt.log 2>&1 \
  || { cat fmt.log; fail "fmt-tut" "tutorials not fmt-clean"; }
pass "fmt-tut"
"$NUSA_BIN" fmt --check "$ROOT"/examples/generics.ns \
  "$ROOT"/examples/traits.ns "$ROOT"/examples/match.ns \
  "$ROOT"/examples/enums.ns "$ROOT"/examples/closures.ns \
  "$ROOT"/examples/error_option_result.ns \
  "$ROOT"/examples/concurrency.ns >fmt2.log 2>&1 \
  || { cat fmt2.log; fail "fmt-ex" "examples 2.0 not fmt-clean"; }
pass "fmt-ex"

# 6. Examples 2.0: run output eksak.
run_ns_ok ex-generics "$ROOT/examples/generics.ns" '42
hi
1
11
'
run_ns_ok ex-traits "$ROOT/examples/traits.ns" 'Nusa
Nusa
'
run_ns_ok ex-match "$ROOT/examples/match.ns" '10
20
30
1
0
'
run_ns_ok ex-enums "$ROOT/examples/enums.ns" '5
7
-1
'
run_ns_ok ex-closures "$ROOT/examples/closures.ns" '3
42
15
11
'
run_ns_ok ex-error "$ROOT/examples/error_option_result.ns" '42
0
21
true
true
'
run_ns_ok ex-concurrency "$ROOT/examples/concurrency.ns" '42
15
'

# 7. Regresi Phase 43 inti: 4 template masih terdaftar.
LIST="$("$NUSA_BIN" new --list 2>/dev/null)" || fail "templates" "rc != 0"
for t in hello-world cli-app library server; do
  echo "$LIST" | grep -q "$t" || fail "templates" "missing $t"
done
pass "templates"

# 8. Tabel ecosystem: 12/12 ✅, tanpa penanda tertunda.
grep -q '| Website | ✅' "$ROOT/docs/ecosystem.md" \
  || fail "ecosystem-table" "website not marked done"
grep -q '| Tutorials | ✅' "$ROOT/docs/ecosystem.md" \
  || fail "ecosystem-table" "tutorials not marked done"
if grep -q '⏳' "$ROOT/docs/ecosystem.md"; then
  fail "ecosystem-table" "pending marker remains"
fi
[ -f "$ROOT/rfcs/README.md" ] || fail "ecosystem-table" "missing rfcs/README.md"
[ -f "$ROOT/docs/playground.md" ] || fail "ecosystem-table" "missing docs/playground.md"
[ -f "$ROOT/docs/pkg.md" ] || fail "ecosystem-table" "missing docs/pkg.md"
pass "ecosystem-table"

echo "E2E ECOSYSTEM PASS ($PASS_N cases)"
