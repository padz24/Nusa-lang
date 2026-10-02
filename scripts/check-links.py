#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Link-check website/: resolusi per-file termasuk subdirektori (DoD-1).
Gagal (exit 1) bila ada tautan internal mati. Eksternal http(s) dilewati."""
import os
import re
import sys
import urllib.parse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "website")

HREF = re.compile(r'''(?:href|src)="([^"#]+)(?:#[^"]*)?"''')
errors = []
count = 0
for dirpath, _, files in os.walk(OUT):
    for fn in files:
        if not fn.endswith(".html"):
            continue
        page = os.path.join(dirpath, fn)
        with open(page, encoding="utf-8") as f:
            h = f.read()
        for m in HREF.finditer(h):
            u = m.group(1).strip().split("?")[0].split("#")[0]
            if not u or u.startswith(("http://", "https://", "mailto:", "data:")):
                continue
            if u.startswith("http://127.0.0.1"):
                continue
            count += 1
            target = os.path.normpath(os.path.join(os.path.dirname(page), urllib.parse.unquote(u)))
            if os.path.isdir(target):
                target = os.path.join(target, "index.html")
            if not os.path.isfile(target):
                errors.append("%s -> %s" % (os.path.relpath(page, OUT), u))
        # larangan tautan .md mentah (pola gen.py)
        for m in re.finditer(r'href="([^"]*\.md[^"]*)"', h):
            errors.append("%s RAW-MD %s" % (os.path.relpath(page, OUT), m.group(1)))

print("checked %d internal links" % count)
if errors:
    print("BROKEN (%d):" % len(errors))
    for e in errors[:30]:
        print("  " + e)
    sys.exit(1)
print("links OK")
