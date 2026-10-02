# Optimization Framework — Design (Phase 53, target 2.4.0)

Status: Proposed → Implementation (milestone 1: ConstFold + DCE).
LLVM tetap optimizer utama backend; pass Nusa jalan di MIR dan
semua punya benchmark (mandat master: jangan optimasi berdasar
asumsi). Pipeline `.ns` belum di-rewire (nol perilaku).

## 1. Antarmuka pass (kecil, seragam)

```cpp
// compiler/mir/Opt.h (nusa::mir) — murni C++ standar.
struct OptStats { int folded = 0; int eliminated = 0; };
bool foldConstants(MirFunction &fn, OptStats *stats = nullptr);
bool eliminateDeadCode(MirFunction &fn, OptStats *stats = nullptr);
int optimizeFunction(MirFunction &fn, OptStats *stats = nullptr);
int optimizeModule(MirModule &mod, OptStats *stats = nullptr);
```

- Tiap pass mengembalikan `changed`; `optimizeFunction` mengulang
  fold+DCE sampai fixpoint (batas iterasi = jumlah instruksi + 1).
- Pass tidak pernah mengubah semantik terobservasi: hanya rewrite
  yang terbukti aman (lihat §3). Hasil selalu lolos `verify()`.

## 2. Milestone 1 (SATU-SATUNYA perubahan perilaku-IR fase ini)

1. **Constant folding**: arith (`Add/Sub/Mul/Div`) dan cmp atas
   operand yang keduanya konstanta → satu `Const`. Div aman:
   pembagi nol dan `INT64_MIN / -1` TIDAK difold (tetap runtime,
   konsisten dengan trap div-zero Nusa).
2. **Dead code elimination**: hapus instruksi murni (const/arith/
   cmp) yang hasilnya tak dipakai siapa pun. calls, alloc/load/
   store, print, terminator = efek samping → selalu dipertahankan.
   Fixpoint (penghapusan berantai).

DITUNDA (butuh fondasi lebih dulu, masing-masing milestone sendiri):
dead store elimination (butuh analisis alias slot), block
simplification (butuh CFG: pred/succ + jump-threading), inlining
(butuh batas budget + rekrutmen id), escape analysis (butuh model
ownership — terkait Phase 54), specialization/CSE (butuh hashing
nilai + numbering).

## 3. Aturan keamanan (anti-UB C++)

- Aritmetika folding memakai `uint64_t` + cast (wrap terdefinisi),
  BUKAN `int64_t` langsung (signed overflow = UB).
- Perbandingan bertanda dihitung via `<`/`==` pada `int64_t`
  (terdefinisi untuk semua nilai; tanpa overflow).
- `Div` hanya difold bila aman (§2); `1/0` di MIR tetap instruksi
  (runtime Nusa yang trap, bukan folder).

## 4. Benchmark (mandat)

- `bench/mir_opt_main.cpp`: workload sintetik (rantai arith +
  dead code) → lapor instruksi sebelum/sesudah + waktu dinding.
- `compiler/tests/e2e_opt.sh`: kompilasi + jalankan bench,
  asersi reduksi + `verify()` pasca-opt + regresi `.ns`.
- Angka acuan dicatat di CHANGELOG (perangkat-spesifik, ulangi ≥3x).

## 5. Hasil milestone 1 (terukur, Termux AArch64)

- Workload `bench/mir_opt_main.cpp` (n=2000: rantai add + dead mul):
  6002 → 2 instruksi (folded=4000, eliminated=6000),
  `verify` pra/pasca = ok, ~20ms (3 run: 19.9/21.9/23.4ms).
- Temuan performa selama milestone: DCE `erase` satu-per-satu =
  O(n²) (~2s untuk 6002 instruksi) → filter partisi sekali jalan
  (~20ms, ~100x). Didokumentasikan agar pass berikut tidak
  mengulang pola yang sama.
