# Database / Data Tooling — Design (Phase 77, target 2.28.0)

Status: implementasi slice ini. Mandat master: PHASE 77 (TANPA
engine di stdlib; FFI stabil + networking + serialisasi +
ekosistem paket agar ekosistem sediakan SQLite/dll; JSON TOML
CSV format biner). Spec bahasa tak berubah.

## 1. Keputusan cakupan (jujur)

- Engine: TIDAK ADA (mandat). Bukti pola: paket referensi
  `packages/sqlite/` (shim C handle-based + wrapper Nusa +
  manifest `[link]`), bukan modul std.
- FFI dianggap stabil (10 kasus hijau); fase ini hanya
  menambah POLA terdokumentasi (shim untuk API pointer/
  callback yang tak terekspresikan di `extern fn`).
- Networking: cukup (tcp + http + e2e localhost); tanpa Shu.
- Biner: DITUNDA (`ord` belum ada — preseden crypto-2.27).
- Base64: DITUNDA (butuh `ord`).

## 2. Serialisasi murni-Nusa (gaya index/getter — tanpa
// callback/array-return yang berisiko)

- `std/csv.ns` (RFC-4180 praktis): `csv_num_rows(text)`,
  `csv_row(text, i)`, `csv_num_fields(row)`, `csv_field(row,
  i)` — quote ganda, `""` escape, `\n` dalam quote, `\r\n`
  ditoleransi; OOB → `""`.
- `std/json.ns` (objek datar): `json_has_key`,
  `json_get_string/int/bool` (unescape `\" \\ \n \t \r`;
  unknown-escape → char apa adanya cermin lexer; int via
  prefix digit + parse_int; bool hanya literal `true`),
  `json_valid` (kurung seimbang di luar string). Nested/
  array = first-match datar (didokumentasikan).
- `std/toml.ns` (skalar baris): `toml_get_string/int/bool(
  doc, section, key)` (section `""` = top-level; `#` penuh
  baris + inline di luar string diabaikan; `[[array]]`
  tak didukung).

## 3. Paket `packages/sqlite/` (referensi ekosistem)

- `shim.c`: tabel 16 slot `sqlite3*`; `ndb_open(path)`,
  `ndb_exec(id, sql)` (changes/-1), `ndb_query_dump(id, sql,
  path)` (CSV + header nama kolom; rows/-1), `ndb_close`.
  (`extern fn` menolak return string — kepemilikan memori —
  maka hasil via file + csv.ns; SELECT-only, didokumentasikan.)
- `sqlite.ns`: `extern fn` + alias `db_open/exec/query/close`.
- `nusa.toml`: `[link] files=["shim.o"] libraries=["sqlite3"]`
  + `[capabilities] native=true`; `.o` TIDAK di-commit
  (dibangun di e2e; lihat README).
- e2e: guard libsqlite3 (SKIP bersih bila absen); alur
  `pkg init app` + `pkg add --path` + `pkg build` (jalur
  ekosistem) + CRUD :memory: + tutup + negatif
  (open buruk, SQL buruk, query buruk).

## 4. Peta implementasi

`std/csv.ns` + `std/json.ns` + `std/toml.ns` →
`tests/csv_test.ns`, `tests/json_test.ns`, `tests/toml_test.ns`
(`nusa test`) → `packages/sqlite/*` → `e2e_data.sh` (serialisasi
+ sqlite + negatif) → docs (`data-2.28.md` + standard-library +
ffi pola + sqlite README) + CHANGELOG + regen.

## 5. Acceptance criteria

- `nusa test` +3 file hijau; `e2e_data` hijau (atau SKIP
  sqlite yang eksplisit); suite lama hijau; bench identik;
  `.ns.ll` + artefak sqlite bersih.
