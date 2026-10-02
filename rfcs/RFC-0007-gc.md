# RFC-0007: Garbage collection

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-08
- Menggantikan: —
- Digantikan oleh: —

## Summary

Mengganti heap string program-lifetime (RFC-0003) dengan koleksi
otomatis. RFC ini BELUM disetujui; 2.0 MEMPERTAHANKAN model
tanpa-GC sebagai kontrak. Implementasi dilarang sebelum Accepted.

## Motivation

Loop alokasi tumbuh per iterasi + string bocor by-design membatasi
program long-running (server template serve-once hari ini justru
menghindari masalah ini). Klaim "aman memori" Nusa tak lengkap
tanpa jawaban alokasi tak-terbatas.

## Design

Sketsa (terbuka direvisi):

- Kandidat A — RC presisi + break-cycle eksplisit: cocok model
  owned (setiap owner jelas), biaya sinkronisasi antar-thread
  (atomic counts), SIKLUS bocor (didokumentasikan, bukan ditutup).
- Kandidat B — konservatif (Boehm-style): tanpa anotasi compiler,
  tetapi false-retention + interaksi closure-pair buram tak
  terkarakterisasi; perlu spike pengukuran dulu.
- Kandidat C — arena per-request + owned global: server/request
  pola dibebaskan massal; general-purpose tetap bocor.
- Wajib sebelum Accepted: (1) prototype + bench (throughput,
  footprint, pause); (2) interaksi FFI (pointer keluar Nusa =
  root? pin?); (3) interaksi thread (stop-the-world vs lokal);
  (4) kapan destruktor berjalan (tak ada Drop hari ini).

## Alternatives

- Tetap tanpa-GC selamanya: opsi sah bila bench menunjukkan biaya
  > nilai (diputuskan via data, bukan ideologi).
- Borrow-checker (RFC-0003 menolak): membuka ulang penolakan =
  butuh justifikasi baru yang kuat.

## Compatibility

APAPUN pilihannya: tanpa perubahan sintaks yang terlihat
(diutamakan); program 1.x/2.0 berjalan identik (hanya footprint).

## Tests

Rencana: uji soak (alokasi-loop konstan-memori), uji thread +
GC, uji FFI-pin, bench perbandingan ketiga kandidat.

## Specification

Bila diterima: spec §5 ditulis ulang + `docs/memory-model.md`
revisi mayor.

## Discussion

- 2026-09-08: dibuka sebagai Proposed; SENGAJA di luar 2.0 —
  klaim GC tanpa root-tracking teruji = tidak bertanggung jawab.
  Data bench kandidat adalah acceptance criteria RFC ini.
