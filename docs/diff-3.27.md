# Differential Compiler Testing — Milestone 1: Matriks Opt (Phase 126)

Status: slice implementasi (target 3.27.0). Mandat master:
bandingkan debug/release/optimized build, level LLVM berbeda,
target berbeda; hasil program memenuhi semantik sama.

## 1. Cakupan (tanpa duplikat e2e_verify)

- e2e_verify (ada): O0≡O2 + run≡build+exec + fuzz diferensial.
- BARU `e2e_diff.sh`: 5 program bench × 10 konfigurasi
  (default, -O0/-O1/-O2/-O3/-Os/-Oz, -g, --profile release/debug)
  → stdout + exit code IDENTIK per program (50 build + 55 run).
- Interpretasi jujur "debug vs release build": level PROGRAM
  (-g vs polos, profil dev/debug/release) — BUKAN rebuild
  compiler Debug/Release (biaya penuh tanpa sinyal tambahan;
  ASan/UBSan sudah dipakai di unit terpilih).

## 2. Batas jujur

- Target berbeda: run lintas-target TAK MUNGKIN (biner asing
  tak dieksekusi di host) — diferensial run = host-only;
  lintas-target dikunci di level IR/triple (`e2e_cross`,
  `e2e_abi`), dinyatakan eksplisit.
- Matriks tak mencakup `-O` + `-g` kombinatorial penuh
  (ledakan kombinatorial; sampel mewakili tiap sumbu).
- Divergensi numerik float: N/A (tanpa float di bahasa).

## 3. TEMUAN: false-Fresh lintas-output (ditutup slice ini)

Matriks MENEMUKAN bug nyata (bukan flake): `diff-loopsum` gagal
dengan biner FIB basi. Akar: sidik inkremental per-direktori
(TANPA identitas output) — `base` + `cfg0` (sumber+flag sama)
berbagi `fingerprint`; `cfg0` dinyatakan Fresh dari sidik `base`
+ file basi warisan iterasi fib → biner salah dieksekusi.
Skenario pengguna nyata: dua output dari satu sumber+flag se-dir.
Perbaikan: field `Fingerprint::out` (+ serialisasi + sameFlags);
migrasi aman (sidik lama tanpa-`out` = rebuild sekali).
Regresi: kasus ini (base+cfg0 flag-identik) + `e2e_incr` utuh.

## 4. Diverifikasi

- `e2e_diff` 5/5; `e2e_verify` + `e2e_release` hijau.

## 4. Ditunda jujur

- Rebuild-compiler differential, target-run via emulator/QEMU,
  superoptimizer/translation validation.
