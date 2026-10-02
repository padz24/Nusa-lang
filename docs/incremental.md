# Incremental Compilation (Phase 35, v1.16.0)

```sh
nusa build app.ns -o app   # Built ... (dingin: frontend + clang)
nusa build app.ns -o app   # Fresh app (cached) — 7x lebih cepat
```

## Model: frontend selalu, clang kondisional

Pipeline dibagi dua berdasarkan biaya terukur (lihat `docs/perf.md`):

- **Frontend** (load → lex → parse → check → IR, ~0.2–0.4 dtk)
  SELALU jalan. Murah, dan satu-satunya cara sound mengetahui
  modul/dependensi tanpa cache AST rapuh.
- **Backend** (clang link, ~1–1.4 dtk) DILEWATI bila artefak masih
  valid. Inilah penghematan nyatanya.

## Fingerprint (`<outdir>/.nusa-cache/fingerprint`)

Satu build diidentifikasi oleh:

- `compiler` = sha256 binary nusa (ganti compiler → rebuild),
- `target`/`opt`/`emit`/`sysroot` + input link (`-l` nama; file
  beserta hash isinya),
- semua modul transitif `(path, sha256-isi)` — termasuk modul std
  bila diimpor (di-hash dari disk, bukan nama),
- `ir` = sha256 IR yang terakhir diemisikan.

Hit bila: flag sama + IR sama + output ada → `Fresh <out> (cached)`.
IR dibandingkan (BUKAN hanya input) sehingga perubahan komentar/
format ikut hemat; aman pula andaikata compiler nondeterministik
(IR beda → rebuild; tak pernah artefak basi).

Miss melaporkan penyebab: `rebuilding: ~a.ns, +b.ns, -c.ns`
(`~` isi berubah, `+` baru, `-` hilang) atau
`rebuilding: flags changed`. `--fresh` memaksa build penuh
(juga via `pkg build --fresh`).

## Dependency graph

`compiler/incr`: simpul per modul (path absolut, sha, daftar nama
import) dari `ModuleLoader`. Edge best-effort via basename (cukup
untuk pelaporan; kebenaran tak bergantung padanya — fingerprint
memakai SET terurut).

## Yang TIDAK di-cache (sadar + beralasan)

- **AST/semantik per-modul**: analisis Nusa global (monomorphisasi,
  dispatch statis, lifting closure lintas-modul) — mempartisinya
  butuh query system (proyek tersendiri); frontend murah sehingga
  penghematannya tak berarti.
- **Split objek per-modul**: alasan sama + penempatan instance
  generik takdeterministik per modul akan merusak cache.
- **Skip frontend**: butuh determinisme terbukti + load-cache;
  penghematan (0.3 dtk) tak sebanding risiko artefak basi.
