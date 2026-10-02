# RFC-0003: Ownership & memory model

- Status: Implemented
- Penulis: Nusa team (retroaktif)
- Tanggal: 2026-09-06
- Menggantikan: —
- Digantikan oleh: —

## Summary

Model memori resmi: owned values di stack + heap string
program-lifetime, tanpa GC/pinjaman/RC. Terimplementasi sebagai
Phase 23 (v1.4.0): dokumen desain dulu, lalu status by-design di
kode (mengganti komentar "bocor sengaja").

## Motivation

Concurrency (Phase 28) dan FFI (Phase 32) DILARANG tanpa model
memori stabil (mandat master). Opsi harus dipilih eksplisit dan
dijelaskan sederhana agar cocok LLVM + Android/Bionic.

## Design

- Owned values; variabel = slot stack per scope; copy semantik
  nilai untuk agregat; string heap bocor by-design hingga roadmap GC.
- 8 area normatif: Allocation, Deallocation, Ownership, References
  (tidak ada referensi user-level), Mutability, Lifetime, Stack, Heap.
- Konsekuensi tercatat: loop alokasi tumbuh per iterasi; tanpa TCO;
  closure capture by-value (RFC lanjutan bila perlu move/borrow).

## Alternatives

- Borrowing ala Rust: ditolak (kompleksitas checker vs nilai;
  dievaluasi ulang bila referensi user-level ditambahkan).
- GC (tracing/RC): ditolak untuk 1.x (prediktabilitas + footprint
  Android); pintu dibuka untuk 2.x.
- Arena per-scope: ditolak (lifetime tak cocok untuk return).

## Compatibility

Klarifikasi (bukan perubahan perilaku): program yang berjalan
tetap berjalan; yang mengandalkan `free` manual tetap tak ada.

## Tests

`e2e_memory.sh` (copy, concat-loop, input-EOF, struct);
`e2e_concurrency.sh` memvalidasi interaksi model × threads.

## Specification

`docs/language-spec-1.0.md` §5; `docs/memory-model.md` (normatif).

## Discussion

- 2026-09-06: "jangan implementasi sebelum dokumen ada" dipatuhi;
  evaluasi opsi didokumentasikan di `memory-model.md` sebelum
  status kode diubah.
