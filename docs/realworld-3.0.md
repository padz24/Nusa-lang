# Real-World Validation — Phase 85 (beta 3.0.0)

Tujuan mandat: memakai Nusa untuk proyek nyata (bukan toy
example) dan mencatat masalah desain bahasa yang muncul.
Constraint: freeze beta — NOL perubahan bahasa; temuan
dicatat untuk pasca-3.0, BUKAN diimplementasikan.

## 1. Enam proyek (`realworld/`)

| # | Direktori | Jenis mandat | Deskripsi |
|---|---|---|---|
| 1 | `realworld/wordcount/` | CLI application | `wc`-mini: lines/words/bytes per file via argv |
| 2 | `realworld/todohttp/` | HTTP server | HTTP loopback: `GET /health`, `GET /todos` (JSON), 404; N koneksi sekuensial |
| 3 | `realworld/kvstore/` | network application | TCP KV: `SET k v` / `GET k` per baris, state in-memory, M request |
| 4 | `realworld/nsvet/` | developer tool | pemeriksa higienitas `.ns`: trailing-space, tab, baris>120, tanpa-newline-akhir, hitung `TODO` |
| 5 | `realworld/nstok/` | compiler tool | tokeniser Nusa (subset): hitung keyword/ident/int/string/komentar |
| 6 | `realworld/csvsum/` | data processing | agregasi kolom CSV per nama header: count/sum/min/max (+avg skala-1000, lihat F2) |

Semua murni-Nusa + stdlib; NOL dependensi native; port uji
dinamis 19011–19020 (hindari suite paralel); server
berhenti-sendiri setelah kuota (bounded, tanpa kill-buta).

## 2. Conformance

`compiler/tests/e2e_realworld.sh`: 7 kasus (6 proyek +
fmt+build). Golden deterministik; network via loopback dengan
port dinamis (pola e2e_dist); server kuota-bounded.

## 3. Temuan desain (hasil implementasi — jujur)

- F1 (konfirmasi): tanpa builtin `split`/daftar-direktori —
  parsing di-roll-manual (`index_of`+`slice`); `nsvet` butuh
  daftar file eksplisit. Calon RFC pasca-3.0, bukan blocker.
- F2 (konfirmasi): tanpa float — `csvsum` memakai `avg_x1000`
  skala-bulat. Calon RFC pasca-3.0.
- F3 (konfirmasi, by-design): server = loop sekuensial +
  serve-kuota (tanpa async/executor — RFC-0004 Proposed).
- F4 (BUG NYATA, DIPERBAIKI di slice ini): `file_exists`
  mengembalikan nilai BASI bila dipanggil berulang dari satu
  situs (slot `fe.res` tak-diinit; satu-satunya dari 5 pola
  resSlot yang lupa init — `fw/fr/in/cry` benar). Gejala:
  `wc` melaporkan file-hilang sebagai ada pada argumen
  ke-2+. Perbaikan: satu store `false` sebelum branch
  (`compiler/codegen/Codegen.cpp`); regresi: `filesloop` di
  `e2e_stdlib.sh`. Bugfix diizinkan freeze; NOL perubahan
  bahasa.
- F5 (konfirmasi): tanpa array dinamis (literal fixed-size +
  baca-indeks saja; tanpa push/assign) — `kvstore` memakai
  state log-string append-only. Calon RFC pasca-3.0.
- F6 (kecil, by-design): tanpa `break`/`continue` — loop
  memakai flag-`done` (pola `std/net.ns:recv_line`); `nstok`
  memverifikasi gaya ini skala ke tokenizer penuh.

## 4. Hasil ukur (diisi pasca-verifikasi)

- `e2e_realworld`: 7/7 hijau.
- Full `ctest -j2`: 84/84 hijau (83 in-run + 1 flake-timeout
  `e2e_control[sum]` exit-124 terkunci-hijau isolasi + rerun;
  pola flake-beban tercatat sejak Phase 79).
