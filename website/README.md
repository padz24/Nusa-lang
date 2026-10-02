# Nusa Docs — Website Dokumentasi Resmi (Docs v0.1.0 · Bahasa 2.18.0)

Situs statis offline-first, tanpa CDN/JS eksternal/tracking/build-step.
Sumber tunggal: `docs/`, `tutorials/`, `examples/`, `std/`, `rfcs/`,
`CHANGELOG.md`, `nusa --help`. Jangan edit `website/*.html` manual —
generate ulang.

## Preview lokal

```bash
python3 scripts/gen.py
python3 -m http.server -d website 8080
# buka http://127.0.0.1:8080/
```

## Skrip

| Skrip | Fungsi |
|---|---|
| `scripts/gen.py` | Entry build: Markdown/sumber → `website/` (idempoten; `--check` gagal bila drift) |
| `scripts/build_site.py` | Konstruktor halaman (dipanggil gen.py) |
| `scripts/sitedata.py` | Data kurasi (contoh `.ns` terverifikasi run) |
| `scripts/build-search.py` | Rebuild `search/index.json` dari HTML |
| `scripts/build-dataset.py` | Rebuild `ai/dataset.json` dari HTML |
| `scripts/check-links.py` | Link-check internal (resolusi per-file) |
| `scripts/check-coverage.py` | Cakupan sumber → halaman (DoD-1) |
| `scripts/validate-dataset.py` | Validasi skema dataset 6-field (DoD-8) |
| `scripts/test-snippets.sh` | `run` + `fmt --check` semua `.ns` web |
| `scripts/check-budget.sh` | Budget CSS/JS/HTML/indeks + larangan CDN |

## Verifikasi rilis (semua hijau 2026-09-10)

```bash
python3 scripts/gen.py            # built 208 pages
python3 scripts/check-links.py    # 5857 internal links OK
bash scripts/check-budget.sh      # css 9,9 KB / js 7,5 KB / indeks 114 KB
python3 scripts/validate-dataset.py
python3 scripts/check-coverage.py
bash scripts/test-snippets.sh     # lolos sebelum redesign; konten .ns tak berubah sesudahnya
python3 scripts/gen.py --check    # no drift
```

## Deploy

Opsi 1 GitHub Pages · Opsi 2 Cloudflare Pages · Opsi 3 lokal
(`python3 -m http.server`). Hanya file statis; playground butuh
`nusa playground --port 8321` lokal (127.0.0.1, sekuensial, berlimit).

## Batasan jujur

- Playground web hanya klien untuk API loopback lokal; situs statis
  tidak mengeksekusi kode (tombol Run degradasi ke `nusa run`).
- `examples/math.ns` = library tanpa `main` (fmt saja, by design).
- Bahasa didukung: Nusa 2.18.0 (spec normatif 2.0 + adendum);
  halaman 1.x bertag historis, RFC Proposed bertag larangan implementasi.
