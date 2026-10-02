// Benchmark pass optimisasi MIR (Phase 53): workload sintetik —
// rantai arith terlipat + dead code. Lapor instruksi sebelum/sesudah
// + waktu dinding. Bukan bagian lib (dikompilasi e2e_opt.sh).
#include "compiler/mir/Mir.h"
#include "compiler/mir/Opt.h"

#include <chrono>
#include <iostream>

using namespace nusa;
using namespace nusa::mir;

static size_t countInstrs(const MirModule &mod) {
  size_t n = 0;
  for (const auto &fn : mod.functions)
    for (const auto &bb : fn.blocks)
      n += bb.instrs.size();
  return n;
}

int main(int argc, char **argv) {
  int n = argc > 1 ? std::atoi(argv[1]) : 2000;
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f = b.newFunction("work", {}, MirType::i64(), "entry");
  BasicBlock &e = b.addBlock(f, "entry");
  Value acc = b.constI64(f, e, 0);
  for (int i = 0; i < n; i++) {
    Value k = b.constI64(f, e, i);
    acc = b.add(f, e, acc, k);
    Value dead = b.mul(f, e, k, k); // mati tiap iterasi
    (void)dead;
  }
  b.ret(f, e, acc);
  size_t before = countInstrs(mod);
  auto t0 = std::chrono::steady_clock::now();
  OptStats st;
  std::vector<VerifyError> errors;
  bool okPre = verify(mod, errors);
  optimizeModule(mod, &st);
  bool okPost = verify(mod, errors);
  auto t1 = std::chrono::steady_clock::now();
  size_t after = countInstrs(mod);
  auto us =
      std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
  std::cout << "instrs_before=" << before << "\n";
  std::cout << "instrs_after=" << after << "\n";
  std::cout << "folded=" << st.folded << "\n";
  std::cout << "eliminated=" << st.eliminated << "\n";
  std::cout << "verify_pre=" << okPre << " verify_post=" << okPost << "\n";
  std::cout << "opt_us=" << us << "\n";
  return (okPre && okPost && after < before) ? 0 : 1;
}
