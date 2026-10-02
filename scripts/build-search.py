#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Rebuild search/index.json dari website/ yang sudah ada (REQ-038)."""
import os
import re
import json
import html as ihtml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "website")


def text_of(path):
    with open(path, encoding="utf-8") as f:
        h = f.read()
    m = re.search(r'<meta name="nusa:title" content="([^"]*)"', h)
    title = ihtml.unescape(m.group(1)) if m else os.path.basename(path)
    def meta(n):
        mm = re.search(r'<meta name="nusa:%s" content="([^"]*)"' % n, h)
        return ihtml.unescape(mm.group(1)) if mm else ""
    body = re.sub(r"<script.*?</script>", " ", h, flags=re.S)
    body = re.sub(r"<[^>]+>", " ", body)
    return {"url": os.path.relpath(path, OUT), "title": title,
            "category": meta("category"), "version": meta("version") or "2.18.0",
            "keywords": [k.strip() for k in meta("keywords").split(",") if k.strip()],
            "snippet": re.sub(r"\s+", " ", body)[:300].strip(),
            "signature": "", "id": title.split(" ")[0].lower()}


idx = []
for dirpath, _, files in os.walk(OUT):
    for fn in files:
        if fn.endswith(".html"):
            idx.append(text_of(os.path.join(dirpath, fn)))
idx.sort(key=lambda e: e["url"])
os.makedirs(os.path.join(OUT, "search"), exist_ok=True)
with open(os.path.join(OUT, "search", "index.json"), "w", encoding="utf-8") as f:
    json.dump(idx, f, ensure_ascii=False, sort_keys=True, indent=1)
print("search index: %d entries" % len(idx))
