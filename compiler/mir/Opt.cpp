// Pass optimisasi MIR — implementasi milestone 1 (Phase 53) +
// eliminasi block tak-terjangkau (Phase 102, milestone 1).
// Hanya header standar C++. Aritmetika folding memakai uint64_t
// (wrap terdefinisi) — signed overflow C++ adalah UB.
#include "compiler/mir/Opt.h"

#include "compiler/mir/Cfg.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace nusa {
namespace mir {
namespace {

bool isPure(Op op) {
  switch (op) {
  case Op::ConstI64:
  case Op::ConstBool:
  case Op::ConstStr:
  case Op::Add:
  case Op::Sub:
  case Op::Mul:
  case Op::Div:
  case Op::CmpLt:
  case Op::CmpGt:
  case Op::CmpLtEq:
  case Op::CmpGtEq:
  case Op::CmpEq:
  case Op::CmpNotEq:
    return true;
  default:
    return false;
  }
}

// Cari produsen Const untuk id (peta dibangun per fungsi).
const Instr *findConst(const std::unordered_map<uint32_t, const Instr *> &prods,
                       uint32_t id, Op want) {
  auto it = prods.find(id);
  if (it == prods.end() || it->second->op != want)
    return nullptr;
  return it->second;
}

} // namespace

bool foldConstants(MirFunction &fn, OptStats *stats) {
  bool changed = false;
  // Produsen hasil per id (terakhir menang; id unik per verify).
  std::unordered_map<uint32_t, const Instr *> prods;
  for (const auto &bb : fn.blocks) {
    for (const auto &in : bb.instrs) {
      if (in.hasResult)
        prods[in.result] = &in;
    }
  }
  auto constI64 = [&](uint32_t id, int64_t &out) -> bool {
    const Instr *p = findConst(prods, id, Op::ConstI64);
    if (!p)
      return false;
    out = p->intVal;
    return true;
  };
  auto constBool = [&](uint32_t id, bool &out) -> bool {
    const Instr *p = findConst(prods, id, Op::ConstBool);
    if (!p)
      return false;
    out = p->boolVal;
    return true;
  };

  for (auto &bb : fn.blocks) {
    for (auto &in : bb.instrs) {
      if (in.op != Op::Add && in.op != Op::Sub && in.op != Op::Mul &&
          in.op != Op::Div && in.op != Op::CmpLt && in.op != Op::CmpGt &&
          in.op != Op::CmpLtEq && in.op != Op::CmpGtEq &&
          in.op != Op::CmpEq && in.op != Op::CmpNotEq)
        continue;
      if (in.operands.size() != 2)
        continue;
      int64_t a = 0, b = 0;
      bool av = constI64(in.operands[0], a);
      bool bv = constI64(in.operands[1], b);
      bool folded = false;
      if (av && bv) {
        uint64_t ua = static_cast<uint64_t>(a);
        uint64_t ub = static_cast<uint64_t>(b);
        switch (in.op) {
        case Op::Add:
          in.op = Op::ConstI64;
          in.intVal = static_cast<int64_t>(ua + ub);
          folded = true;
          break;
        case Op::Sub:
          in.op = Op::ConstI64;
          in.intVal = static_cast<int64_t>(ua - ub);
          folded = true;
          break;
        case Op::Mul:
          in.op = Op::ConstI64;
          in.intVal = static_cast<int64_t>(ua * ub);
          folded = true;
          break;
        case Op::Div:
          // Aman saja: pembagi nol dan INT64_MIN/-1 tetap runtime.
          if (b != 0 && !(a == INT64_MIN && b == -1)) {
            in.op = Op::ConstI64;
            in.intVal = a / b;
            folded = true;
          }
          break;
        case Op::CmpLt:
          in.op = Op::ConstBool;
          in.resultType = MirType::boolean();
          in.boolVal = a < b;
          folded = true;
          break;
        case Op::CmpGt:
          in.op = Op::ConstBool;
          in.resultType = MirType::boolean();
          in.boolVal = a > b;
          folded = true;
          break;
        case Op::CmpLtEq:
          in.op = Op::ConstBool;
          in.resultType = MirType::boolean();
          in.boolVal = a <= b;
          folded = true;
          break;
        case Op::CmpGtEq:
          in.op = Op::ConstBool;
          in.resultType = MirType::boolean();
          in.boolVal = a >= b;
          folded = true;
          break;
        case Op::CmpEq:
          in.op = Op::ConstBool;
          in.resultType = MirType::boolean();
          in.boolVal = a == b;
          folded = true;
          break;
        case Op::CmpNotEq:
          in.op = Op::ConstBool;
          in.resultType = MirType::boolean();
          in.boolVal = a != b;
          folded = true;
          break;
        default:
          break;
        }
      } else if (in.op == Op::CmpEq || in.op == Op::CmpNotEq) {
        // Eq/NotEq atas bool konstanta (operand non-i64).
        bool x = false, y = false;
        if (constBool(in.operands[0], x) && constBool(in.operands[1], y)) {
          in.resultType = MirType::boolean();
          if (in.op == Op::CmpEq) {
            in.op = Op::ConstBool;
            in.boolVal = x == y;
          } else {
            in.op = Op::ConstBool;
            in.boolVal = x != y;
          }
          folded = true;
        }
      }
      if (folded) {
        in.operands.clear();
        in.callee.clear();
        in.slot.clear();
        in.target.clear();
        in.trueTarget.clear();
        in.falseTarget.clear();
        changed = true;
        if (stats)
          stats->folded++;
        // Perbarui peta agar rantai dalam block yang sama ikut terlipat.
        prods[in.result] = &in;
      }
    }
  }
  return changed;
}

bool eliminateDeadCode(MirFunction &fn, OptStats *stats) {
  bool changed = false;
  // Kumpulkan id yang dipakai (semua operand instruksi yang bertahan).
  for (;;) {
    std::unordered_set<uint32_t> used;
    for (const auto &bb : fn.blocks) {
      for (const auto &in : bb.instrs) {
        for (uint32_t op : in.operands)
          used.insert(op);
      }
    }
    bool removed = false;
    for (auto &bb : fn.blocks) {
      // Filter partisi sekali jalan (bukan erase satu-per-satu O(n²)).
      size_t keep = 0;
      for (size_t i = 0; i < bb.instrs.size(); i++) {
        const Instr &in = bb.instrs[i];
        if (in.hasResult && isPure(in.op) && !used.count(in.result)) {
          removed = true;
          changed = true;
          if (stats)
            stats->eliminated++;
        } else {
          if (keep != i)
            bb.instrs[keep] = std::move(bb.instrs[i]);
          keep++;
        }
      }
      bb.instrs.resize(keep);
    }
    if (!removed)
      break;
  }
  return changed;
}

int optimizeFunction(MirFunction &fn, OptStats *stats) {
  int total = 0;
  // Sekali jalan: buang block tak-terjangkau dulu agar pass berikut
  // bekerja di atas CFG minimal (aman: block terbuang tak dirujuk
  // block hidup mana pun — menurut definisi tak-terjangkau).
  if (eliminateUnreachableBlocks(fn, stats))
    total++;
  // Batas fixpoint: tiap iterasi menghapus/melipat ≥1 instruksi.
  size_t bound = 0;
  for (const auto &bb : fn.blocks)
    bound += bb.instrs.size();
  for (size_t i = 0; i <= bound; i++) {
    bool c1 = foldConstants(fn, stats);
    bool c2 = eliminateDeadCode(fn, stats);
    if (c1 || c2)
      total++;
    else
      break;
  }
  return total;
}

int optimizeModule(MirModule &mod, OptStats *stats) {
  int total = 0;
  // Sekali jalan sebelum pass per-fungsi: buka peluang fold/DCE
  // lintas batas call (terminasi terjamin — bukan fixpoint).
  for (auto &fn : mod.functions) {
    if (inlineSmallCalls(fn, mod, stats))
      total++;
  }
  for (auto &fn : mod.functions)
    total += optimizeFunction(fn, stats);
  return total;
}

// Hapus block yang tak terjangkau dari entry (analisis Cfg). Entry
// hilang (verifier yang melapor) -> diam (tanpa penghapusan).
bool eliminateUnreachableBlocks(MirFunction &fn, OptStats *stats) {
  bool hasEntry = false;
  for (const auto &bb : fn.blocks) {
    if (bb.label == fn.entry) {
      hasEntry = true;
      break;
    }
  }
  if (!hasEntry)
    return false;
  std::set<std::string> live = reachable(fn);
  size_t keep = 0;
  bool changed = false;
  for (size_t i = 0; i < fn.blocks.size(); i++) {
    if (live.count(fn.blocks[i].label)) {
      if (keep != i)
        fn.blocks[keep] = std::move(fn.blocks[i]);
      keep++;
    } else {
      changed = true;
      if (stats)
        stats->blocksRemoved++;
    }
  }
  fn.blocks.erase(fn.blocks.begin() + static_cast<ptrdiff_t>(keep),
                  fn.blocks.end());
  return changed;
}

// Id pemakaian maksimum + 1 (cermin lazy-init MirBuilder::fresh agar
// id salinan tak tabrakan; nextValueId caller diselaraskan).
static uint32_t maxUsedId(const MirFunction &fn) {
  uint32_t m = static_cast<uint32_t>(fn.params.size());
  for (const auto &bb : fn.blocks) {
    for (const auto &in : bb.instrs) {
      if (in.hasResult && in.result >= m)
        m = in.result + 1;
      for (uint32_t op : in.operands) {
        if (op >= m)
          m = op + 1;
      }
    }
  }
  return m;
}

bool inlineSmallCalls(MirFunction &fn, const MirModule &mod,
                      OptStats *stats) {
  bool changed = false;
  int siteNo = 0; // suffix rename slot per situs (deterministik)
  for (auto &bb : fn.blocks) {
    for (size_t i = 0; i < bb.instrs.size(); i++) {
      Instr &call = bb.instrs[i];
      if (call.op != Op::Call)
        continue;
      // Cari callee se-modul (builtin/asing dilewati).
      const MirFunction *callee = nullptr;
      for (const auto &cand : mod.functions) {
        if (cand.name == call.callee) {
          callee = &cand;
          break;
        }
      }
      if (!callee || callee->name == fn.name)
        continue; // asing atau rekursi langsung
      if (callee->blocks.size() != 1)
        continue; // multi-block = milestone berikut
      const BasicBlock &cb = callee->blocks.front();
      if (cb.instrs.empty() || cb.instrs.back().op != Op::Return)
        continue;
      if (cb.instrs.size() > kMaxInlineInstrs)
        continue;
      if (callee->params.size() != call.operands.size())
        continue;
      bool calleeVoid =
          callee->ret == MirType::void_() && cb.instrs.back().operands.empty();
      if (!calleeVoid && !call.hasResult)
        continue; // hasil callee terbuang = bentuk invalid
      if (calleeVoid && call.hasResult)
        continue;
      // Return callee: teruskan nilai ke hasil call.
      const Instr &ret = cb.instrs.back();
      if (!calleeVoid && ret.operands.empty())
        continue; // bentuk invalid (verifier menolak) — lewati
      // Petakan id callee -> id caller (param -> argumen aktual).
      uint32_t next = fn.nextValueId;
      uint32_t used = maxUsedId(fn);
      if (next < used)
        next = used;
      std::unordered_map<uint32_t, uint32_t> remap;
      for (size_t p = 0; p < callee->params.size(); p++)
        remap[static_cast<uint32_t>(p)] = call.operands[p];
      // Dua pass agar acuan-maju di badan callee ikut terpetakan.
      for (size_t k = 0; k + 1 < cb.instrs.size(); k++) {
        const Instr &src = cb.instrs[k];
        if (src.hasResult && !remap.count(src.result))
          remap[src.result] = next++;
      }
      std::string suffix = ".inl" + std::to_string(siteNo);
      std::vector<Instr> body;
      for (size_t k = 0; k + 1 < cb.instrs.size(); k++) {
        const Instr &src = cb.instrs[k];
        Instr cp = src;
        if (cp.hasResult)
          cp.result = remap[cp.result];
        for (uint32_t &op : cp.operands) {
          auto it = remap.find(op);
          if (it != remap.end())
            op = it->second;
        }
        if (!cp.slot.empty())
          cp.slot += suffix;
        body.push_back(std::move(cp));
      }
      if (!calleeVoid) {
        uint32_t v = ret.operands.front();
        auto it = remap.find(v);
        if (it != remap.end())
          v = it->second;
        uint32_t want = call.result;
        for (auto &obb : fn.blocks) {
          for (auto &oin : obb.instrs) {
            for (uint32_t &op : oin.operands) {
              if (op == want)
                op = v;
            }
          }
        }
      }
      // Sisip badan di posisi call, lalu buang call.
      bb.instrs.erase(bb.instrs.begin() + static_cast<ptrdiff_t>(i));
      bb.instrs.insert(bb.instrs.begin() + static_cast<ptrdiff_t>(i),
                       std::make_move_iterator(body.begin()),
                       std::make_move_iterator(body.end()));
      fn.nextValueId = next;
      siteNo++;
      changed = true;
      if (stats)
        stats->inlined++;
      // Lanjut dari sesudah badan tersisip (instruksi salinan tak
      // mengandung Call yang perlu diproses ulang di sini — sekali jalan).
      i += body.size();
      if (i > 0)
        i--; // for-loop ++ mendarat tepat sesudah badan
    }
  }
  return changed;
}

} // namespace mir
} // namespace nusa
