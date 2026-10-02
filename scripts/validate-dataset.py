#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Validasi skema ai/dataset.json (REQ-046, DoD-8)."""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
P = os.path.join(ROOT, "website", "ai", "dataset.json")
REQ = ["title", "category", "version", "content", "code_example", "keywords"]

with open(P, encoding="utf-8") as f:
    ds = json.load(f)
assert isinstance(ds, list) and ds, "dataset kosong"
bad = 0
for i, e in enumerate(ds):
    for k in REQ:
        if k not in e:
            print("entry %d: field hilang %s" % (i, k))
            bad += 1
    if "version" in e and not re.match(r"^\d+\.\d+\.\d+$", e["version"]):
        print("entry %d: version invalid %r" % (i, e["version"]))
        bad += 1
size = os.path.getsize(P)
print("entries=%d size=%d bytes" % (len(ds), size))
if size > 5 * 1024 * 1024:
    print("WARN: dataset > 5 MB, harus split per kategori")
    bad += 1
sys.exit(1 if bad else 0)
