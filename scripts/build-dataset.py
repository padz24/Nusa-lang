#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Rebuild ai/dataset.json dari website/ yang sudah ada (REQ-046/047)."""
import os
import re
import json
import html as ihtml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "website")

ds = []
for dirpath, _, files in sorted(os.walk(OUT)):
    for fn in sorted(files):
        if not fn.endswith(".html"):
            continue
        p = os.path.join(dirpath, fn)
        with open(p, encoding="utf-8") as f:
            h = f.read()
        def meta(n):
            m = re.search(r'<meta name="nusa:%s" content="([^"]*)"' % n, h)
            return ihtml.unescape(m.group(1)) if m else ""
        codes = re.findall(r'<code class="lang-nusa">(.*?)</code>', h, flags=re.S)
        body = re.sub(r"<[^>]+>", " ", h)
        ds.append({
            "title": meta("title") or fn,
            "category": meta("category"),
            "version": meta("version") or "2.18.0",
            "content": re.sub(r"\s+", " ", body)[:2000].strip(),
            "code_example": ihtml.unescape(codes[0])[:2000] if codes else "",
            "keywords": meta("keywords"),
        })
os.makedirs(os.path.join(OUT, "ai"), exist_ok=True)
with open(os.path.join(OUT, "ai", "dataset.json"), "w", encoding="utf-8") as f:
    json.dump(ds, f, ensure_ascii=False, sort_keys=True, indent=1)
print("dataset: %d entries" % len(ds))
