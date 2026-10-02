#!/usr/bin/env bash
# check-budget.sh — REQ-053/064..067: budget ukuran + penegakan performa statis.
set -u
cd "$(dirname "$0")/.."
FAIL=0
CSS=$(wc -c < website/assets/style.css); JS=$(wc -c < website/assets/app.js)
echo "css=$CSS (<=25600) js=$JS (<=46080)"
[ "$CSS" -gt 25600 ] && { echo "BUDGET-FAIL css"; FAIL=1; }
[ "$JS" -gt 46080 ] && { echo "BUDGET-FAIL js"; FAIL=1; }
BIG=0
while IFS= read -r f; do
  S=$(wc -c < "$f")
  if [ "$S" -gt 61440 ]; then echo "BUDGET-FAIL html>60KB: $f ($S)"; BIG=1; fi
done < <(find website -name '*.html')
[ $BIG -eq 1 ] && FAIL=1
if grep -rq 'cdn\.\|googleapis\|unpkg\|jsdelivr' website --include='*.html' --include='*.js' --include='*.css'; then
  echo "BUDGET-FAIL: referensi CDN eksternal ditemukan"; FAIL=1
fi
IDX=$(wc -c < website/search/index.json)
echo "search-index=$IDX (<=512000)"
[ "$IDX" -gt 512000 ] && { echo "BUDGET-FAIL search index"; FAIL=1; }
[ $FAIL -eq 0 ] && echo "budget OK" || { echo "budget FAILED"; exit 1; }
