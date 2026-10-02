# Nusa 2.0 Design Review (Phase 44 — dokumen, tanpa code)

Evaluasi menyeluruh pra-2.0 per 15 area mandat master. Keputusan
diusulkan di RFC-0005 (`rfcs/RFC-0005-nusa-2.0.md`, Proposed);
dokumen ini = bahan mentah temuan. Bukti dirujuk ke test/doc yang
ada — bukan opini.

## 1. Syntax — KEEP (+2 reserved)

Stabil sejak 1.0; match/enum/lambda/extern menempel tanpa merusak
program lama. Utang: `return` wajib terakhir per block + **bug
terkonfirmasi: bare `return;` di dalam `if` pada `fn main`
gagal verifikasi IR** (`ret void` vs `i32 @main`; repro:
`if c { return; }` di main — workaround `else`). WAJIB diperbaiki
pre-2.0 (bukan breaking: program pemicu dziś tak terkompilasi).
`async`/`await` reserved mengikuti RFC-0004 bila Accepted.

## 2. Type System — KEEP, satu kandidat breaking

Alias transparan + monomorphisasi + bounds terbukti (e2e 11–18
kasus per area). Kandidat breaking: `Option`/`Result` encoding
struct (konstruktor butuh dummy) → sum-type natif (bersama enum
1.7.0, fondasi sudah ada). Dampak: API std berubah → butuh migrasi
tercatat. Tanpa itu, 2.0 mengunci encoding dummy selamanya.

## 3. Memory Model — KEEP + GC sebagai pertanyaan terbuka

Owned + program-lifetime heap sudah normatif (§5 spec 1.0).
Fakta: string bocor by-design; loop alokasi tumbuh/iterasi; tanpa
TCO. Pertanyaan 2.0: GC (tracing/RC/arena-return) atau pertahankan
tanpa-GC + dokumentasi? Keputusan memengaruhi FFI + async.

## 4. Generics — KEEP

Tanpa temuan. Ekstensi (metode generik, turbofish) = RFC lanjutan,
bukan 2.0-blocker.

## 5. Traits — KEEP

Dispatch statis + koherensi unik program-wide stabil. Supertrait/
default-body/`Self` = RFC lanjutan.

## 6. Error Handling — KEEP, tergantung §2

`?` + kombinator stabil. Bila sum-type natif (§2) diterima,
`Option`/`?`-untuk-`Option` + konversi error otomatis jadi kandidat
2.0 (butuh RFC rincian sendiri).

## 7. Concurrency — KEEP + async di luar 2.0 kecuali RFC-0004 Accepted

Primitif 1.9.0 deterministik-teruji. Channel `i64`-saja + tanpa
select/close = batas terdokumentasi (generik channel butuh
monomorphisasi runtime × tipe — desain belum ada; JANGAN dijanjikan
di 2.0).

## 8. Modules — KEEP

Flat-neighbor + pkg system koeksis baik. Tanpa perubahan.

## 9. Package System — KEEP

Path-deps + registry file-backed + checksum dua-lapis memadai untuk
ekosistem kini. Yank/namespace/HTTP-registry = non-blocker.

## 10. FFI — KEEP

Batasan primitif (§12 spec) = batas keamanan sadar, bukan utang.
Perluasan (struct-by-value/callback) = RFC + audit keamanan.

## 11. Compiler Architecture — PERBAIKI SEBELUM 2.0

Utang: (a) bug bare-return-main (§1); (b) incremental =
frontend-selalu (disengaja, murah — pertahankan, catat);
(c) LSP satu-file tanpa typechecker; (d) DWARF skalar-saja;
(e) cap 500 nesting / 100 rantai / 25 error (disengaja — pertahankan
+ dokumentasikan sebagai kontrak, bukan malu-maluin).
(a) satu-satunya bug; sisanya keputusan sadar → nyatakan normatif.

## 12. Runtime — KEEP

`nusa_rt.c` + precompiled obj stabil. `RLIMIT_AS` dilarang di
Bionic (temuan Phase 42) → runtime 2.0 tetap polling + dokumentasi.

## 13. Standard Library — TAMBAH TERBATAS

Kesenjangan terkonfirmasi: `tcp_listen`/`accept` (template server
masih stub!), UDP/TLS (sengaja-tidak), PRNG non-kripto, cap recv
1 MiB. Usulan 2.0: `tcp_listen`/`accept` SAJA (menutup janji
template); sisanya tetap-batas.

## 14. Tooling — KEEP

fmt/lsp/pkg/test/doc/bootstrap/sandbox/playground/new lengkap.
LSP multi-file + typechecker-hover = non-blocker.

## 15. Cross Compilation — KEEP

LP64 ELF matriks (host + linux-aarch64/x86-64) + penolakan
Windows/macOS beralasan. Tanpa perubahan.

## Konsolidasi

- Breaking changes (diusulkan): (B1) sum-type natif Option/Result;
  (B2) keyword `async`/`await` (bila RFC-0004 Accepted).
- Deprecated (diusulkan): konstruktor struct-dummy Option/Result
  (bila B1); `import` tetangga-flat untuk paket (arahkan ke pkg,
  tanpa menghapus).
- Technical debt (wajib pre-2.0): bug bare-return-main; selebihnya
  (O(n²) selfhost-lex, channel-i64, registry-lokal, playground-
  sekuensial, leak-by-design, wrapping, caps) DINYATAKAN kontrak.
- Compiler limitations → spec 2.0 (jujur, bukan janji).
- Stdlib limitations → tambah listen/accept, sisanya kontrak.
