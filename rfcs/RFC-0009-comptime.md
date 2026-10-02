# RFC-0009: Compile-time programming

- Status: Proposed
- Penulis: Nusa team
- Tanggal: 2026-09-11
- Menggantikan: —
- Digantikan oleh: —

## Summary

Menambahkan komputasi waktu-kompilasi bertahap ke Nusa: dimulai
dari item `const` (ekspresi konstan murni, slice 2.21.0), menuju
blok `comptime`/eksekusi fungsi murni dengan batasan keamanan
eksplisit. RFC ini Proposed; slice `const` diimplementasikan
sebagai prototipe terkendali (subset aman yang terbukti
terminating), BUKAN persetujuan penuh atas eksekusi Turing-lengkap.

## Motivation

Magic numbers (`30`, `86400`, `"127.0.0.1"`) tersebar tanpa nama;
batas/konfigurasi tak tervalidasi sebelum run; spesialisasi
(nilai yang harus konstan, mis. ukuran buffer) tak terekspresikan.
Bahasa membutuhkan konstanta bernama yang dievaluasi compiler —
tanpa membuka eksekusi arbitrer (keamanan dulu).

## Design

Sketsa penuh (bertahap; hanya slice 1 yang normatif di 2.21.0):

```nusa
const MAX: i64 = 100;          // anotasi opsional (inferensi)
const DOUBLE = MAX * 2;        // ref const lain (urutan-bebas)
const OK: bool = DOUBLE == 200;
```

Slice 1 (2.21.0, diimplementasikan): item top-level `const NAME
[: T] = expr;` dengan `expr` = literal i64/bool/string, unary
`-/+`, arith/perbandingan i64, `==`/`!=` homogen, ref const lain.
Substitusi di typechecker (desugar, cermin match/enum):
codegen TAK tersentuh. Div-nol/overflow = error compile.
Siklus (`A=B,B=A`) = error. Assign ke const = error. Lokal
`let` men-shadow const (aturan shadowing seragam).

Slice berikut (DITUNDA, butuh RFC lanjutan/Accepted): `comptime
{ ... }` blok, pemanggilan fungsi murni, `const fn`, generik
atas konstanta, asersi compile-time. Prasyarat:zulaufuel limit,
determinisme, tanpa IO/FFI/alloc-tak-terbatas (lihat Security).

## Alternatives

- `comptime` penuh langsung: ditolak untuk 2.21.0 (permukaan
  keamanan + terminasi belum terbukti; butuh interpreter).
- Makro (Phase 69): ortogonal (ekspansi sintaks vs evaluasi
  nilai); keduanya bisa koeksis bila ambang makro tercapai.
- `static`/global mutabel: ditolak (melanggar owned-values +
  determinisme; Nusa tanpa interior mutability).

## Compatibility

Aditif: keyword baru `const` (nol pemakaian korpus, diverifikasi
via grep). Semantik lama tak berubah; IR/ABI identik (desugar).
Migrasi: tidak perlu.

## Tests

Unit: lexer/parser/semantic/typecheck (terima + tolak:
non-const, siklus, div-nol, overflow, mismatch, assign,
duplikat); `e2e_const.sh` (runtime + negatif + fmt + doc).

## Specification

`docs/language-spec-2.0.md`: item `ConstDecl` (§2 EBNF),
semantik §3 (substitusi), §4 (aturan const-eval); kata kunci
§1. Dokumen desain: `docs/comptime-2.21.md`.

## Discussion

- 2026-09-11: Proposed bersama prototipe slice-1 (2.21.0).
  Keputusan Accepted penuh menunggu evaluasi slice + kebutuhan
  fungsi-murni/eksekusi (lihat ambang di `docs/comptime-2.21.md`).

## Security

Batas slice 1 (dienforce konstruksi, bukan konvensi): TANPA
panggilan (termasuk builtin), TANPA variabel/parameter, TANPA
loop, TANPA string-ops (literal saja) → terminating trivial,
deterministik, tanpa efek samping, tanpa alokasi. Perluasan
APAPUN (fungsi/loop/IO) wajib membuktikan ulang: fuel limit,
determinisme antar-mesin, tanpa exfiltrasi via pesan error/
waktu, dan audit model memori.
