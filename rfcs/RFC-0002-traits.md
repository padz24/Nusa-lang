# RFC-0002: Traits

- Status: Implemented
- Penulis: Nusa team (retroaktif)
- Tanggal: 2026-09-06
- Menggantikan: —
- Digantikan oleh: —

## Summary

Mekanisme abstraksi `trait` + `impl Trait for Type` dengan dispatch
statis (rewrite ke fungsi mangled) dan bounds `T: Trait` (+ ganda).
Terimplementasi sebagai Phase 22 (v1.3.0).

## Motivation

Generik (RFC-0001) butuh batasan bermakna (`fn max<T: Ord>(…)`):
tanpa traits, bounds tak terungkap dan overloading ad-hoc tak
terkendali.

## Design

- `trait T { fn m(self, …); }` (tanpa body; `self` telanjang
  pertama); `impl T for P { … }` untuk struct (termasuk
  generik-konkret) dan primitif; array/fn/void ditolak.
- Panggilan `recv.m(args)`: resolve statis (tipe, nama) → rewrite
  `Trait_method_Type`; codegen nol baris.
- Bounds dicek saat instansiasi konkret; koherensi unik
  program-wide (satu impl per pasangan trait–tipe).

## Alternatives

- Dynamic dispatch (vtable/objek trait): ditolak untuk fase ini
  (biaya runtime + interaksi borrow-checker yang belum ada).
- Body default/supertrait/`Self`/associated items: ditunda (ruang
  untuk RFC lanjutan tanpa merusak yang ada).

## Compatibility

Aditif murni; mangling memakai namespace terpisah (tabrakan nama
mangled vs deklarasi user ditolak eksplisit).

## Tests

Unit: lexer/parser/semantic/typecheck/fmt/doc; `e2e_traits.sh`
(18 kasus: bounds, primitif, generik, modul, privacy, penolakan).

## Specification

`docs/language-spec-1.0.md` §9; `docs/traits.md`.

## Discussion

- 2026-09-06: dispatch statis dipilih agar kompatibel model memori
  (tanpa fat pointer); bounds ganda `T: A + B` didukung sejak awal
  karena murah di checker.
