# RFC-0001: Generics

- Status: Implemented
- Penulis: Nusa team (retroaktif)
- Tanggal: 2026-09-06
- Menggantikan: —
- Digantikan oleh: —

## Summary

Fungsi dan struct generik (`fn identity<T>(x: T) -> T`,
`struct Point<T> { … }`) dengan inferensi call-site dan
monomorphisasi per kombinasi tipe argumen. Terimplementasi sebagai
Phase 21 (v1.2.0, dirilis kumulatif di v1.5.0).

## Motivation

Tanpa generik, `Option<T>`/`Result<T, E>`, koleksi, dan kombinator
(`opt_map`, `res_map`) harus diduplikasi per tipe atau memakai
`void`-hack yang merusak type safety.

## Design

- Sintaks: `fn f<T, U>(…)` + `struct P<T, U> { … }`; konstruksi
  struct inferensi atau eksplisit `Point<i64> { … }`.
- Unifikasi struktural (tembus `[T]`/`fn`); tiap kombinasi argumen →
  instance konkret bernama-mangled; template dibuang pre-codegen.
- Rekursi tipe-sama berhenti (cache); cap kedalaman 64.
- Struct by-value di param/return (menutup TODO v0.8.0).

## Alternatives

- Monomorphisasi vs boxing/uniform-representation: dipilih
  monomorphisasi (nol-cost, cocok LLVM; ukuran biner diterima).
- Turbofish eksplisit: ditolak (inferensi cukup; eksplisit hanya
  untuk konstruksi struct yang ambigu).

## Compatibility

Aditif murni (sintaks baru, program lama tak berubah makna).

## Tests

Unit: parser/semantic/typecheck/codegen/fmt/doc; `e2e_generics.sh`
(12 kasus), `e2e_structs_gen.sh` (14), `e2e_alias.sh` (11),
`e2e_fn.sh` (14).

## Specification

`docs/language-spec-1.0.md` §8; `docs/types.md`.

## Discussion

- 2026-09-06: diputuskan model C++ (body dicek per-instance) agar
  diagnostik mengikuti tipe konkret; state checker save-restore
  anti-korupsi bersarang. Fungsi generik sebagai value ditolak
  eksplisit (inferensi butuh argumen konkret).
