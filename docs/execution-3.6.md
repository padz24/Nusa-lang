# Arsitektur Eksekusi — Milestone 1: Kapabilitas + Desain (Phase 105)

Status: slice arsitektur. Mandat master Phase 105 (arsitektur yang
memungkinkan AOT + JIT opsional + interpreter opsional; AOT tetap
default; target 3.6.0). NOL penggantian compiler; NOL API produk baru.

## 1. Hari ini (AOT — satu-satunya jalur produk)

`checkStages` → `Codegen` → `.ns.ll` → `clang` → biner natif
(`compiler/driver/Driver.cpp::runBuild`). AOT default ditegakkan
oleh tidak adanya jalur lain — diverifikasi `e2e_release` hijau
pasca-slice (NOL perilaku).

## 2. Bukti kelayakan JIT (eksperimen, bukan klaim)

- Spike `/tmp` (tak di-commit): modul IRT `main → puts` via
  IRBuilder + ORC LLJIT in-process + `DynamicLibrarySearchGenerator`
  proses → `jit-ok`, rc=0, Termux AArch64/Bionic, LLVM 21.1.8.
- Dikunci repo sebagai `compiler/tests/test_orc.cpp` (ctest
  `test_orc`): bangun `forty_two() → 42` + `main` memanggilnya +
  `puts`, assert rc JIT == 0. Pagar toolchain: bila Termux/LLVM
  berubah merusak ORC, test ini merah SEBELUM subsystem dibangun.

## 3. Desain penyisipan (untuk Phase 106+/subsystem)

- Seam: sesudah `Codegen::emitIRToFile` (modul LLVM in-memory,
  bukan `.ll` file) → sesi ORC per-proses → lookup `main` →
  panggil. Driver tetap pemilik argv/exit-code.
- Simbol runtime: hari ini `nusa_rt.o` di-link clang. JIT butuh
  `nusa_rt` terlihat in-process — opsi: (a) muat `.o`/`.so`
  runtime via `DynamicLibrarySearchGenerator::Load`, (b) tautkan
  simbol ke biner `nusa` dan expose. Keduanya butuh uji Bionic
  lanjutan (di luar milestone ini).
- State inkremental (syarat REPL): sesi ORC hidup + tambah modul
  per baris + simbol persisten — API subsystem, bukan spike ini.
- Interpreter: DITUNDA eksplisit — semantik kedua = beban ganda
  (desain + uji + drift); kebutuhan (REPL/iterasi) dipenuhi JIT
  atau AOT-server bila JIT terbukti berat.

## 4. Keputusan milestone

- AOT default: YA (tak tersentuh, terverifikasi).
- Subsystem JIT: BELUM (desain + pagar di slice ini; implementasi
  mengikuti kebutuhan REPL Phase 106).
- Interpreter: TIDAK (ditunda beralasan, bukan ditolak permanen).

## 5. Diverifikasi

- `test_orc` hijau; full build `-j2` bersih; `e2e_release` 6/6
  hijau (NOL perilaku produk); website regen + no-drift.
