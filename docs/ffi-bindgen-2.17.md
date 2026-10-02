# Bindgen — Design (Phase 66, target 2.17.0)

Status: audit → Implementation (milestone 1: `nusa bindgen`
subset C). Tanpa perubahan checker/codegen (output = apa yang
checker terima hari ini). C++ interop tetap ditolak (mangling).

## 1. Cakupan subset (jujur)

Masuk: prototipe fungsi file-scope (`ret name(params);`),
tipe integer C (`int/long/long long/short/char` + `signed`/
`unsigned`, multi-word `unsigned long long`), `_Bool`/`bool`,
`char*`/`const char*` (param saja), `void` return/`(void)`,
`extern "C"` wrapper, tabel typedef builtin (`size_t`,
`ssize_t`, `int8/16/32/64_t`, `uint8/16/32/64_t`,
`intptr_t/uintptr_t`), `()` kosong = nol param (asumsi
terdokumentasi), definisi `static inline` (dilewati anggun),
komentar + baris `#` dilewati.

Keluar (skip + peringatan ke stderr, BUKAN fatal): float/double,
pointer umum, struct/union/enum-by-value, variadik, typedef
kustom, makro fungsi, atribut `__attribute__` (ditoleransi bila
menempel deklarasi? TIDAK — skip bila mengaburkan; lihat §3).

Pemetaan = tabel `docs/ffi.md` (int→i64 dst.; LP64 PCS;
string-return DITOLAK cermin checker). Nama param hilang →
`p0..`; `void` return → tanpa `-> T`.

## 2. CLI & semantik hasil

`nusa bindgen <header.h> [-o out.ns]` (default stdout).
rc 0 bila ≥1 fungsi teremisikan (peringatan tak menggagalkan);
rc 1 bila nol (bukan header yang dikenali) atau file tak terbaca.
Output lolos `nusa fmt --check` + `nusa check` (dengan stub
program? file binding TANPA main — check butuh main; e2e
memvalidasi via program pengimpor... modul: binding file
di-`import` program. Catatan: `extern` di modul impor — loader
merge, didukung? e2e membuktikan).

## 3. DITUNDA

Struct/enum/union layout, macro konstanta, `static inline`
(isu ODR — skip), C++ (mangling/namespaces/classes),
`--allowlist/--denylist` filter, multi-header sekaligus.

## 4. Acceptance criteria

- Header sampel (libc subset + header sintetik tepi) → output
  eksak golden; ujung-ke-ujung: binding `answer()/twice()` custom
  .c → build+run 42 (cermin e2e_ffi v2).
- `e2e_bindgen.sh` hijau + suite hijau; bench identik.
