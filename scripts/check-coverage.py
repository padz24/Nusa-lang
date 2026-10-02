#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check-coverage.py — DoD-1: tiap sumber terindeks, nol halaman yatim."""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "website")

docs = {f[:-3] for f in os.listdir(os.path.join(ROOT, "docs")) if f.endswith(".md")}
have_docs = set()
d = os.path.join(OUT, "docs")
if os.path.isdir(d):
    have_docs = {f[:-5] for f in os.listdir(d) if f.endswith(".html")}
missing_docs = docs - have_docs

std = {f[:-3] for f in os.listdir(os.path.join(ROOT, "std")) if f.endswith(".ns")}
have_std = set()
s = os.path.join(OUT, "stdlib")
if os.path.isdir(s):
    have_std = {f[:-5] for f in os.listdir(s) if f.endswith(".html")} - {"index"}
missing_std = std - have_std

import glob
rfcs = {os.path.basename(p)[:-3] for p in glob.glob(os.path.join(ROOT, "rfcs", "RFC-*.md"))}
have_rfc = set()
r = os.path.join(OUT, "rfcs")
if os.path.isdir(r):
    have_rfc = {f[:-5] for f in os.listdir(r) if f.endswith(".html")} - {"index"}
missing_rfc = rfcs - have_rfc

tuts = {m for m in os.listdir(os.path.join(ROOT, "tutorials")) if m.startswith("0")}
have_learn = set()
lrn = os.path.join(OUT, "learn")
if os.path.isdir(lrn):
    have_learn = {f[:-5] for f in os.listdir(lrn) if f.endswith(".html")} - {"index"}
missing_learn = tuts - have_learn

ok = True
for label, missing in [("docs", missing_docs), ("stdlib", missing_std),
                       ("rfcs", missing_rfc), ("learn", missing_learn)]:
    if missing:
        print("MISSING %s: %s" % (label, sorted(missing)))
        ok = False
    else:
        print("coverage %s OK" % label)
sys.exit(0 if ok else 1)
