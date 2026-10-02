#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Nusa Docs generator — Markdown/sumber -> website/ statis (stdlib only).
REQ-016/028/035/046/047/053/054/055/058. Idempoten & deterministik.
Usage: python3 scripts/gen.py [--check]  (--check = gagal bila output berubah)
"""
import os
import re
import sys
import json
import html
import hashlib
import shutil
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.environ.get("NUSA_DOCS_OUT", os.path.join(ROOT, "website"))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
from sitedata import (  # noqa: E402
    LEARN_MODULES, LANG_TOPICS, STD_SINCE, COMPILER_PAGES,
    ECOSYSTEM_PAGES, DIAGNOSTICS_CATALOG, DOCS_VERSION,
)

NUSA = os.path.join(ROOT, "build", "nusa")
PAGES = []  # registry untuk search/dataset: dict(url,title,category,version,keywords,snippet,code)


def stable_version():
    m = None
    with open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8") as f:
        for line in f:
            m = re.match(r"## \[(\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?)\]", line)
            if m:
                return m.group(1)
    return "2.18.0"


STABLE = stable_version()


def run_nusa(args, timeout=60):
    p = subprocess.run([NUSA] + args, capture_output=True, text=True, timeout=timeout, cwd=ROOT)
    return p.returncode, p.stdout, p.stderr


def tut_output(mod_id):
    rc, out, _ = run_nusa(["run", "tutorials/%s/main.ns" % mod_id], timeout=90)
    if rc != 0:
        raise SystemExit("tutorial %s gagal run (rc=%d)" % (mod_id, rc))
    return out


def tut_source(mod_id):
    with open(os.path.join(ROOT, "tutorials", mod_id, "main.ns"), encoding="utf-8") as f:
        return f.read()


def example_output(path):
    try:
        rc, out, _ = run_nusa(["run", path], timeout=90)
    except subprocess.TimeoutExpired:
        return None
    if rc != 0:
        return None
    return out


# ---------- markdown minimal ----------
def md_inline(s):
    s = html.escape(s)
    s = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"`([^`]+)`", r"<code>\1</code>", s)
    s = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', s)
    return s


def md_to_html(md):
    lines = md.split("\n")
    out = []
    i = 0
    in_code = False
    code_lang = ""
    code_buf = []
    in_list = False
    in_table = False

    def flush_code():
        out.append('<pre data-lang="%s"><code class="lang-%s">%s</code></pre>'
                   % (html.escape(code_lang or "text"), html.escape(code_lang or "text"),
                      html.escape("\n".join(code_buf))))
    while i < len(lines):
        ln = lines[i]
        if ln.strip().startswith("```"):
            if in_code:
                flush_code()
                code_buf = []
                in_code = False
            else:
                if in_list:
                    out.append("</ul>")
                    in_list = False
                code_lang = ln.strip()[3:].strip()
                in_code = True
            i += 1
            continue
        if in_code:
            code_buf.append(ln)
            i += 1
            continue
        if re.match(r"^\|.*\|\s*$", ln) and i + 1 < len(lines) and re.match(r"^\|?[\s:\-|]+\|?\s*$", lines[i + 1]):
            if in_list:
                out.append("</ul>")
                in_list = False
            cells = [c.strip() for c in ln.strip().strip("|").split("|")]
            out.append("<table><thead><tr>" + "".join("<th>%s</th>" % md_inline(c) for c in cells) + "</tr></thead><tbody>")
            in_table = True
            i += 2
            continue
        if in_table:
            if re.match(r"^\|.*\|\s*$", ln):
                cells = [c.strip() for c in ln.strip().strip("|").split("|")]
                out.append("<tr>" + "".join("<td>%s</td>" % md_inline(c) for c in cells) + "</tr>")
                i += 1
                continue
            else:
                out.append("</tbody></table>")
                in_table = False
                continue
        m = re.match(r"^(#{1,4})\s+(.*)", ln)
        if m:
            if in_list:
                out.append("</ul>")
                in_list = False
            lvl = len(m.group(1))
            txt = m.group(2).strip()
            slug = re.sub(r"[^a-z0-9]+", "-", txt.lower()).strip("-")
            out.append('<h%d id="%s">%s <a href="#%s" aria-label="taut">#</a></h%d>'
                       % (lvl, slug, md_inline(txt), slug, lvl))
            i += 1
            continue
        if re.match(r"^(\-|\*|\d+\.)\s+", ln):
            if not in_list:
                out.append("<ul>")
                in_list = True
            out.append("<li>%s</li>" % md_inline(re.sub(r"^(\-|\*|\d+\.)\s+", "", ln)))
            i += 1
            continue
        if ln.strip() == "":
            if in_list:
                out.append("</ul>")
                in_list = False
            i += 1
            continue
        if in_list:
            out.append("</ul>")
            in_list = False
        if ln.startswith("> "):
            out.append("<blockquote><p>%s</p></blockquote>" % md_inline(ln[2:]))
        else:
            out.append("<p>%s</p>" % md_inline(ln))
        i += 1
    if in_code:
        flush_code()
    if in_list:
        out.append("</ul>")
    if in_table:
        out.append("</tbody></table>")
    return "\n".join(out)


# ---------- template ----------
NAV = [("learn/", "Learn"), ("language/", "Language"), ("stdlib/", "Stdlib"),
       ("compiler/", "Compiler"), ("ecosystem/", "Ecosystem"), ("reference/", "Reference"),
       ("rfcs/", "RFCs"), ("changelog/", "Changelog"), ("downloads/", "Downloads"),
       ("playground/", "Playground"), ("search/", "Search")]


def rel_to_root(url):
    p = url + "index.html" if url.endswith("/") else url
    depth = p.count("/")
    return "../" * depth if depth > 0 else "./"


def render(title, category, version, keywords, body, url, sidebar=None,
           crumbs=None, prevnext=None, code_example="", description=""):
    base = rel_to_root(url)
    reg = {
        "url": url, "title": title, "category": category, "version": version,
        "keywords": keywords, "snippet": re.sub(r"\s+", " ", re.sub(r"<[^>]+>", " ", body))[:300].strip(),
        "code": code_example,
    }
    PAGES.append(reg)
    dataset = {"title": title, "category": category, "version": version,
               "content": reg["snippet"], "code_example": code_example, "keywords": ", ".join(keywords)}
    nav_html = "".join('<a href="%s%s"%s>%s</a>' % (base, u, ' aria-current="page"' if category.startswith(u.split("/")[0]) else "", t) for u, t in NAV)
    side_html = ""
    if sidebar:
        items = []
        for u, t in sidebar:
            cur = ' aria-current="page"' if u == url else ""
            items.append('<li><a href="%s%s"%s>%s</a></li>' % (base, u, cur, html.escape(t)))
        side_html = "<ul>" + "".join(items) + "</ul>"
    kind = "kind-" + re.sub(r"[^a-z0-9]+", "-", category.lower()).strip("-")
    crumb_html = ""
    if crumbs:
        crumb_html = '<p class="crumbs">' + " / ".join('<a href="%s%s">%s</a>' % (base, u, html.escape(t)) for u, t in crumbs) + "</p>"
    pn_html = ""
    if prevnext:
        (pu, pt), (nu, nt) = prevnext
        pn_html = '<div class="prevnext">%s%s</div>' % (
            ('<a href="%s%s">← %s</a>' % (base, pu, html.escape(pt))) if pu else "<span></span>",
            ('<a href="%s%s">%s →</a>' % (base, nu, html.escape(nt))) if nu else "<span></span>")
    desc = description or (reg["snippet"][:150])
    if title == "Nusa Docs":
        doctitle = "Nusa Docs %s — dokumentasi resmi Bahasa Nusa" % version
    else:
        doctitle = "%s — Nusa Docs %s" % (title, version)
    return """<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%s</title>
<meta name="description" content="%s">
<link rel="canonical" href="https://nusa-lang.org/docs/%s">
<meta property="og:title" content="%s">
<meta property="og:description" content="%s">
<meta http-equiv="Content-Security-Policy" content="default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self'; connect-src 'self' http://127.0.0.1:*; img-src 'self' data:">
<meta name="nusa:title" content="%s">
<meta name="nusa:category" content="%s">
<meta name="nusa:version" content="%s">
<meta name="nusa:keywords" content="%s">
<meta name="nusa-base" content="%s">
<link rel="stylesheet" href="%sassets/style.css">
<link rel="manifest" href="%smanifest.webmanifest">
<script type="application/ld+json">%s</script>
</head>
<body class="%s">
<a class="skip-link" href="#main">Lewati ke konten</a>
<header class="top">
<button id="hamburger" aria-label="Menu">☰</button>
<a class="brand" href="%sindex.html"><b>▚</b> Nusa Docs</a>
<span class="ver-badge">docs %s</span> <span class="ver-badge hot">nusa %s</span>
<nav class="global">%s</nav>
<a class="btn" href="%sdownloads/">Unduh</a>
</header>
<div class="layout">
<aside class="side" aria-label="Navigasi bagian">%s</aside>
<main id="main">%s<article>%s</article>%s</main>
<aside class="toc" aria-label="Daftar isi"><p><b>Stabilitas</b><br><span class="badge %s">%s</span></p></aside>
</div>
<footer><div class="fin"><p>Nusa Docs %s untuk Bahasa Nusa %s (LLVM 21.1.8, AArch64/Termux). Semua halaman bisa dibaca offline lewat service worker.</p><nav><a href="%ssitemap.xml">Peta situs</a><a href="%sai/">Data AI</a><a href="%ssearch/alpha.html">Indeks A–Z</a></nav></div></footer>
<script src="%sassets/app.js" defer></script>
</body>
</html>""" % (
        html.escape(doctitle), html.escape(desc, quote=True), html.escape(url),
        html.escape(doctitle), html.escape(desc, quote=True),
        html.escape(title), html.escape(category), html.escape(version),
        html.escape(", ".join(keywords)), base.rstrip("/") or ".",
        base, base,
        json.dumps(dataset, ensure_ascii=False),
        kind,
        base, DOCS_VERSION, STABLE, nav_html, base,
        side_html, crumb_html, body, pn_html,
        "stable" if version.startswith("2") else "historic",
        html.escape(version), DOCS_VERSION, STABLE, base, base, base, base,
    )


def write(url, content):
    if url.endswith("/"):
        path = os.path.join(OUT, url, "index.html")
    else:
        path = os.path.join(OUT, url)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)


def badge(version, extra=""):
    cls = "stable" if version.startswith("2") else "historic"
    return '<span class="badge %s">v%s</span> %s' % (cls, html.escape(version), extra)


def playground_link(code):
    import urllib.parse
    return "../playground/?code=" + urllib.parse.quote(code[:2000], safe="")


def code_block(lang, code, out=None, try_link=True):
    h = '<pre data-lang="%s"><code class="lang-%s">%s</code></pre>' % (lang, lang, html.escape(code))
    if out is not None:
        h += '<details><summary>Output</summary><samp class="out">%s</samp></details>' % html.escape(out)
    if try_link and lang == "nusa":
        h += '<p><a class="btn sec" href="%s">Coba di playground</a></p>' % playground_link(code)
    return h


if __name__ == "__main__":
    check_only = "--check" in sys.argv
    if check_only:
        import tempfile
        tmp = tempfile.mkdtemp(prefix="nusadocs")
        env = dict(os.environ, NUSA_DOCS_OUT=tmp)
        r = subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "gen.py"),
                            "--build"], capture_output=True, text=True, cwd=ROOT, env=env)
        if r.returncode != 0:
            print(r.stdout + r.stderr)
            sys.exit(r.returncode)
        r2 = subprocess.run(["diff", "-r", "-q", "-x", "README.md", tmp, OUT],
                            capture_output=True, text=True)
        shutil.rmtree(tmp, ignore_errors=True)
        if r2.returncode != 0:
            print("DRIFT:\n" + r2.stdout[:3000])
            sys.exit(1)
        print("no drift")
    else:
        import build_site as _b
        _b.main()
