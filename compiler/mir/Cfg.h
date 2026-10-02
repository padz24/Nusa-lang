#pragma once
// Analisis control-flow MIR — milestone 1 (Phase 102, NUSA IR 2.0).
//
// Cakupan: penerus/pendahulu per block, himpunan terjangkau dari entry,
// urutan reverse-postorder. Murni C++ standar (tanpa compiler/* lain,
// tanpa LLVM) — satu lapisan dengan Mir.h/Opt.h. Deterministik:
// traversal mengikuti urutan block fungsi.
//
// Bukan: analisis data-flow/lifetime/escape/alias (milestone berikut);
// verifikasi reachable (verifier tetap strict-lokal sejak Phase 52).
#include "compiler/mir/Mir.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace nusa {
namespace mir {

// Penerus langsung suatu block: Branch -> {target},
// CondBranch -> {trueTarget, falseTarget}, lainnya -> {}.
// (Terminator diasumsikan terakhir; non-terminator tak punya penerus.)
std::vector<std::string> successors(const BasicBlock &bb);

// Label -> daftar pendahulu (urutan kemunculan, deterministik).
// Target ke label yang tak ada ikut dicatat (terlihat saat verifikasi).
std::map<std::string, std::vector<std::string>> predecessors(const MirFunction &fn);

// Himpunan label yang terjangkau dari entry (BFS). Entry yang hilang
// (tak ada block berlabel entry) -> himpunan kosong; verifier yang
// melaporkannya, bukan CFG.
std::set<std::string> reachable(const MirFunction &fn);

// Urutan reverse-postorder dari entry (DFS iteratif, deterministik).
// Hanya block terjangkau; entry hilang -> kosong.
std::vector<std::string> rpo(const MirFunction &fn);

} // namespace mir
} // namespace nusa
