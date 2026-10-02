# Security Model — Audit & Design (Phase 59, target 2.10.0)

Status: audit selesai → Implementation (milestone 1: capability
declaration + 2 enforcement). `nusa audit/tree/outdated/verify`
tetap milik Phase 60.

## 1. Hasil audit (evidence)

| Area | Temuan |
|---|---|
| Package installation | Registry file-backed + checksum 2-lapis, TAPI `tar -xzf` tanpa validasi entri → **path traversal** bila registry jahat (file di luar dir lolos checksum). TERBUKTI by-reading (ensureCached). |
| Dependency execution | TIDAK ADA eksekusi saat install (tanpa build script/ install hook — hanya tar + clang). Deps = sumber, dikompilasi saat build. Permukaan lebih kecil dari cargo/npm. |
| FFI | `extern fn` + `--link`/manifest `[link]` menaut kode native apa pun; TANPA deklarasi capability. |
| Filesystem/process/net | Builtin `file_*`, `process_*`, `tcp_*` tanpa izin; program ad-hoc = user menjalankan kode sendiri (kepercayaan eksplisit). |
| Unsafe code | Tanpa dialek `unsafe{}`; permukaan = builtin + FFI (lih. mem-2.5.md §2). |
| Build scripts | Tidak ada (bukan fitur). |
| Registry tampering | Ditutup checksum transport + konten (Phase 31, e2e_registry). |

## 2. Model izin (deklaratif, deny-by-default bertahap)

```toml
[capabilities]
filesystem = false  # file_read/file_write/file_exists
network = false     # tcp_*/dial (std.net/http)
process = false     # process_exit/env/cwd (+ spawn kelak)
environment = false # (dicadangkan: env terperinci)
native = false      # extern fn + [link] (FFI)
```

- Default `false` (aman bila bagian hilang); unknown key di
  `[capabilities]` ditolak (typo fail-closed).
- `pkg init`/`new` scaffold mencantumkan bagian (semua false).
- `pkg build` mencetak ringkasan capability (visibilitas) dan
  MENOLAK: sumber memakai `extern fn` ATAU `[link]` non-kosong
  tanpa `native = true`. Ad-hoc `nusa build` tak tersentuh
  (tanpa konteks paket; didokumentasikan).
- filesystem/network/process/environment = DEKLARASI (advisory)
  milestone ini; enforcement syscall milik follow-up (butuh
  interposisi; tercatat jujur). `native` dienforce SEKARANG
  karena überprüfbar di AST (`FunctionDecl.isExtern`).

## 3. Traversal guard (enforcement kedua, kecil-kritis)

Selepas `tar -xzf` di `ensureCached`, SEBELUM checksum: walk
direktori — tolak (hapus + error) bila ada subdirektori,
symlink/file-khusus, atau nama tak-waras (kosong/`.`/`..`/
mengandung `/` `\` atau berawalan `.`). Murni `std::filesystem`
(tanpa dialek tar). Publish kita hanya memancarkan top-level
`*.ns` + `nusa.toml`, sehingga paket legitim tak terdampak.

## 4. Acceptance criteria

- Unit: round-trip `[capabilities]` + traversal-walk validator.
- `e2e_sec.sh`: extern tanpa cap → tolak; +`native=true` → build;
  `[link]` tanpa cap → tolak; evil-tarball → tolak bersih;
  paket bersih build + ringkasan capability.
- Seluruh suite hijau; bench identik.
