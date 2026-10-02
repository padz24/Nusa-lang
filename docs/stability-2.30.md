# Nusa 2.30.1 Stability Release — Design (Phase 80)

Status: implementasi slice ini. Mandat master: PHASE 80
(freeze fitur bahasa mayor; stabilitas, performa, keamanan,
kompatibilitas, dokumentasi; tanpa RFC khusus = tanpa fitur
besar). Spec bahasa TAK BERUBAH (nol sintaks/semantik baru).

## 1. Penomoran (keputusan tercatat)

Master menarget "2.5.0" di dua fase berbeda (stale — didahului
eksekusi 2.20→2.30). Rilis ini = **2.30.1** (patch: tanpa fitur;
monoton; aman untuk perbandingan versi pkg).

## 2. Stabilitas (harness)

- `e2e_eco2.sh`: `HOME=$TMPDIR/home` hermetik (cache
  `$HOME/.cache` bersama antar-suite paralel = flake
  tamper-verify; e2e lain tak menyentuh cache itu).
- `e2e_dist.sh`: sudah keras Phase 79 (bounded-wait, port
  dinamis, retry).
- Dibiarkan (proteksi hang disengaja): timeout ketat
  `e2e_control` (10s), tanpa-jitter retry.
- Fuzz seed-kedua (`NUSA_VERIFY_FUZZ_SEED`) satu sapuan.
  TEMUAN NYATA: seed 424242 menunjukkan divergensi O0/O2 palsu —
  generator meng-clobber countdown loop (non-terminasi; oracle
  membandingkan output terpotong-timeout). BUKAN miscompile.
  Perbaikan 2 lapis: counter loop masuk frozen (terminasi
  terbukti 6 seed x 40 program, 0 hang) + oracle gagal eksplisit
  pada rc=124. Fondasi Phase 71 bekerja sesuai tujuan.

## 3. Performa (baseline, bukan optimasi)

- Catat angka `bench.sh` + ukuran biner + waktu build
  `parse.ns` (korpus terbesar) sebagai baseline §6.
- Tanpa perubahan codegen (freeze).

## 4. Keamanan (audit, bukan fitur)

- Runtime C: tanpa strcpy/strcat/sprintf/gets (terverifikasi;
  pengecualian shim paket: strcpy ke malloc(strlen+1) —
  aman-by-construction, tercatat).
- Flag build Release + e2e_hardening/sec hijau (rerun).
- `extern` return-string tetap ditolak (kepemilikan memori).

## 5. Kompatibilitas

- `e2e_compat.sh` (baru): `check` SEMUA `.ns` repo
  (std/examples/tutorials/bench/selfhost/tests/packages;
  daftar eksplisit + ambang N) + `build` subset runnable.
  Negatif/berbeda-perilaku (mis. modul-tanpa-main) masuk
  daftar-terkendali, bukan PASS/FAIL buta.
- Artefak `.ns.ll` di pohon sumber dibersihkan (gitignored;
  pembuatnya = run in-place e2e_ecosystem — didokumentasikan,
  tak diubah perilakunya).

## 6. Baseline 2.30.1 (diukur 2026-09-12, Termux AArch64)

- `bench.sh` closureapply: build 2.06 s, exe 6776 B, run 0.17 s
  (deviasi antar-run wajar di perangkat yang sama).
- `build/nusa`: 1522248 B.
- Build `selfhost/parse.ns` (korpus terbesar, ~65 KB): 3.7 s wall.
- Full `ctest -j2`: 79/79 hijau (1149 dtk; diisi pasca-verifikasi final).

## 7. Dokumentasi

- `standard-library.md` header → v2.30.1; `compiler.md`
  catatan freeze; `check-links.py` hijau; website regen.
- File ini + CHANGELOG [2.30.1].

## 8. Acceptance criteria

- Nol perubahan parser/checker/codegen (dibuktikan diff
 listing); suite hijau (78+1); fuzz seed-2 hijau; baseline
  tercatat; `.ns.ll` bersih.
