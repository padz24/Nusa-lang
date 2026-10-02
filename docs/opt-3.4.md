# Advanced Optimizer — Milestone 1: Inlining (Phase 103)

Status: slice implementasi. Mandat master Phase 103 (interprosedural,
inlining, constant propagation, DCE, loop, spesialisasi,
devirtualisasi, escape, alokasi; target 3.4.0; LLVM tetap optimizer
utama; tiap optimisasi ber-bench). NOL rewire pipeline.

## 1. Pemilihan milestone

- Propagasi konstanta: SUDAH ADA (peta produsen per fungsi di
  `foldConstants`, lintas-block).
- DCE/loop/escape/alokasi: DCE ada; loop butuh loop-analysis,
  escape butuh model memori formal — prasyarat belum ada.
- Devirtualisasi/spesialisasi: N/A (tanpa dynamic dispatch di MIR).
- Dipilih: **inlining** ("better inlining" eksplisit di mandat) —
  membuka fold/DCE lintas batas call; terukur via bench + unit.

## 2. Perubahan (`compiler/mir/Opt.{h,cpp}`)

- `inlineSmallCalls(fn, mod, stats)`: salin badan callee
  SATU-BLOCK ke situs call langsung. Syarat: callee se-modul,
  1 block, terminator `Return`, ≤ `kMaxInlineInstrs` (32),
  bukan caller (anti-rekursi langsung), arity cocok, void-ness
  cocok. Sekali jalan (bukan fixpoint) → terminasi terjamin.
- Kebenaran: id hasil di-remap ke id segar (cermin
  `MirBuilder::fresh`); param → argumen aktual; slot memori
  di-rename `.inlN` per situs (anti-tabrakan — BUG KELAS INI
  DICEGAH by design, dikunci `testInlineSlotRename`); hasil
  `ret` disubstitusi ke semua pemakai hasil call; urutan efek
  (print/store) lestari karena sisip-di-posisi.
- `OptStats::inlined`; dipanggil sekali di awal `optimizeModule`
  (sebelum pass per-fungsi) agar fold+DCE melipat lintas call.
- Batas jujur: callee multi-block, rekursi, callee raksasa, dan
  fungsi-mati-pasca-inline (dead-function elimination) =
  milestone berikut.

## 3. Diverifikasi

- Unit `test_mir` 16/16 (13 lama + 3 baru: `testInlineSmallCall`
  — call hilang + lipat jadi `print(30)`; `testInlineGuards` —
  rekursi/multi-block/raksasa lestari; `testInlineSlotRename`).
- `e2e_mir` 2/2; `e2e_opt` 4/4: bench sintetik 6002 → 2 instruksi
  (reduksi IDENTIK baseline — workload tanpa call, benar tak
  berubah); `e2e_release` 6/6; `.ns` + IR OK (MIR belum rewire).
- Bench waktu dalam varian perangkat (beban build paralel saat
  ukur); tidak diklaim sebagai speedup.

## 4. Ditunda jujur

- Inlining multi-block, heuristik budget adaptif, dead-function
  elimination, loop/escape/specialization (butuh analisis prasyarat).
