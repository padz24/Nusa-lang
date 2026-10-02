// Analisis control-flow MIR — implementasi (Phase 102, milestone 1).
// Murni C++ standar. Deterministik: traversal mengikuti urutan block.
#include "compiler/mir/Cfg.h"

namespace nusa {
namespace mir {

std::vector<std::string> successors(const BasicBlock &bb) {
  if (bb.instrs.empty())
    return {};
  const Instr &last = bb.instrs.back();
  if (last.op == Op::Branch)
    return {last.target};
  if (last.op == Op::CondBranch)
    return {last.trueTarget, last.falseTarget};
  return {};
}

std::map<std::string, std::vector<std::string>>
predecessors(const MirFunction &fn) {
  std::map<std::string, std::vector<std::string>> pred;
  for (const auto &bb : fn.blocks)
    pred[bb.label]; // pastikan setiap block punya entri (bisa kosong)
  for (const auto &bb : fn.blocks) {
    for (const auto &s : successors(bb))
      pred[s].push_back(bb.label);
  }
  return pred;
}

// Indeks label -> posisi block (duplikat: kemunculan pertama menang;
// duplikat sendiri ditolak verifier).
static std::map<std::string, size_t> blockIndex(const MirFunction &fn) {
  std::map<std::string, size_t> idx;
  for (size_t i = 0; i < fn.blocks.size(); i++) {
    if (!idx.count(fn.blocks[i].label))
      idx[fn.blocks[i].label] = i;
  }
  return idx;
}

std::set<std::string> reachable(const MirFunction &fn) {
  std::set<std::string> seen;
  std::map<std::string, size_t> idx = blockIndex(fn);
  if (!idx.count(fn.entry))
    return seen;
  std::vector<std::string> stack;
  stack.push_back(fn.entry);
  seen.insert(fn.entry);
  while (!stack.empty()) {
    std::string cur = stack.back();
    stack.pop_back();
    auto it = idx.find(cur);
    if (it == idx.end())
      continue;
    for (const auto &s : successors(fn.blocks[it->second])) {
      // Hanya block yang ada (target dangling milik verifier).
      if (idx.count(s) && seen.insert(s).second)
        stack.push_back(s);
    }
  }
  return seen;
}

std::vector<std::string> rpo(const MirFunction &fn) {
  std::map<std::string, size_t> idx = blockIndex(fn);
  std::vector<std::string> post;
  std::set<std::string> visited;
  if (!idx.count(fn.entry))
    return post;
  // DFS iteratif eksplisit (hindari rekursi pada graf besar).
  std::vector<std::pair<std::string, size_t>> stack;
  stack.push_back({fn.entry, 0});
  visited.insert(fn.entry);
  while (!stack.empty()) {
    const std::string &cur = stack.back().first;
    size_t &next = stack.back().second;
    std::vector<std::string> succ;
    auto it = idx.find(cur);
    if (it != idx.end())
      succ = successors(fn.blocks[it->second]);
    if (next < succ.size()) {
      std::string s = succ[next++];
      if (idx.count(s) && visited.insert(s).second)
        stack.push_back({s, 0});
    } else {
      post.push_back(cur);
      stack.pop_back();
    }
  }
  return std::vector<std::string>(post.rbegin(), post.rend());
}

} // namespace mir
} // namespace nusa
