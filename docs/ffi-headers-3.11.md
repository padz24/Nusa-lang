# Native Interoperability 3.0 — Milestone 1: `nusa headers` (Phase 110)

Status: slice implementasi (target 3.11.0). Mandat master: C,
pustaka C-kompatibel, system libs, shared libs natif; tooling
bindgen/headers/abi-check; C++ hanya subsystem terpisah (TIDAK
dikerjakan — tidak ada dependensi ABI C++ di compiler).

## 1. Cakupan: Nusa → C (`nusa headers <file.ns> [-o out.h]`)

Kebalikan `nusa bindgen`: subset API yang representasi C-nya
JUJUR (abi-3.10) diekspor; sisanya skip + warning (cermin
filosofi bindgen). Pemetaan: `i64→int64_t`, `string→const
char *` (pinjaman program-lifetime, JANGAN free), `void→void`,
struct datar (urutan deklarasi, non-packed), enum (struct
`{tag, v{t}_{pos}}` + komentar tag). PENOLAKAN eksplisit:
`bool` (i1 vs `_Bool` = mismatch biner), closure (pair vs
function-pointer), generik/metode (nama mangled), array/fn
field, extern/main (bukan API).

## 2. Resep embedding (terbukti, didokumentasikan di header)

Objek Nusa membawa `main` sendiri (fn main wajib): link program
C dengan `-Wl,--allow-multiple-definition` agar main C menang
(terverifikasi ld.lld Termux). Objek `main`-opsional = follow-up
(butuh flag semantik). `nusa abi-check` = follow-up (ujungnya
adalah `e2e_abi` yang sudah ada).

## 3. Diverifikasi

- Unit `test_headers` 4/4 (golden, penolakan, kosong-gagal, rusak).
- `e2e_headers` 4/4: golden + `-o`, LINK C↔Nusa nyata
  (`add/mkpoint/get` → `42/7 p/7 0`), penolakan, negatif.
- `e2e_bindgen` + `e2e_abi` + `e2e_release` hijau (tak tersentuh);
  `.ns` + IR OK.

## 4. Ditunda jujur

- C++ interop (subsystem terpisah; tanpa komitmen ABI).
- Objek tanpa-main, ekspor instans generik konkret, metode
  lewat simbol mangled, `nusa abi-check` CLI.
