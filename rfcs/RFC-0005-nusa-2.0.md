# RFC-0005: Nusa 2.0

- Status: Accepted
- Penulis: Nusa team
- Tanggal: 2026-09-08
- Menggantikan: —
- Digantikan oleh: —

## Summary

Rilis mayor 2.0 = stabilisasi, BUKAN fitur besar: kunci bahasa
(spec 1.0 → 2.0), perbaiki satu bug terkonfirmasi (bare-return di
main), putuskan dua kandidat breaking (sum-type natif, keyword
async), tambah `tcp_listen`/`accept`, nyatakan seluruh batas sadar
sebagai kontrak, dan sediakan migration guide 1.x → 2.0. Bahan
mentah: `docs/nusa-2.0-design.md` (15 area).

## Motivation

Ekosistem 1.x (pkg/registry/LSP/playground/templates) membuat
perubahan sewenang-wenang mahal. Tanpa RFC 2.0, utang kecil
membusuk (bug main-return) dan batas-sadar tak pernah dikunci
sehingga tiap rilis ditagih janji yang tak pernah dibuat
(channel-generik, GC, async).

## Design

Lingkup TERTUTUP 2.0 (di luar ini = RFC lanjutan):

1. Perbaiki bug bare-`return;`-di-`if`-dalam-`main` (gagal verif
   IR; repro di design §1). Bukan breaking (pemicu dziś tak build).
2. Putuskan B1 (sum-type natif Option/Result) YA/TIDAK + migrasi.
3. Putuskan B2 (keyword async/await) mengikuti RFC-0004.
4. Tambah `tcp_listen`/`accept` (menutup janji template server).
5. Kunci kontrak: caps (500/100/25), wrapping, leak-by-design,
   channel-i64, registry-lokal, playground-sekuensial, DWARF-skalar,
   LSP-satu-file → spec 2.0.
6. Migration guide 1.x → 2.0 + Definition of Done §31 master
   dicentang ulang satu-per-satu.

## Alternatives

- Big-bang 2.0 (tulis ulang + fitur): DITOLAK (mandat master:
  jangan rewrite diam-diam; risiko regresi 47-test suite).
- 2.0 = re-tag 1.20.0 tanpa perubahan: DITOLAK (bug main-return
  + janji template tak tertutup = rilis tak jujur).
- Inkremental terkunci (dipilih): satu RFC payung ini + RFC
  rincian (B1/RFC-0004) + checklist DoD.

## Compatibility

B1/B2 = breaking terkontrol (daftar + migrasi + versi mayor =
tempat yang tepat). Selain itu 2.0 WAJIB backward-compatible
dengan 1.20.0 (suite 47 hijau + program korpus byte-identik).

## Tests

- Regression: seluruh suite kini + repro bug main-return sebagai
  kasus `e2e_control`/typecheck (dari penolakan→penerimaan).
- Migrasi: korpus program 1.x (examples/bench/std) build+run
  identik di bawah 2.0; bila B1: bonus migrasi `opt_map` otomatis?
  (diputuskan di RFC rincian).
- `e2e_spec` naik ke spec 2.0 (13 topik + revisi B-keputusan).

## Specification

`docs/language-spec-1.0.md` → `docs/language-spec-2.0.md`
(revisi §4/§5/§11/§13 + kontrak §15 diperluas); spec 1.0 disimpan
(riwayat, bukan ditimpa).

## Discussion

- 2026-09-08: dibuka sebagai Proposed; diputuskan Accepted hari
  yang sama: B1 = YA (sum-type natif, breaking), B2 = parsial
  (channel bertag RFC-0006 masuk 2.0; GC/FFI-callback sebagai
  RFC-0007/0008 terdesain untuk 2.1 — klaim tanpa bukti ditolak).
  Sisa keputusan: (1) migrasi B1 dikunci di migration-2.0.md;
  (2) `tcp_listen` blocking-dulu (non-blocking = RFC lanjutan);
  (3) checklist DoD §31 dicentang di laporan rilis 2.0.
