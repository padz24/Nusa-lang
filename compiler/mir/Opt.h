#pragma once
// Pass optimisasi MIR — milestone 1 (Phase 53): constant folding +
// dead code elimination; milestone 2 (Phase 102): eliminasi block
// tak-terjangkau (di atas analisis Cfg). Murni C++ standar (tanpa
// compiler/* lain, tanpa LLVM). Semantik terobservasi tidak berubah
// (lih. docs/opt-2.4.md, docs/ir-3.3.md).
#include "compiler/mir/Mir.h"

namespace nusa {
namespace mir {

struct OptStats {
  int folded = 0;
  int eliminated = 0;
  int blocksRemoved = 0; // Phase 102: block tak-terjangkau terbuang
  int inlined = 0;       // Phase 103: call tersalin badan callee
};

// Ambang ukuran callee yang boleh di-inline (jumlah instruksi).
// Di atas ini: panggil biasa (hemat ukuran kode + waktu kompilasi).
constexpr size_t kMaxInlineInstrs = 32;

// Lipat arith/cmp beroperand-konstanta menjadi satu Const.
// Div aman: pembagi nol dan INT64_MIN/-1 TIDAK difold.
bool foldConstants(MirFunction &fn, OptStats *stats = nullptr);

// Hapus instruksi murni (const/arith/cmp) yang hasilnya tak dipakai.
// Call/alloc/load/store/print/terminator dipertahankan.
bool eliminateDeadCode(MirFunction &fn, OptStats *stats = nullptr);

// Hapus block yang tak terjangkau dari entry (analisis Cfg; entry
// hilang -> diam). Mengembalikan true bila ada block terbuang.
bool eliminateUnreachableBlocks(MirFunction &fn, OptStats *stats = nullptr);

// Salin badan callee satu-block ke tiap situs call langsung.
// Syarat callee: ada di modul, satu block, terminator = Return,
// ≤ kMaxInlineInstrs instruksi, bukan caller itu sendiri (anti
// rekursi langsung). Slot memori di-rename (".inlN") agar tak
// tabrakan dengan slot caller. Sekali jalan (bukan fixpoint):
// terminasi terjamin. Prasyarat: modul lolos verify.
bool inlineSmallCalls(MirFunction &fn, const MirModule &mod,
                      OptStats *stats = nullptr);

// Fixpoint fold+DCE per fungsi; mengembalikan total perubahan.
int optimizeFunction(MirFunction &fn, OptStats *stats = nullptr);
int optimizeModule(MirModule &mod, OptStats *stats = nullptr);

} // namespace mir
} // namespace nusa
