# RFC-0008: FFI expansion

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-08
- Menggantikan: —
- Digantikan oleh: —

## Summary

Melampaui FFI primitif 1.13.0 (i64/string/bool/void): struct
by-value dan callback C. RFC ini BELUM disetujui; 2.0
mempertahankan batas primitif. Implementasi dilarang sebelum
Accepted.

## Motivation

`tcp_listen` (2.0) masih ditulis di runtime C karena Nusa tak bisa
menerima callback (event-loop, comparator qsort, pthread-create
langsung) dan tak bisa bertukar struct nilai dengan C — dua pola
yang memaksa "turun ke C" dan melemahkan janji self-hosting.

## Design

Sketsa (terbuka direvisi):

- Struct by-value: ABI AArch64 PCS (HFA/HVA, koersi 16-byte)
  via atribut LLVM; Nusa struct (pointer hari ini) butuh
  representasi ganda atau konversi marshal di batas — dipilih
  marshal eksplisit dulu (benar > cepat), ukur kemudian.
- Callback: closure Nusa → function pointer C membutuhkan thunk
  executable per instance (libffi-style closure API — TIDAK
  tersedia di Bionic!). Alternatif: hanya fn top-level tanpa
  capture (trampolin statis, tanpa alokasi executable) sebagai
  fase 1; capture bila ada mekanisme executable-memory yang aman
  (W^X Android melarang mmap RWX).
- Keamanan: daftar gelang-putih simbol + `unsafe` block eksplisit?
  (diputuskan di diskusi; 1.x "pustaka dipercaya penuh" tak
  berskala ke callback).

## Alternatives

- Tetap primitif + tulis runtime di C: status quo (bekerja,
  terbukti 2.0); kelemahan: logika bocor ke C.
- Bindgen otomatis (header → `extern`): membutuhkan parser C —
  proyek sendiri; tidak sebelum callback jalan manual.

## Compatibility

Aditif (signature baru yang dulu ditolak kini diterima); program
lama tak berubah. `unsafe`-gating (bila dipilih) = sintaks baru
minor.

## Tests

Rencana: round-trip struct (padding/align fuzz vs C), callback
tanpa-capture (qsort comparator), callback-capture DITOLAK
dengan pesan (bila fase 1), uji W^X (mmap RWX gagal bersih).

## Specification

Bila diterima: spec §12 ditulis ulang + `docs/ffi.md` + audit
keamanan.

## Discussion

- 2026-09-08: dibuka sebagai Proposed; SENGAJA di luar 2.0 —
  executable-thunk di Android (W^X) adalah risiko platform yang
  butuh spike sebelum janji apa pun.
