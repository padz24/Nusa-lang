# Nusa IR — Design (Phase 52, target 2.3.0)

Status: Proposed → Implementation (milestone 1: fondasi MIR).
Pipeline belum di-rewire (codegen tetap AST → LLVM; nol perilaku).

## 1. Mengapa (mandat master)

Tidak semua lowering/semantik boleh bergantung langsung pada LLVM:
optimisasi Nusa, analisis, dan verifikasi butuh representasi internal
yang stabil, kecil, dan bebas header LLVM.

```
Nusa Source → AST → HIR → Nusa MIR → LLVM IR → LLVM 21.1.8 → Target
```

HIR (desugar tingkat tinggi: match/enum/closure/trait-rewrite) =
milestone berikut. Phase 52 milestone 1 = **MIR fondasi**:
model data + builder + verifier + printer teks.

## 2. Model MIR (mencerminkan kebutuhan codegen saat ini)

Codegen hari ini mengenal nilai Int/Bool/String/Void/Array/Struct/
Func, alloca di entry, GEP+call, cabang then/else/merge dan
loop cond/body/after. MIR memodelkan concepts yang sama secara
eksplisit:

- Typed values: `%id : type` (SSA-seperti) + konstanta.
  `MirType = I64 | Bool | Str | Void | Ptr | Func`.
- Function calls: `Call` (callee global/builtin + argumen).
- Memory operations: `Alloc` (slot bernama), `Load`, `Store`.
- Branches & basic blocks: `Branch`, `CondBranch`, `Return`,
  `Unreachable`; tiap block tepat satu terminator di akhir.
- Control flow: graf block + entry; if/while lowering = pola
  cond → then/else/merge yang sudah dipakai codegen.
- Optimization passes (Phase 53): disiapkan via struktur data
  yang dapat diinspeksi/ditulis ulang (instr list per block);
  milestone ini BELUM berisi pass.

## 3. Batas lapisan (melanjutkan Phase 51)

- `compiler/mir/` hanya boleh include header standar C++.
  Dilarang include ast/semantic/codegen/LLVM (dikunci `e2e_mir.sh`).
- Namespace `nusa::mir`. Tanpa dependensi ke pipeline existing →
  risiko nol terhadap program `.ns` yang ada.

## 4. Acceptance criteria milestone 1

- `MirBuilder` dapat membangun fungsi dengan cabang + loop +
  call + alloc/load/store; `verify()` menerima.
- `verify()` menolak: block tanpa terminator, terminator di tengah,
  operand tak-terdefinisi, target cabang tak-ada, call ke fungsi
  tak-dideklarasikan (kecuali nama builtin allowlist).
- `dump()` deterministik (golden test).
- Unit `test_mir` + `e2e_mir.sh` hijau; seluruh suite hijau;
  benchmark identik baseline 2.2.0.

## 5. Keputusan implementasi (temuan selama milestone)

- Referensi `MirFunction`/`BasicBlock` stabil via `std::deque`
  (aman dipegang lintas `addBlock`/`newFunction` susulan).
- Counter id nilai hidup di `MirFunction.nextValueId` (bukan vector
  paralel di builder) + lazy-init dari isi — builder baru di atas
  modul tak-kosong tetap benar. ASan menemukan OOB pada desain
  lama; dikunci regresi `testFreshBuilder`; ASan+UBSan bersih.
- Verifier menerima allowlist nama builtin (bukan include
  `frontend/`) agar `mir/` tetap tanpa dependensi compiler/*.
