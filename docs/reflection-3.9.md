# Reflection & Introspection — Milestone 1: Evaluasi + Metadata (Phase 108)

Status: slice evaluasi + implementasi tooling (target 3.9.0).
Mandat master: evaluasi reflection; runtime reflection hanya bila
perlu; pertimbangkan compile-time reflection + metadata tipe/
struct/enum/fungsi untuk serialization, codegen, tooling,
diagnostics.

## 1. Evaluasi → vonis

- Runtime reflection (query tipe saat jalan, downcast dinamis):
  TIDAK DIBANGUN. Alasan: Nusa monomorphisasi statis tanpa
  type-info runtime; menambahnya = biaya memori + permukaan
  keamanan + preseden dinamis yang bertentangan dengan model
  owned-values statis. Nol kebutuhan dari `realworld/` (F1–F6
  tak menyebut refleksi).
- Compile-time reflection (kode menanya tipe saat kompilasi,
  mis. `typeof(x)`): DITUNDA ke RFC (level bahasa — aturan §4.10),
  BUKAN ditolak. Prasyarat: kasus serialization nyata yang gagal
  tanpa itu (belum ada: JSON/TOML/CSV getter + RPC envelope
  bekerja dengan getter eksplisit).
- Metadata untuk tooling: DIBANGUN slice ini — `nusa doc
  --format json`: render mesin dari data yang SAMA dengan
  Markdown (fungsi/struct/alias/const/enum/trait/extern +
  signature + tipe + `///`). Tanpa semantik baru, tanpa RFC
  (flag opt-in; UX Markdown tak berubah).

## 2. Skema JSON (deterministik; urutan = urutan sumber)

```
{"file": "...",
 "functions": [{"name","tparams":[],"params":[{"name","type"}],
                "ret":"...","extern":false,"doc":"..."}],
 "structs": [{"name","tparams":[],"fields":[{"name","type"}],"doc"}],
 "aliases": [{"name","target","doc"}],
 "consts": [{"name","type":null|"...","doc"}],
 "enums": [{"name","tparams":[],"variants":[{"name","payload":[]}],"doc"}],
 "traits": [{"name","methods":[...],"doc"}],
 "externs": [...]}
```

String di-escape JSON (`\" \\ \n \t \r \u00XX`); `doc` = gabungan
`///` menempel ("" bila tak ada); tipe = `Type::name()` yang sama
dengan Markdown (satu kebenaran). Import tak diikuti (cermin
Markdown). Gagal lex/parse → rc=1 + error identik Markdown.

## 3. Diverifikasi

- Unit `test_doc` 11/11 (9 lama — Markdown byte-identik pasca
  refactor prolog — + 2 baru JSON/struktur + error-paritas).
- `e2e_tooling` + JSON (golden + `-o` + format-asing + bad);
  `e2e_release` hijau; `.ns` + IR OK (tanpa sentuh pipeline).

## 4. Ditunda jujur

- `typeof`/introspeksi level-bahasa (RFC bila ada kebutuhan
  serialization nyata); runtime reflection (tidak direncanakan);
  resolve-import di metadata (gabung manual seperti Markdown).
