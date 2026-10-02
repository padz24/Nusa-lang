# Profile-Guided Optimization — Milestone 1: Alur clang (Phase 104)

Status: slice implementasi. Mandat master Phase 104 (PGO: build
instrumented → run workload → collect → optimize → release; CLI
`build --profile` / `profile` / `build --use-profile`; target 3.5.0;
implikasi reproducibility didokumentasikan).

## 1. Deviasi CLI (disengaja, tercatat)

- `nusa build --profile` SUDAH MILIK Phase 62 (dev/debug/release);
  `nusa bench` + `build --stats` (Phase 65) menutup peran `nusa
  profile`. Menimpa keduanya = breaking perubahan stabilitas.
- Dipilih namespace `--pgo-*` (prioritas master: stability > features):
  - `nusa build prog.ns -o prog_inst --pgo-generate` (clang
    `-fprofile-generate`; biner menulis `*.profraw` saat workload).
  - `llvm-profdata merge -o prog.profdata *.profraw` (langkah manual
    eksplisit — nama `.profraw` bervariasi per run, glob aman).
  - `nusa build prog.ns -o prog_opt --pgo-use=prog.profdata`
    (clang `-fprofile-use=`; juga bentuk `--pgo-use FILE`).
  - `nusa pkg build` meneruskan kedua flag (cermin `--stats`).
- Validasi dini (pre-build): kombinasi generate+use ditolak
  (eksklusif); profil hilang ditolak dengan arahan merge.

## 2. Cache safety (temuan desain)

Mode PGO ADALAH flag: `Fingerprint::pgo` =
`"" | "generate" | "use:<sha-profdata>"` (`compiler/incr/`,
`sameFlags`, serialisasi; lama tanpa-`pgo` tetap terbaca).
Tanpa ini biner instrumented/teroptimasi/base tertukar via
`Fresh (cached)` — diverifikasi negatif di `e2e_pgo` (kasus
`pgo-cache`). Isi profdata ikut sha: profil baru → rebuild.

## 3. Implikasi reproducibility (mandat)

- PGO MENGGUGURKAN jaminan reproducible-builds (§61) untuk artefak
  yang memakainya: `.profdata` bergantung workload, input, urutan
  scheduling, dan mesin (counter + layout). Dua mesin → dua
  `.profdata` → dua biner yang sah-berbeda.
- Aturan: rilis yang mengklaim reproducible (lockfile/pin) DILARANG
  memakai `--pgo-use`; PGO hanya untuk artefak performa-tertakar
  (`bench/suite.sh` + workload terkunci); simpan `.profdata` +
  perintah merge persis di catatan rilis bila dipakai.
- Determinisme compiler sendiri tak tersentuh (flag hanya
  diteruskan ke clang; tanpa PGO perilaku bit-identik).

## 4. Diverifikasi (Termux AArch64, clang/llvm-profdata 21.1.8)

- Loop penuh `fib(20)` → `6765` identik di ketiga biner
  (base/instrumented/optimized); `e2e_pgo` 4/4 (loop, missing,
  exclusive, cache); `test_incr` + `test_pkg` hijau; `e2e_release`
  hijau; `.ns` + IR OK.
- Ukuran `hello`: base 5816 B, instrumented 85424 B, PGO 5816 B
  (ilustrasi, bukan klaim speedup — workload mainan).
