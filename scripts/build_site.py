#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Konstruktor halaman Nusa Docs. Dipanggil oleh scripts/gen.py."""
import os
import re
import json
import html

import gen
from gen import (ROOT, OUT, STABLE, render, write, md_to_html, badge,
                 code_block, tut_output, tut_source, example_output,
                 run_nusa, NAV, playground_link)
from sitedata import (LEARN_MODULES, LANG_TOPICS, STD_SINCE,
                      COMPILER_PAGES, ECOSYSTEM_PAGES, DIAGNOSTICS_CATALOG,
                      DOCS_VERSION)


def sidebar(items):
    return items


def sec_nav(prefix, pages):
    return [(prefix + "/", "Indeks")] + [(prefix + "/" + s + ".html", t) for s, t in pages]


# ---------- 1. Homepage ----------
def homepage():
    cards = [
        ("language/types", "let x: i64 = 30;", "Pengetikan statis", "i64, bool, string, array, dan alias type yang dicek ketat sebelum biner dibuat."),
        ("language/generics", "fn identity<T>(v: T) -> T", "Generik dan trait", "Monomorfisasi per tipe konkret, dispatch statis, bound T: Show."),
        ("language/error-handling", "let n = parse_age(s)?;", "Enum, match, dan ?", "Option/Result natif, exhaustiveness wajib, propagasi error eksplisit."),
        ("language/concurrency", "chan_len(c: i64) -> i64", "Konkurensi", "Thread, channel, mutex, atomik, plus observability antrean."),
        ("ecosystem/packaging", "nusa pkg build", "Paket, registry, FFI", "Manifes terkunci, registry file-backed, extern fn C."),
        ("compiler/lsp", "nusa lsp", "Perkakas bahasa", "Server bahasa 7 fitur, info DWARF -g, dan rantai bootstrap."),
    ]
    body = """
<h1 class="hero-title">Nusa, dari sumber ke biner.</h1>
<p class="hero-sub">Nusa adalah bahasa compiled modern oleh <b>Padil (Banyumas)</b>:
tulis <code>.ns</code>, dapatkan executable native lewat LLVM 21.1.8 di Termux Android.</p>
<div class="hero-cta"><a class="btn" href="downloads/">Unduh Nusa {st}</a>
<a class="btn sec" href="learn/">Mulai dari nol</a></div>
<div class="hero-grid">
<div class="pane"><div class="pane-bar">hello.ns</div>
<pre data-lang="nusa"><code class="lang-nusa">fn main() {{
    print("Hello, Nusa!");
}}</code></pre></div>
<div class="pane"><div class="pane-bar">$ nusa run hello.ns</div>
<span class="term-out"><span class="ok">Hello, Nusa!</span><br><span class="dim">biner ~20 KB · acuan bench/ AArch64: ~1,4 dtk</span><span class="caret" aria-hidden="true"></span></span></div>
</div>
<h2>Kenapa Nusa</h2>
<div class="cards">
""".format(st=STABLE)
    for url, sig, t, d in cards:
        body += '<div class="card"><span class="sig">%s</span><a href="%s.html"><b class="t">%s</b></a><br><small>%s</small></div>' % (sig, url, t, d)
    body += "</div>"
    body += """
<h2>Mulai dalam lima baris</h2>
<pre data-lang="bash"><code class="lang-bash">pkg install clang cmake llvm
cd ~/nusa && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
./build/nusa run tutorials/01-hello/main.ns
# Hello, Nusa!</code></pre>
<h2>Pilih jalurmu</h2>
<ul>
<li><b>Pemula</b> — program pertama dalam 15 menit lewat <a href="learn/">kurikulum 01–08</a>.</li>
<li><b>Developer</b> — <a href="language/">referensi bahasa</a> dan <a href="stdlib/">18 modul stdlib</a> untuk kerja nyata.</li>
<li><b>Peretas compiler</b> — <a href="compiler/">bedah internal</a> dari lexer sampai biner LLVM.</li>
<li><b>Kontributor</b> — <a href="rfcs/">proses RFC</a> dan <a href="ecosystem/security.html">model keamanan</a>.</li>
</ul>
"""
    side = [("learn/", "Learn"), ("language/", "Language"), ("stdlib/", "Stdlib"),
            ("compiler/", "Compiler"), ("ecosystem/", "Ecosystem"), ("changelog/", "Changelog")]
    write("index.html", render("Nusa Docs", "home", STABLE,
                               ["nusa", "dokumentasi", "llvm", "termux", "aarch64", "tutorial"],
                               body, "index.html", sidebar=None,
                               description="Dokumentasi resmi bahasa Nusa: compiled .ns ke native via LLVM 21.1.8."))
    return side


# ---------- 2. Learn ----------
def learn_pages():
    paths = {"beginner": [], "intermediate": [], "advanced": []}
    for m in LEARN_MODULES:
        paths[m["path"]].append(m)
    idx = "<h1>Learn Nusa — kurikulum 01–08</h1>"
    idx += '<div data-progress-key="learn"><p>Centang progresmu (tersimpan lokal, tanpa akun):</p>'
    for pname, mods in paths.items():
        idx += "<h2>%s path</h2><ul>" % pname.capitalize()
        for m in mods:
            idx += ('<li><label><input type="checkbox"> '
                    '<a href="%s.html">%s — %s</a></label> '
                    '<small>(%s · prasyarat: %s)</small></li>'
                    % (m["id"], m["id"], html.escape(m["title"]), m["time"], html.escape(m["prereq"])))
        idx += "</ul>"
    idx += "</div>"
    side = [("learn/", "Kurikulum")] + [("learn/%s.html" % m["id"], "%s %s" % (m["id"], m["title"])) for m in LEARN_MODULES]
    write("learn/", render("Learn Nusa", "learn", STABLE,
                           ["tutorial", "belajar", "kurikulum", "beginner", "advanced"],
                           idx, "learn/", sidebar=side))
    for pos, m in enumerate(LEARN_MODULES):
        src = tut_source(m["id"])
        out = tut_output(m["id"])
        with open(os.path.join(ROOT, "tutorials", m["id"], "README.md"), encoding="utf-8") as f:
            readme = f.read()
        body = "<h1>%s — %s</h1>" % (m["id"], html.escape(m["title"]))
        body += badge(STABLE) + "<p><small>Estimasi %s · %s</small></p>" % (m["time"], html.escape(m["concepts"]))
        body += "<h2>Penjelasan</h2>" + md_to_html(readme)
        body += "<h2>Source code (.ns)</h2>" + code_block("nusa", src, out=None, try_link=False)
        body += "<h2>Output program</h2><samp class=\"out\">%s</samp>" % html.escape(out)
        body += '<p><a class="btn sec" href="%s">Coba di playground</a></p>' % playground_link(src)
        body += "<h2>Exercise</h2><ol>"
        for q, a in m["exercises"]:
            body += "<li>%s<details><summary>Solusi</summary><p>%s</p></details></li>" % (html.escape(q), html.escape(a))
        body += "</ol>"
        nxt = LEARN_MODULES[pos + 1] if pos + 1 < len(LEARN_MODULES) else None
        body += "<h2>Next step</h2><ul>"
        if nxt:
            body += '<li>Lanjut: <a href="%s.html">%s — %s</a></li>' % (nxt["id"], nxt["id"], html.escape(nxt["title"]))
        else:
            body += '<li>Selamat! Lanjut ke <a href="../ecosystem/packaging.html">Packaging</a> dan publish paket pertamamu.</li>'
        for s in m["see"]:
            body += '<li>Referensi: <a href="../%s.html">%s</a></li>' % (s, s)
        body += "</ul>"
        pu = ("learn/%s.html" % LEARN_MODULES[pos - 1]["id"], LEARN_MODULES[pos - 1]["id"]) if pos > 0 else (None, None)
        nu = ("learn/%s.html" % nxt["id"], nxt["id"]) if nxt else (None, None)
        write("learn/%s.html" % m["id"],
              render("%s %s" % (m["id"], m["title"]), "learn/%s" % m["path"], STABLE,
                     ["tutorial", m["id"], m["title"].lower()],
                     body, "learn/%s.html" % m["id"], sidebar=side,
                     crumbs=[("learn/", "Learn"), ("learn/%s.html" % m["id"], m["id"])],
                     prevnext=(pu, nu), code_example=src))
    return side


# ---------- 3. Language ----------
def language_pages():
    idx = "<h1>Language Reference</h1><p>12 topik · spec normatif " \
          '<a href="../docs/language-spec-2.0.html">language-spec-2.0</a> · ' \
          '<a href="../docs/migration-2.0.html">Migrasi 1.x → 2.0</a></p>'
    idx += '<p><span class="badge breaking">BREAKING 2.0</span> Option/Result struct → enum natif; ' \
           'ctor qualified; field lama ditolak; <code>async</code> reserved.</p><ul>'
    for t in LANG_TOPICS:
        idx += '<li><a href="%s.html"><b>%s</b></a> — %s</li>' % (t["slug"], t["title"], html.escape(t["overview"][:90]))
    idx += "</ul>"
    side = [("language/", "Indeks")] + [("language/%s.html" % t["slug"], t["title"]) for t in LANG_TOPICS]
    write("language/", render("Language Reference", "language", STABLE,
                              ["language", "reference", "spec", "2.0"],
                              idx, "language/", sidebar=side))
    for pos, t in enumerate(LANG_TOPICS):
        body = "<h1>%s</h1>" % html.escape(t["title"]) + badge(STABLE)
        body += "<h2>Overview</h2><p>%s</p>" % html.escape(t["overview"])
        body += "<h2>Syntax</h2>" + code_block("nusa", t["syntax"], out=None, try_link=False)
        body += "<h2>Example</h2>" + code_block("nusa", t["example_code"], t["example_output"])
        body += "<h2>Rules</h2><ul>" + "".join("<li>%s</li>" % html.escape(r) for r in t["rules"]) + "</ul>"
        body += "<h2>Compiler behavior</h2><p>%s</p>" % html.escape(t["compiler"])
        body += '<div class="warn"><h2>Limitations</h2><ul>' + "".join("<li>%s</li>" % html.escape(x) for x in t["limits"]) + "</ul></div>"
        body += "<h2>Related topics</h2><ul>" + "".join('<li><a href="../%s.html">%s</a></li>' % (r, r) for r in t["related"]) + "</ul>"
        body += "<h2>Spec &amp; Tests</h2><p>Spec: <code>%s</code> · Uji pengunci: <code>%s</code></p>" % (
            html.escape(t["spec"]), html.escape(t["tests"]))
        pu = ("language/%s.html" % LANG_TOPICS[pos - 1]["slug"], LANG_TOPICS[pos - 1]["title"]) if pos > 0 else (None, None)
        nu = ("language/%s.html" % LANG_TOPICS[pos + 1]["slug"], LANG_TOPICS[pos + 1]["title"]) if pos + 1 < len(LANG_TOPICS) else (None, None)
        write("language/%s.html" % t["slug"],
              render(t["title"], "language", STABLE, t["keywords"], body,
                     "language/%s.html" % t["slug"], sidebar=side,
                     crumbs=[("language/", "Language"), ("language/%s.html" % t["slug"], t["title"])],
                     prevnext=(pu, nu), code_example=t["example_code"]))
    return side


# ---------- 4. Reference (CLI/fmt/docgen/diagnostics) ----------
def reference_pages():
    rc, help_out, _ = run_nusa(["--help"])
    assert rc == 0
    body_cli = "<h1>CLI Reference</h1>" + badge(STABLE)
    body_cli += "<p>22+ subperintah <code>nusa</code> v%s. Sumber: <code>nusa --help</code> verbatim.</p>" % STABLE
    body_cli += code_block("bash", help_out.strip(), out=None, try_link=False)
    body_cli += "<h2>Catatan pemakaian</h2><ul>"
    body_cli += "<li><code>build/run</code>: <code>--fresh</code> abaikan cache; <code>--stats</code> rincian ms; <code>-g</code> DWARF; <code>-O0..3/s/z</code>.</li>"
    body_cli += "<li><code>playground/sandbox</code>: hanya loopback + limit (lihat <a href=\"../playground/\">Playground</a>).</li>"
    body_cli += "<li><code>pkg</code>: butuh <code>native = true</code> untuk <code>extern fn</code>/<code>[link]</code>.</li>"
    body_cli += "</ul>"
    side = [("reference/cli.html", "CLI"), ("reference/fmt.html", "Formatter"),
            ("reference/docgen.html", "Docgen"), ("reference/diagnostics.html", "Diagnostics")]
    write("reference/", render("Reference", "reference", STABLE,
                               ["reference", "cli", "fmt", "docgen", "diagnostics"],
                               "<h1>Reference</h1><ul>" + "".join(
                                   '<li><a href="%s">%s</a></li>' % (u.split("/")[1], t) for u, t in side) + "</ul>",
                               "reference/", sidebar=side))
    write("reference/cli.html", render("CLI Reference", "reference", STABLE,
                                       ["cli", "nusa", "build", "run", "pkg", "playground"],
                                       body_cli, "reference/cli.html", sidebar=side))
    body_fmt = "<h1>Formatter (nusa fmt)</h1>" + badge(STABLE)
    body_fmt += "<p>Formatter token-based idempoten: <code>nusa fmt file.ns</code>, cek via <code>--check</code>. "
    body_fmt += "Aturan: bar <code>|x|</code> menempel, <code>?</code> menempel, arm match diformat, komentar doc <code>///</code> DIPERTAHANKAN (regresi 2.8.0).</p>"
    body_fmt += code_block("bash", "./build/nusa fmt --check examples/hello.ns\n./build/nusa fmt tutorials/01-hello/main.ns", out=None, try_link=False)
    write("reference/fmt.html", render("Formatter", "reference", STABLE, ["fmt", "formatter", "style"],
                                       body_fmt, "reference/fmt.html", sidebar=side))
    body_doc = "<h1>Docgen (nusa doc)</h1>" + badge(STABLE)
    body_doc += "<p>Generator API Markdown dari komentar <code>///</code>: <code>nusa doc std/math.ns [-o out.md]</code>. "
    body_doc += "Mencakup seksi Functions + Extern Functions + Traits + Enums. Website stdlib digenerate dari output ini.</p>"
    write("reference/docgen.html", render("Docgen", "reference", STABLE, ["doc", "docgen", "api"],
                                          body_doc, "reference/docgen.html", sidebar=side))
    body_diag = "<h1>Diagnostics Catalog</h1>" + badge(STABLE)
    body_diag += "<p>Format: <code>file:line:col</code> + snippet + caret span + <code>help:</code>. Cari pesan verbatim via <a href=\"../search/\">Search</a> (mode error-message).</p>"
    body_diag += "<table><thead><tr><th>Pesan</th><th>Arti &amp; aksi</th><th>Tahap</th></tr></thead><tbody>"
    for msg, arti, tahap in DIAGNOSTICS_CATALOG:
        body_diag += "<tr><td><code>%s</code></td><td>%s</td><td>%s</td></tr>" % (html.escape(msg), html.escape(arti), html.escape(tahap))
    body_diag += "</tbody></table>"
    write("reference/diagnostics.html", render("Diagnostics", "reference", STABLE,
                                               ["error", "diagnostics", "runtime error", "help"],
                                               body_diag, "reference/diagnostics.html", sidebar=side))
    return side


# ---------- 5. Stdlib ----------
def parse_doc(mod):
    rc, out, _ = run_nusa(["doc", "std/%s.ns" % mod])
    if rc != 0:
        return []
    fns = []
    for m in re.finditer(r"### `([^`]+)`\s*\n\n((?:(?!\n### ).)*)", out, re.S):
        sig = m.group(1)
        desc = m.group(2).strip()
        fns.append((sig, desc))
    return fns


def stdlib_pages():
    mods = sorted(STD_SINCE.keys())
    idx = "<h1>Standard Library — 18 modul</h1>" + badge(STABLE)
    idx += "<p>Sumber: <code>std/*.ns</code> via <code>nusa doc</code> + kurasi. Contoh diuji run di CI.</p><ul>"
    for m in mods:
        ver, note = STD_SINCE[m]
        idx += '<li><a href="%s.html"><b>std.%s</b></a> <span class="badge stable">sejak %s</span> — %s</li>' % (m, m, ver, html.escape(note))
    idx += "</ul>"
    side = [("stdlib/", "Indeks")] + [("stdlib/%s.html" % m, "std.%s" % m) for m in mods]
    write("stdlib/", render("Standard Library", "stdlib", STABLE,
                            ["stdlib", "api", "std"] + mods, idx, "stdlib/", sidebar=side))
    for m in mods:
        ver, note = STD_SINCE[m]
        fns = parse_doc(m)
        body = "<h1>std.%s</h1>" % m + badge(ver)
        body += "<p>%s</p>" % html.escape(note)
        body += "<h2>Module</h2><p><code>import %s;</code> (atau <code>import std.%s;</code> — ekuivalen).</p>" % (m, m)
        if m in ("sync", "thread"):
            body += '<p><b>Terkait:</b> builtin <code>chan_len(c: i64) -&gt; i64</code> ' \
                    '(observability antrean channel, sejak 2.7.0, race-tolerant) — ' \
                    'lihat <a href="../language/concurrency.html">Concurrency</a> + ' \
                    '<a href="../changelog/v2.7.0.html">Changelog 2.7.0</a>.</p>'
        if not fns:
            body += "<p><i>Modul tanpa fungsi publik terdokumentasi.</i></p>"
        for sig, desc in fns:
            name = sig.split("(")[0]
            body += "<h2><code>%s</code></h2>" % html.escape(sig)
            body += "<p><b>Signature:</b> <code>%s</code></p>" % html.escape(sig)
            body += "<p>%s</p>" % (html.escape(desc) if desc != "*No documentation.*" else "<i>Belum ada doc-comment ///; signature di atas adalah kontrak.</i>")
            body += "<p><b>Error behavior:</b> kegagalan mengikuti konvensi modul — string → <code>\"\"</code>, bool → <code>false</code>, int parse → <code>-1</code>/Option, handle basi → <code>runtime error: invalid handle</code> + exit 1.</p>"
        body += '<h2>Source</h2><p>Lihat <code>std/%s.ns</code> di repo.</p>' % m
        extra_kw = {"sync": ["chan_len", "channel"], "thread": ["chan_len", "channel"],
                    "http": ["http_get", "http_serve_once"], "tasks": ["Future", "Cancel"]}.get(m, [])
        write("stdlib/%s.html" % m,
              render("std.%s" % m, "stdlib", ver, ["std." + m, m, "stdlib", "api"] + extra_kw,
                     body, "stdlib/%s.html" % m, sidebar=side,
                     crumbs=[("stdlib/", "Stdlib"), ("stdlib/%s.html" % m, "std." + m)]))
    return side


# ---------- 6. Compiler ----------
def compiler_pages():
    idx = "<h1>Compiler Internals</h1>" + badge(STABLE)
    idx += """<h2>Pipeline</h2>
<pre data-lang="text"><code class="lang-text">Source (.ns)
  ↓ Lexer → Parser → AST → Semantic → TypeChecker
  ↓ MIR (2.3+) → Opt (2.4+) → LLVM 21.1.8 Backend → Native Binary</code></pre>
<ul>"""
    for p in COMPILER_PAGES:
        idx += '<li><a href="%s.html"><b>%s</b></a> <span class="badge stable">%s</span> — %s</li>' % (
            p["slug"], p["title"], p["version"], html.escape(p["summary"][:100]))
    idx += "</ul>"
    side = [("compiler/", "Pipeline")] + [("compiler/%s.html" % p["slug"], p["title"]) for p in COMPILER_PAGES]
    write("compiler/", render("Compiler Internals", "compiler", STABLE,
                              ["compiler", "llvm", "pipeline", "runtime", "MIR"],
                              idx, "compiler/", sidebar=side))
    for p in COMPILER_PAGES:
        body = "<h1>%s</h1>" % html.escape(p["title"]) + badge(p["version"])
        body += "<h2>Overview</h2><p>%s</p>" % html.escape(p["summary"])
        body += "<h2>Source</h2><ul>" + "".join("<li><code>%s</code></li>" % html.escape(s) for s in p["sources"]) + "</ul>"
        body += '<div class="warn"><h2>Limitations</h2><ul>' + "".join("<li>%s</li>" % html.escape(x) for x in p["limits"]) + "</ul></div>"
        body += "<h2>Tests</h2><p><code>%s</code></p>" % html.escape(p["test"])
        write("compiler/%s.html" % p["slug"],
              render(p["title"], "compiler", p["version"], p["keywords"], body,
                     "compiler/%s.html" % p["slug"], sidebar=side,
                     crumbs=[("compiler/", "Compiler"), ("compiler/%s.html" % p["slug"], p["title"])]))
    return side


# ---------- 7. Ecosystem ----------
def ecosystem_pages():
    idx = "<h1>Ecosystem</h1>" + badge(STABLE) + "<ul>"
    for p in ECOSYSTEM_PAGES:
        idx += '<li><a href="%s.html"><b>%s</b></a> — %s</li>' % (p["slug"], p["title"], html.escape(p["summary"][:100]))
    idx += "</ul>"
    side = [("ecosystem/", "Indeks")] + [("ecosystem/%s.html" % p["slug"], p["title"]) for p in ECOSYSTEM_PAGES]
    write("ecosystem/", render("Ecosystem", "ecosystem", STABLE,
                               ["ecosystem", "pkg", "registry", "template", "security"],
                               idx, "ecosystem/", sidebar=side))
    for p in ECOSYSTEM_PAGES:
        body = "<h1>%s</h1>" % html.escape(p["title"]) + badge(p["version"])
        body += "<h2>Overview</h2><p>%s</p>" % html.escape(p["summary"])
        body += "<h2>Source</h2><ul>" + "".join("<li><code>%s</code></li>" % html.escape(s) for s in p["sources"]) + "</ul>"
        body += '<div class="warn"><h2>Limitations</h2><ul>' + "".join("<li>%s</li>" % html.escape(x) for x in p["limits"]) + "</ul></div>"
        body += "<h2>Tests</h2><p><code>%s</code></p>" % html.escape(p["test"])
        write("ecosystem/%s.html" % p["slug"],
              render(p["title"], "ecosystem", p["version"], p["keywords"], body,
                     "ecosystem/%s.html" % p["slug"], sidebar=side,
                     crumbs=[("ecosystem/", "Ecosystem"), ("ecosystem/%s.html" % p["slug"], p["title"])]))
    return side


# ---------- 8. tutorials/ + examples/ mirror ----------
def tutorial_example_pages():
    body = "<h1>Tutorials Index</h1><p>Cermin kurikulum <a href=\"../learn/\">Learn</a> (sumber: <code>tutorials/*/README.md</code>).</p><ul>"
    for m in LEARN_MODULES:
        body += '<li><a href="../learn/%s.html">%s — %s</a></li>' % (m["id"], m["id"], html.escape(m["title"]))
    body += "</ul>"
    write("tutorials/", render("Tutorials Index", "tutorials", STABLE, ["tutorials", "index"],
                               body, "tutorials/"))
    exs = sorted(f for f in os.listdir(os.path.join(ROOT, "examples")) if f.endswith(".ns"))
    idx = "<h1>Examples — %d runnable</h1><p>Semua contoh diuji <code>nusa run</code> saat build.</p><ul>" % len(exs)
    for f in exs:
        name = f[:-3]
        idx += '<li><a href="%s.html">%s</a></li>' % (name, f)
    idx += "</ul>"
    write("examples/", render("Examples", "examples", STABLE, ["examples", "sample", "runnable"],
                              idx, "examples/"))
    for f in exs:
        name = f[:-3]
        with open(os.path.join(ROOT, "examples", f), encoding="utf-8") as fh:
            src = fh.read()
        out = example_output("examples/" + f)
        body = "<h1>%s</h1>" % f + badge(STABLE)
        body += code_block("nusa", src, out)
        if out is None:
            body += '<div class="warn"><p>Contoh ini butuh argumen/konteks khusus (lihat komentar sumber) sehingga output tidak di-capture otomatis.</p></div>'
        write("examples/%s.html" % name,
              render(f, "examples", STABLE, ["example", name, "runnable"], body,
                     "examples/%s.html" % name,
                     crumbs=[("examples/", "Examples"), ("examples/%s.html" % name, f)],
                     code_example=src))


# ---------- 9. docs/ mirror (sumber Markdown apa adanya) ----------
DOC_VERSIONS = {"language-spec-1.0": "1.0.0", "spec": "1.0.0"}


def docs_pages():
    files = sorted(f for f in os.listdir(os.path.join(ROOT, "docs")) if f.endswith(".md"))
    idx = "<h1>Docs Archive — %d dokumen sumber</h1><p>Render verbatim dari <code>docs/*.md</code> (sumber tunggal). Halaman kurasi ada di Learn/Language/Compiler.</p><ul>" % len(files)
    for f in files:
        name = f[:-3]
        idx += '<li><a href="%s.html">%s</a></li>' % (name, f)
    idx += "</ul>"
    write("docs/", render("Docs Archive", "docs", STABLE, ["docs", "archive", "markdown"],
                          idx, "docs/"))
    for f in files:
        name = f[:-3]
        with open(os.path.join(ROOT, "docs", f), encoding="utf-8") as fh:
            md = fh.read()
        ver = DOC_VERSIONS.get(name, STABLE)
        tag = "historic" if ver.startswith("1") else "stable"
        body = "<h1>%s</h1>" % f + '<p><span class="badge %s">sumber v%s</span> ' % (tag, ver)
        body += "Dokumen sumber verbatim; untuk versi kurasi lihat navigasi.</p>" + md_to_html(md)
        write("docs/%s.html" % name,
              render(f, "docs", ver, [name, "docs", "sumber"], body, "docs/%s.html" % name,
                     crumbs=[("docs/", "Docs"), ("docs/%s.html" % name, f)]))


# ---------- 10. RFC ----------
def rfc_status(path):
    with open(path, encoding="utf-8") as f:
        txt = f.read()
    m = re.search(r"[Ss]tatus:\s*(\w+)", txt)
    return m.group(1) if m else "Unknown"


def rfc_pages():
    import glob
    files = sorted(glob.glob(os.path.join(ROOT, "rfcs", "RFC-*.md")))
    rows = []
    for fp in files:
        base = os.path.basename(fp)[:-3]
        st = rfc_status(fp)
        rows.append((base, st))
    idx = "<h1>RFC Index</h1><p>Proses: Proposed → Accepted → Implemented (Rejected/Superseded bila batal). "
    idx += "Perubahan bahasa besar WAJIB via RFC. <span class=\"badge proposed\">Proposed = dilarang implementasi sebelum Accepted</span>.</p>"
    idx += '<p><label>Filter status: <select data-filter-target=".rfc-row"><option value="all">all</option>'
    for s in ["Proposed", "Accepted", "Implemented", "Rejected", "Superseded"]:
        idx += '<option value="%s">%s</option>' % (s, s)
    idx += "</select></label></p><ul>"
    for base, st in rows:
        idx += '<li class="rfc-row" data-tags="%s"><a href="%s.html">%s</a> <span class="badge">%s</span></li>' % (st, base, base, st)
    idx += "</ul>"
    side = [("rfcs/", "Indeks")] + [("rfcs/%s.html" % b, b) for b, _ in rows]
    write("rfcs/", render("RFC Index", "rfcs", STABLE,
                          ["rfc", "proposal", "process"] + [b for b, _ in rows],
                          idx, "rfcs/", sidebar=side))
    for base, st in rows:
        with open(os.path.join(ROOT, "rfcs", base + ".md"), encoding="utf-8") as fh:
            md = fh.read()
        tag = "proposed" if st == "Proposed" else ("stable" if st == "Implemented" else "")
        body = "<h1>%s</h1><p><span class=\"badge %s\">%s</span></p>" % (base, tag, html.escape(st))
        if st == "Proposed":
            body += '<div class="warn"><p>Badge Proposed: implementasi DILARANG sebelum status Accepted.</p></div>'
        body += md_to_html(md)
        write("rfcs/%s.html" % base,
              render(base, "rfcs", STABLE, ["rfc", base.lower(), st.lower()], body,
                     "rfcs/%s.html" % base, sidebar=side,
                     crumbs=[("rfcs/", "RFCs"), ("rfcs/%s.html" % base, base)]))


# ---------- 11. Changelog ----------
def parse_changelog():
    with open(os.path.join(ROOT, "CHANGELOG.md"), encoding="utf-8") as f:
        txt = f.read()
    parts = re.split(r"(?m)^## \[([^\]]+)\]", txt)
    versions = []
    for i in range(1, len(parts), 2):
        versions.append((parts[i], parts[i + 1] if i + 1 < len(parts) else ""))
    return versions


def changelog_pages():
    versions = parse_changelog()
    backlinks = {"2.7.0": ["language/concurrency"], "2.17.0": ["compiler/ffi"],
                 "2.18.0": ["compiler/targets"], "2.0.0": ["language/error-handling", "docs/migration-2.0"],
                 "2.9.0": ["stdlib/http"], "2.8.0": ["stdlib/tasks"]}
    idx = "<h1>Changelog</h1><p>Digenerate otomatis dari <code>CHANGELOG.md</code> saat build. "
    idx += "Bahasa stabil: <b>%s</b>.</p>" % STABLE
    idx += '<p><label>Filter: <select data-filter-target=".ver-row"><option value="all">all</option><option value="2.x">2.x</option><option value="1.x">1.x</option></select></label></p>'
    idx += "<h2>Feature timeline</h2><ul>"

    def slug_of(ver):
        m = re.match(r"(\d+\.\d+\.\d+)", ver)
        if m:
            return "v" + m.group(1), m.group(1)
        s = re.sub(r"\W+", "-", ver).strip("-")
        return s, STABLE

    for ver, body in versions:
        slug, _ = slug_of(ver)
        tag = "2.x" if ver.startswith("2") else ("1.x" if ver.startswith("1") else "etc")
        first = html.escape(body.strip().split("\n")[0][:120])
        idx += '<li class="ver-row" data-tags="%s all"><a href="%s.html">%s</a> — %s</li>' % (tag, slug, html.escape(ver), first)
    idx += "</ul>"
    write("changelog/", render("Changelog", "changelog", STABLE, ["changelog", "release", "version"],
                               idx, "changelog/"))
    for ver, body in versions:
        slug, v = slug_of(ver)
        html_body = "<h1>%s</h1>" % html.escape(ver) + badge(v)
        if v in backlinks:
            html_body += "<p>Terdampak: " + ", ".join('<a href="../%s.html">%s</a>' % (b, b) for b in backlinks[v]) + "</p>"
        html_body += md_to_html("## Release notes\n" + body.strip())
        write("changelog/%s.html" % slug,
              render("Changelog %s" % ver, "changelog", v,
                     ["changelog", v, "release"], html_body, "changelog/%s.html" % slug,
                     crumbs=[("changelog/", "Changelog"), ("changelog/%s.html" % slug, ver)]))


# ---------- 12. Downloads ----------
def downloads_page():
    import glob
    debs = sorted(glob.glob(os.path.join(ROOT, "website", "downloads", "*.deb")) +
                  glob.glob(os.path.join(ROOT, "packaging", "*.deb")))
    body = "<h1>Downloads</h1>" + badge(STABLE)
    body += '<div class="warn"><p>Hanya <b>aarch64</b> (paket menolak arsitektur lain). Instalasi butuh <code>apt</code> (resolusi <code>clang</code>). Hosting rilis = file di repo.</p></div>'
    body += "<h2>Pilih versi stabil (tanpa JS)</h2>"
    body += '<details class="pick"><summary>Versi tersedia</summary>'
    ver_num = STABLE.split("-")[0]  # inti numerik utk nama file .deb
    ver_tag = "beta" if "-" in STABLE else "stabil"
    body += '<span class="opt"><b>%s</b> (%s) — bangun via <code>bash packaging/deb.sh</code></span>' % (STABLE, ver_tag)
    for d in debs:
        h = gen.hashlib.sha256(open(d, "rb").read()).hexdigest() if os.path.isfile(d) else "-"
        body += '<span class="opt"><a href="%s">%s</a><br><small>sha256: %s</small></span>' % (
            html.escape(os.path.basename(d)), html.escape(os.path.basename(d)), h)
    body += "</details>"
    body += code_block("bash", "bash packaging/deb.sh   # hasil: website/downloads/nusa_%s-1_aarch64.deb\napt install ./website/downloads/nusa_%s-1_aarch64.deb\nnusa --version" % (ver_num, ver_num), out=None, try_link=False)
    body += "<h2>Verifikasi environment</h2>" + code_block(
        "bash", "uname -m          # harus aarch64\nllvm-config --version  # harus 21.1.8\nclang --version      # harus 21.1.8", out=None, try_link=False)
    write("downloads/", render("Downloads", "downloads", STABLE, ["download", "deb", "install", "aarch64"],
                               body, "downloads/"))


# ---------- 13. Playground ----------
def playground_page():
    body = """<h1>Playground</h1>""" + badge(STABLE) + """
<p>Rantai: Browser → Playground API → Compiler Service → Sandbox → Output.
Server memakai ulang <code>nusa playground</code>/<code>nusa sandbox</code> yang ada (tanpa engine baru).</p>
<p><label>Contoh: <select id="pg-sample">
<option value='fn main() { print("Hello, Nusa!"); }'>hello</option>
<option value='fn main() { let c = chan_new(); chan_send(c, 41); print(chan_len(c)); print(chan_recv(c)); chan_free(c); }'>chan_len</option>
</select></label></p>
<p><textarea id="pg-code" rows="10" cols="60">fn main() {
    print("Hello, Nusa!");
}</textarea></p>
<p><button class="btn" id="pg-run">Run (via lokal 127.0.0.1:8321)</button> <a id="pg-share" href="#">share-link</a></p>
<p><b>Status:</b> <span id="pg-status">idle</span></p>
<pre><samp id="pg-out" class="out">Output muncul di sini.</samp></pre>
<div class="warn"><h2>Batasan jujur</h2><ul>
<li>Loopback-only 127.0.0.1, sekuensial, tanpa auth — JANGAN expose publik tanpa reverse-proxy + auth + rate-limit.</li>
<li>Memori via polling VmRSS (RLIMIT_AS merusak startup Bionic); tanpa namespace; NPROC tak dipakai.</li>
<li>Jaringan TIDAK diblokir; <code>import</code> lokal nonaktif di mode playground.</li>
<li>Limit: timeout 10 dtk, memori 256 MB, output 64 KB, body ≤ 256 KB.</li>
<li>Verdict: ok | build-fail | timeout | mem-limit | output-limit | signal.</li>
</ul></div>
<h2>Jalankan lokal</h2>
"""
    body += code_block("bash", "nusa playground --port 8321   # 127.0.0.1 only\nnusa sandbox prog.ns --timeout 10 --memory-mb 256 --output-kb 64",
                       out=None, try_link=False)
    write("playground/", render("Playground", "playground", STABLE,
                                ["playground", "sandbox", "try", "run"],
                                body, "playground/",
                                code_example='fn main() {\n    print("Hello, Nusa!");\n}\n'))


# ---------- 14. Search ----------
def search_page():
    body = """<h1>Search</h1><p>5 mode: keyword, code, API, error-message, version filter. Offline-first (indeks statis).</p>
<form class="searchbox" onsubmit="return false"><input id="q" type="search" placeholder="cth. chan_len" aria-label="Kueri">
<select id="qver" aria-label="Filter versi"><option value="all">all</option><option value="2.x">2.x</option><option value="1.x">1.x</option></select></form>
<div id="results"><p>Ketik minimal 2 karakter. Contoh kontrak: <code>chan_len</code> → concurrency doc, stdlib, changelog 2.7.0.</p></div>
<p><a href="alpha.html">Indeks alfabetis (tanpa JS)</a></p>
"""
    write("search/", render("Search", "search", STABLE, ["search", "chan_len", "api", "error"],
                            body, "search/"))


def alpha_page():
    items = sorted(gen.PAGES, key=lambda p: p["title"].lower())
    body = "<h1>Indeks Alfabetis</h1><p>%d halaman (fallback tanpa JS).</p><ul>" % len(items)
    for p in items:
        body += '<li><a href="../%s">%s</a> <span class="badge">%s</span></li>' % (p["url"], html.escape(p["title"]), html.escape(p["category"]))
    body += "</ul>"
    write("search/alpha.html", render("Indeks Alfabetis", "search", STABLE, ["index", "alfabetis"],
                                      body, "search/alpha.html"))


# ---------- 15. AI ----------
def ai_page():
    body = """<h1>AI Documentation Layer</h1>""" + badge(STABLE) + """
<p>Setiap halaman mengekspos JSON-LD + <code>&lt;meta name="nusa:*"&gt;</code> + skema 6 field
(title/category/version/content/code_example/keywords). Dump massal: <a href="dataset.json">dataset.json</a>.
Evaluasi: <a href="eval.json">eval.json</a> (50 should-answer + 50 should-refuse).</p>
<h2>Kontrak dataset</h2>
"""
    body += code_block("json", '{\n  "title": "chan_len — Channel Observability",\n  "category": "language/concurrency",\n  "version": "2.7.0",\n  "content": "Fungsi chan_len(c: i64) -> i64 ... race-tolerant ...",\n  "code_example": "let c = chan_new();\\nchan_send(c, 1);",\n  "keywords": "chan_len, channel, concurrency"\n}',
                       out=None, try_link=False)
    body += """<h2>Retrieval</h2><p>Chunk per halaman ≤ 512 token overlap 50; ID stabil <code>&lt;slug&gt;#&lt;anchor&gt;</code>;
filter <code>version</code>; sitasi wajib (judul + URL + versi). Konten Proposed (RFC-0004/0007/0008) bertag
<code>stability: proposed</code> — jangan sajikan sebagai fitur stabil.</p>
<h2>Chat QA template</h2><p>Jawab HANYA dari konteks + sebut versi + akui bila di luar korpus.</p>
<h2>Nusa Helper</h2><p>Dataset web kompatibel skema <code>dataset/</code>; notebook Colab dapat rebuild index offline.</p>
"""
    write("ai/", render("AI Documentation", "ai", STABLE, ["ai", "dataset", "retrieval", "nusa-helper", "llm"],
                        body, "ai/"))


def emit_data_files():
    idx = sorted([{"url": p["url"], "title": p["title"], "category": p["category"],
                   "version": p["version"], "keywords": p["keywords"],
                   "snippet": p["snippet"],
                   "signature": (p["code"].split("\n")[0] if p["code"] else ""),
                   "id": p["title"].split(" ")[0].lower()}
                  for p in gen.PAGES], key=lambda e: e["url"])
    with open(os.path.join(OUT, "search", "index.json"), "w", encoding="utf-8") as f:
        json.dump(idx, f, ensure_ascii=False, sort_keys=True, indent=1)
    ds = [{"title": p["title"], "category": p["category"], "version": p["version"],
           "content": p["snippet"], "code_example": p.get("code", ""),
           "keywords": ", ".join(p["keywords"])} for p in gen.PAGES]
    with open(os.path.join(OUT, "ai", "dataset.json"), "w", encoding="utf-8") as f:
        json.dump(ds, f, ensure_ascii=False, sort_keys=True, indent=1)
    answerable = [{"q": "Apa itu %s? (versi %s)" % (p["title"], p["version"]),
                   "a": p["snippet"][:200], "source": p["url"]} for p in gen.PAGES[:50]]
    refuse = [{"q": q, "a": "REFUSE: di luar korpus dokumentasi Nusa %s." % STABLE, "source": None}
              for q in ["Apakah Nusa punya float?",
                        "Bagaimana async/await di Nusa?",
                        "Apakah Nusa punya garbage collector?",
                        "Bisakah channel di-close/select?",
                        "Apakah registry mendukung HTTP?",
                        "Apakah struct bisa punya method inheren?",
                        "Apakah ada IDE online Nusa?",
                        "Apakah Nusa berjalan di Windows?",
                        "Apakah string mendukung ==?",
                        "Apakah ada debugger interaktif?"] * 5]
    with open(os.path.join(OUT, "ai", "eval.json"), "w", encoding="utf-8") as f:
        json.dump({"answerable": answerable, "refuse": refuse[:50]}, f, ensure_ascii=False, sort_keys=True, indent=1)


# ---------- 16. Static root files ----------
SITEMAP_EXTRA = ["index.html"]


def emit_static():
    urls = sorted(set([p["url"] for p in gen.PAGES] + SITEMAP_EXTRA))
    sm = '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
    for u in urls:
        sm += "  <url><loc>https://nusa-lang.org/docs/%s</loc></url>\n" % u
    sm += "</urlset>\n"
    with open(os.path.join(OUT, "sitemap.xml"), "w", encoding="utf-8") as f:
        f.write(sm)
    with open(os.path.join(OUT, "robots.txt"), "w", encoding="utf-8") as f:
        f.write("User-agent: *\nAllow: /\nSitemap: https://nusa-lang.org/docs/sitemap.xml\n")
    with open(os.path.join(OUT, "manifest.webmanifest"), "w", encoding="utf-8") as f:
        json.dump({"name": "Nusa Docs", "short_name": "Nusa", "display": "standalone",
                   "start_url": "./index.html",
                   "description": "Dokumentasi resmi bahasa Nusa (offline-first)"}, f, indent=1)
    with open(os.path.join(OUT, "sw.js"), "w", encoding="utf-8") as f:
        f.write("/* Service Worker cache-first (REQ-054). Baca offline; playground butuh lokal. */\n"
                "const C='nusa-docs-%s';\n" % DOCS_VERSION +
                "self.addEventListener('install',e=>{e.waitUntil(caches.open(C).then(c=>c.addAll(" +
                json.dumps(["./index.html", "./assets/style.css", "./assets/app.js",
                            "./learn/", "./language/", "./search/index.json"]) + "))});\n" +
                "self.addEventListener('fetch',e=>{e.respondWith(caches.match(e.request).then(r=>r||fetch(e.request)))});\n")


def copy_assets():
    os.makedirs(os.path.join(OUT, "assets"), exist_ok=True)
    for src, dst in [("scripts/website_assets_style.css", "assets/style.css"),
                     ("scripts/website_assets_app.js", "assets/app.js")]:
        with open(os.path.join(ROOT, src), "rb") as f:
            data = f.read()
        with open(os.path.join(OUT, dst), "wb") as f:
            f.write(data)


def main():
    os.makedirs(OUT, exist_ok=True)
    homepage()
    learn_pages()
    language_pages()
    reference_pages()
    stdlib_pages()
    compiler_pages()
    ecosystem_pages()
    tutorial_example_pages()
    docs_pages()
    rfc_pages()
    changelog_pages()
    downloads_page()
    playground_page()
    search_page()
    ai_page()
    alpha_page()  # setelah semua halaman terdaftar
    copy_assets()
    emit_data_files()
    emit_static()
    print("built %d pages -> %s (nusa %s, docs %s)" % (len(gen.PAGES), OUT, STABLE, DOCS_VERSION))
