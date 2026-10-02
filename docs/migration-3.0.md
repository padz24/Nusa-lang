# Migrasi 2.x → 3.0 (Phase 83: COMPATIBILITY AUDIT)

Kesimpulan dulu: **NOL migrasi bahasa diperlukan.** Spec 3.0
final adalah ratifikasi freeze 2.x — nol sintaks/semantik
baru, nol breaking change. Satu-satunya pekerjaan migrasi
adalah audit klasifikasi + dependensi platform. Tool:
`nusa migrate [PATH]` (§5).

## 1. Kategori (definisi operasional `nusa migrate`)

| Kategori | Arti | Exit |
|---|---|---|
| compatible | `check` hijau (program), atau gagal tepat `no 'fn main' found` (library) | 0 |
| deprecated | compatible + memakai permukaan legacy yang masih diterima (builtin channel tanpa-tag, §2) | 0 |
| requires migration | `check` gagal + sumber mengandung idiom 1.x (§3) | 1 |
| broken | `check` gagal tanpa idiom 1.x (bug proyek, bukan bahasa) | 1 |
| unsupported | di paket `native = true` (butuh toolchain C platform, §4) | 0* |

`*` unsupported tak memblokir exit (bukan isu bahasa), tapi
tercetak eksplisit di ringkasan — penyebaran binernya butuh
verifikasi per-platform (§4).

Preseden: file gagal `check` karena idiom 1.x diklasifikasi
requires migration (BUKAN broken) agar saran perbaikannya
tepat (tabel §3, cermin `docs/migration-2.0.md` B1/B2).

## 2. Deprecated (masih diterima, ganti bila sempat)

Builtin channel tanpa-tag (v1.9.0) digantikan varian bertag
2.0 (`chan_new_bool/send_bool/recv_bool`,
`chan_new_str/send_str/recv_str`). `chan_len` (v2.7.0) TAK
termasuk (tanpa varian bertag — bukan legacy).

| File | Pemakaian legacy |
|---|---|
| `std/thread.ns` | `chan_new/recv/send` tanpa-tag |
| `std/tasks.ns` | `chan_send/chan_new` tanpa-tag (`chan_len` OK) |
| `examples/concurrency.ns` | `chan_send` tanpa-tag |

Catatan: dua di antaranya adalah stdlib itu sendiri (pemakai
internal permukaan legacy — migrasi internal dijadwalkan
pasca-3.0, tanpa mengubah API publik).

## 3. Requires migration (idiom 1.x yang ditolak 3.0)

| 1.x/2.x-lama (ditolak) | 3.0 |
|---|---|
| `none(d)` / `ok(v, d)` / `err(e, d)` (dummy-arg) | `Option<T>.None()` / `Result<T, E>.Ok(v)` / `.Err(e)` |
| `o.value`, `o.has_value`, `r.is_ok` (akses field) | `match` / `is_some` / `is_ok(r)` / `?` / kombinator |
| `struct Option` / `struct Result` sendiri | ganti ke enum natif (atau ganti nama) |
| `async`/`await`/`macro` sebagai identifier | ganti nama (reserved tanpa sintaks) |

Hasil audit korpus repo: **0 file** (semua idiom di atas
nol pemakaian — pola mirip yang terdeteksi seperti
`m.errors`/`is_none(` adalah false positive, diverifikasi
manual).

## 4. Unsupported (dependensi platform)

| File | Sebab |
|---|---|
| `packages/tls/tls.ns` | paket `native = true` (butuh OpenSSL toolchain) |
| `packages/sqlite/sqlite.ns` | paket `native = true` (butuh SQLite toolchain) |

Keduanya library yang `check`-bersih; kategorinya berasal dari
manifest, bukan dari bahasa. Panduan: pin versi via lock,
`nusa verify` pasca-install, dan uji biner per target
(`--target`) sebelum rilis.

## 5. Tooling: `nusa migrate [PATH]`

- Argumen: file `.ns` tunggal atau direktori (default `.`).
  Walk direktori rekursif, terurut, deterministik; melewati
  `build/ website/ target/ dist/ .nusa-cache/ .git/`.
- Per file: jalankan pipeline `check` sungguhan (bukan
  heuristik buta) + pindai marker idiom-1.x/legacy pada
  sumber yang sudah dibersihkan dari komentar-`//` dan
  literal-string + deteksi manifest `native = true` ke atas
  (maks 4 level).
- Output stdout stabil per baris `<path>: <kategori>
  (<program|library|check-failed)>`; baris akhir `migrate: <C>
  compatible, <D> deprecated, <R> requires migration, <B>
  broken, <U> unsupported (N files)`.
- Exit 1 bila R > 0 atau B > 0; selain itu 0. NOL file
  ditulis/dimodifikasi (read-only; tanpa codegen).
- Keterbatasan jujur: pembeda requires-migration vs broken
  adalah heuristik marker — kasus gagal-check yang mengandung
  kata mirip-idiom di nama field sendiri bisa terklasifikasi
  requires migration; pesan error `check` asli tetap acuan.

## 6. Hasil audit korpus repo (diukur, 87 file; +6 proyek Phase 85)

- compatible: **82** (51 program + 31 library).
- deprecated: **3** (§2).
- requires migration: **0**.
- broken: **0**.
- unsupported: **2** (§4).
- Total: 87 (ambang `e2e_compat` tetap: 46+ ok / 35+ lib;
  kategorisasi ini memecahnya lebih rinci).

Conformance: `compiler/tests/e2e_migrate.sh` (audit repo +
fixture 5 kategori + idempotensi read-only).
