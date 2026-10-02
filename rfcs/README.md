# Nusa RFC System (Phase 41)

Setiap perubahan bahasa yang besar WAJIB melalui RFC sebelum
implementasi. Setelah ekosistem 1.x berjalan, tidak boleh ada
perubahan sintaks/semantik sewenang-wenang di luar proses ini.

## Alur

```
RFC (Proposed)
  ↓  (diskusi tercatat di file RFC)
Design (direvisi di file yang sama)
  ↓
Prototype (bukti kelayakan, bila perlu)
  ↓
Implementation (PR mengacu nomor RFC)
  ↓
Tests (unit + e2e mengacu nomor RFC)
  ↓
Specification (docs/language-spec-* diperbarui)
  ↓
Status → Implemented (atau Rejected bila dibatalkan)
```

## Status yang sah

- `Proposed` — diajukan, menunggu/menjalani diskusi.
- `Accepted` — desain disetujui, belum/diimplementasi.
- `Implemented` — selesai + teruji + spec diperbarui.
- `Rejected` — ditolak beserta alasan (file tetap disimpan).
- `Superseded` — digantikan RFC bernomor lebih baru (wajib rujuk).

## Aturan penomoran & file

- File: `rfcs/RFC-NNNN-slug.md` (NNNN = 4 digit, tanpa lompatan
  tanpa alasan; nomor tak dipakai ulang).
- Satu RFC = satu perubahan besar. RFC historis (0001–0003)
  bersifat retroaktif: mencatat keputusan yang sudah terimplementasi
  agar rationale-nya tidak hilang.
- Wajib memakai `rfcs/TEMPLATE.md` (bagian wajib dicek mesin oleh
  `compiler/tests/e2e_rfc.sh`).
- Diskusi dicatat di bagian `## Discussion` file yang sama
  (tanggal + ringkasan + keputusan), bukan di luar repo.

## Kapan RFC dibutuhkan / tidak

- Butuh RFC: sintaks baru, semantik baru/berubah, tipe baru,
  perubahan ABI/IR yang terlihat, stdlib yang mengubah kontrak,
  perubahan tooling yang mengubah UX.
- Tak butuh RFC: bug fix murni, refactor internal, optimasi tanpa
  perubahan terlihat, dokumentasi, penambahan test.

## Indeks

| Nomor | Judul | Status |
|---|---|---|
| RFC-0001 | Generics | Implemented (v1.2.0) |
| RFC-0002 | Traits | Implemented (v1.3.0) |
| RFC-0003 | Ownership & memory model | Implemented (v1.4.0) |
| RFC-0004 | Async/await | Proposed |
| RFC-0005 | Nusa 2.0 | Accepted |
| RFC-0006 | Tagged channels | Implemented (2.0) |
| RFC-0007 | Garbage collection | Proposed |
| RFC-0008 | FFI expansion | Proposed |
| RFC-0009 | Compile-time programming | Proposed |
| RFC-0010 | Formal verification foundation | Proposed |
| RFC-0011 | Cryptography builtins (slice 1) | Proposed |
| RFC-0012 | Declarative macro system | Proposed |
| RFC-0013 | Vector/SIMD types | Proposed |
