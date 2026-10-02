# Nusa Language Specification 3.0 — Candidate (Phase 81 freeze)

> HISTORIS (Phase 82): diratifikasi menjadi
> `docs/language-spec-3.0.md` (normatif final). Dokumen ini
> dipertahankan sebagai arsip lampiran freeze.

- Status: **candidate** (bukan final; final di Phase 82).
- Normatif penuh tetap: `docs/language-spec-2.0.md` (bila
  bertentangan dengan dokumen ini, dokumen 2.0 menang sampai
  3.0 final diratifikasi).
- Dokumen ini = lampiran freeze: mengunci 6 area + UB +
  conformance + riwayat delta. NOL sintaks/semantik baru.

## 1. Freeze map (6/6 area terkunci)

- syntax: spec-2.0 §1–§3; detail `docs/syntax.md`.
  Kunci: UTF-8, `//`/`///`, keywords (termasuk `async await
  macro` reserved-ditolak), desimal-i64, escapes
  `\" \\ \n \t \r \0`, operator `+ - * / = == != < > <= >=
  -> => ? |`, `!` tunggal ditolak.
- type semantics: spec-2.0 §4,§8,§9,§10; detail
  `docs/types.md`, `structs.md`, `enums.md`,
  `pattern-matching.md`, `traits.md`, `functions.md`.
  Kunci: `i64/bool/string/void`, struct/enum generik,
  trait+impl, match eksaustif, tanpa float/GC/metode-generik.
- memory semantics: spec-2.0 §5 + Appendix A; detail
  `docs/memory-model.md`, `docs/mem-2.5.md`.
  Kunci: owned-move per nilai, bocor-by-design tak direklamasi,
  `extern` return-string ditolak (kepemilikan).
- module semantics: spec-2.0 §7; detail `docs/modules.md`,
  `docs/pkg.md`. Kunci: `import` path-file, modul-tanpa-main
  = library (gagal `check` TEPAT `no 'fn main' found`),
  paket file-backed lokal.
- error semantics: spec-2.0 §6,§14; detail
  `docs/error-handling.md`. Kunci: `file:line:col: error:` +
  snippet/caret + `help:` (Inggris), recovery top-level, cap
  25/tahap, exit != 0 tanpa sinyal, hang DILARANG.
- concurrency semantics: spec-2.0 §11; detail
  `docs/concurrency.md`, `docs/concurrency-2.7.md`.
  Kunci: thread/task blocking, interleave undefined, tanpa
  `async` eksekusi (reserved), timeout/retry/discovery =
  library (bukan primitif bahasa).

## 2. Di luar kontrak stabil (tetap UB/reserved)

GC, `&&`, float, `break`, `+=`, supertrait, `async` eksekusi,
macro system, comptime, FFI non-`extern`-C-min, `nusa lex`
kanal/status invalid, artefak `.ll`/`.s`, PRNG/waktu/interleave,
NUL di nilai string, literal ≥ 2^63. Lihat Appendix A spec-2.0.

## 3. Standard library (non-normatif bahasa)

Daftar normatif §13 spec-2.0 + `docs/standard-library.md`;
tambahan 2.26→2.30 (`crypto/data/server/rpc/discovery/timeout/
retry/config/metrics`) = library, BUKAN perubahan bahasa.

## 4. Conformance (pengunci)

- `compiler/tests/e2e_spec2.sh` (13 topik §1–§13 + probe
  natif/reject) WAJIB hijau.
- `compiler/tests/e2e_compat.sh` (81 file: 46 ok + 35 lib)
  WAJIB hijau (jaring regresi semantik).
- `compiler/tests/e2e_freeze.sh` (Phase 81) WAJIB hijau.

## 5. Riwayat delta bahasa (2.0 → freeze)

2.1→2.30: NOL perubahan sintaks/semantik (terverifikasi:
parser/checker/codegen/lexer tak tersentuh sejak 09-11;
perubahan = stdlib/paket/harness/docs). Satu-satunya temuan
terkait-terminasi (Phase 80, fuzz generator) = bug harness,
bukan semantik bahasa.

## 6. Perubahan disetujui selama freeze (slice ini: NOL)

NOL bugfix bahasa; NOL improvement. Daftar ini sengaja kosong
— setiap entri masa depan WAJIB bernomor RFC + uji pengunci.

## 7. Open items (didelegasikan ke Phase 82+)

Spesifikasi final 3.0 (lexical→platform semantics independen
implementasi), audit kompatibilitas 2.x + `nusa migrate`,
beta — di luar slice ini.
