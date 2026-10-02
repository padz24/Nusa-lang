// Unit test MIR fondasi v2.3.0 (Phase 52): builder + verifier + dump.
// Pass optimisasi v2.4.0 (Phase 53): ConstFold + DCE.
// Analisis control-flow v3.2 (Phase 102, milestone 1): Cfg + eliminasi
// block tak-terjangkau.
#include "compiler/mir/Cfg.h"
#include "compiler/mir/Mir.h"
#include "compiler/mir/Opt.h"
#include <iostream>

using namespace nusa;
using namespace nusa::mir;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

// Program: fn add(a: i64, b: i64) -> i64 { return a + b; }
//          fn main() { let x = add(20, 10); print(x); }
static MirModule buildOk() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &add =
      b.newFunction("add", {{"a", MirType::i64()}, {"b", MirType::i64()}},
                    MirType::i64(), "entry");
  BasicBlock &ae = b.addBlock(add, "entry");
  Value s = b.add(add, ae, Value{0, MirType::i64()}, Value{1, MirType::i64()});
  b.ret(add, ae, s);
  MirFunction &main =
      b.newFunction("main", {}, MirType::void_(), "entry");
  BasicBlock &me = b.addBlock(main, "entry");
  Value twenty = b.constI64(main, me, 20);
  Value ten = b.constI64(main, me, 10);
  Value x = b.call(main, me, "add", {twenty, ten}, MirType::i64());
  b.print(me, x);
  b.ret(main, me);
  return mod;
}

static int testBuildAndVerify() {
  MirModule mod = buildOk();
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  CHECK(errors.empty());
  std::cout << "testBuildAndVerify OK\n";
  return 0;
}

static int testDumpGolden() {
  MirModule mod = buildOk();
  std::string d1 = dump(mod);
  std::string d2 = dump(mod);
  CHECK(d1 == d2); // deterministik
  CHECK(d1.find("func @add(%0: i64, %1: i64) -> i64") != std::string::npos);
  CHECK(d1.find("func @main() -> void") != std::string::npos);
  CHECK(d1.find("entry:") != std::string::npos);
  CHECK(d1.find("add %") != std::string::npos);
  CHECK(d1.find("call @add") != std::string::npos);
  std::cout << "testDumpGolden OK\n";
  return 0;
}

static int testControlFlow() {
  // if/while shape: cond -> then/else -> merge; loop cond/body/after.
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f =
      b.newFunction("f", {{"c", MirType::boolean()}}, MirType::i64(), "cond");
  BasicBlock &cond = b.addBlock(f, "cond");
  BasicBlock &thenB = b.addBlock(f, "then");
  BasicBlock &elseB = b.addBlock(f, "else");
  BasicBlock &merge = b.addBlock(f, "merge");
  b.condBr(cond, Value{0, MirType::boolean()}, "then", "else");
  Value one = b.constI64(f, thenB, 1);
  b.store(thenB, "r", one);
  b.br(thenB, "merge");
  Value zero = b.constI64(f, elseB, 0);
  b.store(elseB, "r", zero);
  b.br(elseB, "merge");
  Value r = b.load(f, merge, "r", MirType::i64());
  b.ret(f, merge, r);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  std::cout << "testControlFlow OK\n";
  return 0;
}

static int testRejects() {
  // 1. Block tanpa terminator.
  {
    MirModule mod;
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    b.constI64(f, e, 1);
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
    CHECK(!errors.empty());
  }
  // 2. Terminator di tengah.
  {
    MirModule mod;
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    b.br(e, "entry");
    b.unreachable(e);
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
  }
  // 3. Operand tak-terdefinisi.
  {
    MirModule mod;
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    b.print(e, Value{99, MirType::i64()});
    b.ret(f, e);
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
  }
  // 4. Target cabang tak-ada + callee tak-dikenal.
  {
    MirModule mod;
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    b.br(e, "nowhere");
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
  }
  {
    MirModule mod;
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    b.call(f, e, "ghost", {}, MirType::void_());
    b.ret(f, e);
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
    // Allowlist builtin meloloskan callee eksternal.
    errors.clear();
    CHECK(verify(mod, errors, {"ghost"}));
  }
  // 5. Arity + tipe return call salah.
  {
    MirModule mod = buildOk();
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("g", {}, MirType::void_(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    Value v = b.constI64(f, e, 1);
    b.call(f, e, "add", {v}, MirType::i64()); // kurang 1 arg
    b.ret(f, e);
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
  }
  // 6. Return tak-cocok + arith campur tipe.
  {
    MirModule mod;
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("f", {}, MirType::i64(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    b.ret(f, e); // hilang nilai
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
  }
  {
    MirModule mod;
    MirBuilder b(mod);
    MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
    BasicBlock &e = b.addBlock(f, "entry");
    Value a = b.constI64(f, e, 1);
    Value t = b.constBool(f, e, true);
    b.add(f, e, a, t);
    b.ret(f, e);
    std::vector<VerifyError> errors;
    CHECK(!verify(mod, errors));
  }
  std::cout << "testRejects OK\n";
  return 0;
}

// Regresi heap-buffer-overflow (ditemukan ASan): builder BARU di atas
// modul tak-kosong harus melanjutkan counter, bukan mengulang dari 0.
static int testFreshBuilder() {
  MirModule mod = buildOk(); // 2 fungsi via builder internal
  MirBuilder b(mod);         // builder baru, modul tak-kosong
  MirFunction &g = b.newFunction("g", {}, MirType::i64(), "entry");
  BasicBlock &e = b.addBlock(g, "entry");
  Value v = b.constI64(g, e, 7);
  b.ret(g, e, v);
  // Tambah block ke fungsi lama via builder baru (id harus unik).
  MirFunction &add = mod.functions[0];
  BasicBlock &extra = b.addBlock(add, "extra");
  b.unreachable(extra);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  CHECK(errors.empty());
  std::cout << "testFreshBuilder OK\n";
  return 0;
}

// Pass optimisasi (Phase 53) — di bawah ini.

static int testFoldArith() {
  // (20 + 10) * 2 - 5 = 55, dilipat berantai dalam satu fixpoint.
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f = b.newFunction("f", {}, MirType::i64(), "entry");
  BasicBlock &e = b.addBlock(f, "entry");
  Value a = b.constI64(f, e, 20);
  Value c = b.constI64(f, e, 10);
  Value s = b.add(f, e, a, c);
  Value two = b.constI64(f, e, 2);
  Value m = b.mul(f, e, s, two);
  Value five = b.constI64(f, e, 5);
  Value r = b.sub(f, e, m, five);
  b.ret(f, e, r);
  OptStats st;
  CHECK(optimizeFunction(f, &st));
  CHECK(st.folded >= 3);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  // Hasil akhir = satu Const 55 yang di-return.
  const Instr *retIn = nullptr;
  for (const auto &in : e.instrs) {
    if (in.op == Op::Return)
      retIn = &in;
  }
  CHECK(retIn != nullptr);
  CHECK(retIn->operands.size() == 1);
  uint32_t rid = retIn->operands[0];
  bool found = false;
  for (const auto &in : e.instrs) {
    if (in.hasResult && in.result == rid && in.op == Op::ConstI64 &&
        in.intVal == 55)
      found = true;
  }
  CHECK(found);
  std::cout << "testFoldArith OK\n";
  return 0;
}

static int testFoldCmp() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f = b.newFunction("f", {}, MirType::boolean(), "entry");
  BasicBlock &e = b.addBlock(f, "entry");
  Value a = b.constI64(f, e, 3);
  Value c = b.constI64(f, e, 7);
  Value lt = b.cmp(f, e, Op::CmpLt, a, c);   // true
  Value t = b.constBool(f, e, true);
  Value u = b.constBool(f, e, true);
  Value eq = b.cmp(f, e, Op::CmpEq, t, u);   // true (bool)
  Value both = b.cmp(f, e, Op::CmpNotEq, lt, eq); // false
  b.ret(f, e, both);
  OptStats st;
  CHECK(optimizeFunction(f, &st));
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  bool foundFalse = false;
  for (const auto &in : e.instrs) {
    if (in.hasResult && in.op == Op::ConstBool && !in.boolVal)
      foundFalse = true;
  }
  CHECK(foundFalse);
  std::cout << "testFoldCmp OK\n";
  return 0;
}

static int testFoldDivSafety() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f = b.newFunction("f", {}, MirType::i64(), "entry");
  BasicBlock &e = b.addBlock(f, "entry");
  Value one = b.constI64(f, e, 1);
  Value zero = b.constI64(f, e, 0);
  Value q = b.div(f, e, one, zero); // tetap runtime, JANGAN difold
  b.ret(f, e, q);
  OptStats st;
  optimizeFunction(f, &st);
  bool stillDiv = false;
  for (const auto &in : e.instrs) {
    if (in.op == Op::Div)
      stillDiv = true;
  }
  CHECK(stillDiv);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  // 10 / 2 = 5 aman difold.
  MirModule mod2;
  MirBuilder b2(mod2);
  MirFunction &g = b2.newFunction("g", {}, MirType::i64(), "entry");
  BasicBlock &e2 = b2.addBlock(g, "entry");
  Value t = b2.constI64(g, e2, 10);
  Value w = b2.constI64(g, e2, 2);
  Value v = b2.div(g, e2, t, w);
  b2.ret(g, e2, v);
  optimizeFunction(g);
  bool found5 = false;
  for (const auto &in : e2.instrs) {
    if (in.hasResult && in.op == Op::ConstI64 && in.intVal == 5)
      found5 = true;
  }
  CHECK(found5);
  std::cout << "testFoldDivSafety OK\n";
  return 0;
}

static int testDce() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
  BasicBlock &e = b.addBlock(f, "entry");
  Value dead1 = b.constI64(f, e, 1); // mati
  Value dead2 = b.constI64(f, e, 2); // mati
  (void)dead1;
  (void)dead2;
  Value live = b.constI64(f, e, 3);
  b.print(e, live);                  // print = efek, bertahan
  b.store(e, "slot", live);          // store = efek, bertahan
  b.ret(f, e);
  size_t before = e.instrs.size();
  OptStats st;
  CHECK(eliminateDeadCode(f, &st));
  CHECK(st.eliminated == 2);
  CHECK(e.instrs.size() == before - 2);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  std::cout << "testDce OK\n";
  return 0;
}

static int testPipeline() {
  // Fold membuka peluang DCE: (1+2) tak dipakai + print(40+2).
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
  BasicBlock &e = b.addBlock(f, "entry");
  Value a = b.constI64(f, e, 1);
  Value c = b.constI64(f, e, 2);
  Value dead = b.add(f, e, a, c);
  (void)dead;
  Value x = b.constI64(f, e, 40);
  Value y = b.constI64(f, e, 2);
  Value s = b.add(f, e, x, y);
  b.print(e, s);
  b.ret(f, e);
  OptStats st;
  CHECK(optimizeModule(mod, &st) > 0);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  // Sisa: const 42 + print + ret = 3 instruksi (const 1/2/3 mati).
  CHECK(e.instrs.size() == 3);
  bool found42 = false;
  for (const auto &in : e.instrs) {
    if (in.hasResult && in.op == Op::ConstI64 && in.intVal == 42)
      found42 = true;
  }
  CHECK(found42);
  std::cout << "testPipeline OK\n";
  return 0;
}

// Phase 102 (milestone 1): penerus/pendahulu/terjangkau/RPO pada diamond
// (entry -> then/else -> merge -> loop-o) + satu block mati; self-loop
// tetap terjangkau.
static int testCfg() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f =
      b.newFunction("f", {{"c", MirType::boolean()}}, MirType::void_(), "entry");
  BasicBlock &entry = b.addBlock(f, "entry");
  BasicBlock &thenB = b.addBlock(f, "then");
  BasicBlock &elseB = b.addBlock(f, "else");
  BasicBlock &merge = b.addBlock(f, "merge");
  BasicBlock &loop = b.addBlock(f, "loop");
  BasicBlock &dead = b.addBlock(f, "dead");
  b.condBr(entry, Value{0, MirType::boolean()}, "then", "else");
  b.br(thenB, "merge");
  b.br(elseB, "merge");
  b.br(merge, "loop");
  b.br(loop, "loop"); // self-loop: hidup
  b.unreachable(dead); // mati: tanpa penerus, tak dirujuk siapa pun

  CHECK(successors(entry).size() == 2);
  CHECK(successors(thenB).size() == 1 && successors(thenB)[0] == "merge");
  CHECK(successors(dead).empty());
  auto pred = predecessors(f);
  CHECK(pred["merge"].size() == 2); // then + else
  CHECK(pred["loop"].size() == 2);  // merge + diri sendiri
  CHECK(pred["dead"].empty());
  auto live = reachable(f);
  CHECK(live.count("entry") && live.count("then") && live.count("else"));
  CHECK(live.count("merge") && live.count("loop"));
  CHECK(!live.count("dead"));
  auto order = rpo(f);
  CHECK(!order.empty() && order[0] == "entry");
  CHECK(order.size() == 5); // semua kecuali dead
  std::cout << "testCfg OK\n";
  return 0;
}

// Phase 102 (milestone 1): block mati terbuang; modul lolos verify;
// entry-tanpa-block (invalid) dibiarkan untuk verifier.
static int testEliminateUnreachableBlocks() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &f = b.newFunction("f", {}, MirType::void_(), "entry");
  BasicBlock &e = b.addBlock(f, "entry");
  BasicBlock &d1 = b.addBlock(f, "dead1");
  BasicBlock &d2 = b.addBlock(f, "dead2");
  Value v = b.constI64(f, e, 7);
  b.print(e, v);
  b.ret(f, e);
  Value w = b.constI64(f, d1, 8);
  b.print(d1, w);
  b.br(d1, "dead2");
  Value u = b.constI64(f, d2, 9);
  b.print(d2, u);
  b.ret(f, d2);
  OptStats st;
  CHECK(eliminateUnreachableBlocks(f, &st));
  CHECK(st.blocksRemoved == 2);
  CHECK(f.blocks.size() == 1 && f.blocks[0].label == "entry");
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  CHECK(!eliminateUnreachableBlocks(f, &st)); // idempoten
  CHECK(st.blocksRemoved == 2);
  // Entry hilang -> diam (verifier yang melapor).
  MirModule mod2;
  MirBuilder b2(mod2);
  MirFunction &g = b2.newFunction("g", {}, MirType::void_(), "missing");
  BasicBlock &ge = b2.addBlock(g, "entry");
  b2.ret(g, ge);
  CHECK(!eliminateUnreachableBlocks(g, nullptr));
  CHECK(g.blocks.size() == 1);
  std::cout << "testEliminateUnreachableBlocks OK\n";
  return 0;
}

// Phase 103 (milestone 1): call ke callee satu-block tersalin;
// fold+DCE sesudahnya melipat lintas batas call (print 30).
static int testInlineSmallCall() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &add =
      b.newFunction("add", {{"a", MirType::i64()}, {"b", MirType::i64()}},
                    MirType::i64(), "entry");
  BasicBlock &ae = b.addBlock(add, "entry");
  Value s = b.add(add, ae, Value{0, MirType::i64()}, Value{1, MirType::i64()});
  b.ret(add, ae, s);
  MirFunction &main = b.newFunction("main", {}, MirType::void_(), "entry");
  BasicBlock &me = b.addBlock(main, "entry");
  Value twenty = b.constI64(main, me, 20);
  Value ten = b.constI64(main, me, 10);
  Value x = b.call(main, me, "add", {twenty, ten}, MirType::i64());
  b.print(me, x);
  b.ret(main, me);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  OptStats st;
  CHECK(optimizeModule(mod, &st) > 0);
  CHECK(st.inlined == 1);
  CHECK(errors.empty() && verify(mod, errors));
  std::string d = dump(mod);
  CHECK(d.find("call @add") == std::string::npos); // call hilang
  CHECK(d.find("print %") != std::string::npos);
  std::cout << "testInlineSmallCall OK\n";
  return 0;
}

// Phase 103: penjaga inlining — rekursi langsung, multi-block, dan
// callee raksasa (> kMaxInlineInstrs) tidak disentuh.
static int testInlineGuards() {
  MirModule mod;
  MirBuilder b(mod);
  // Rekursif langsung.
  MirFunction &rec = b.newFunction("rec", {}, MirType::void_(), "entry");
  BasicBlock &re = b.addBlock(rec, "entry");
  Value z = b.constI64(rec, re, 0);
  b.print(re, z);
  b.call(rec, re, "rec", {}, MirType::void_());
  b.ret(rec, re);
  // Multi-block.
  MirFunction &mb =
      b.newFunction("mb", {}, MirType::void_(), "e0");
  BasicBlock &m0 = b.addBlock(mb, "e0");
  BasicBlock &m1 = b.addBlock(mb, "e1");
  b.br(m0, "e1");
  Value v = b.constI64(mb, m1, 1);
  b.print(m1, v);
  b.ret(mb, m1);
  // Raksasa satu-block.
  MirFunction &big = b.newFunction("big", {}, MirType::void_(), "entry");
  BasicBlock &be = b.addBlock(big, "entry");
  Value acc = b.constI64(big, be, 0);
  for (size_t k = 0; k < kMaxInlineInstrs + 1; k++)
    acc = b.add(big, be, acc, b.constI64(big, be, 1));
  b.print(be, acc);
  b.ret(big, be);
  MirFunction &main = b.newFunction("main", {}, MirType::void_(), "entry");
  BasicBlock &me = b.addBlock(main, "entry");
  // Panggil mb + big dari main (tetap — multi-block / raksasa).
  // rec TIDAK dipanggil dari main: uji rekursi = self-call di dalam
  // rec sendiri harus dipertahankan (bukan disalin ke dirinya).
  b.call(main, me, "mb", {}, MirType::void_());
  b.call(main, me, "big", {}, MirType::void_());
  b.ret(main, me);
  OptStats st;
  optimizeModule(mod, &st);
  CHECK(st.inlined == 0);
  std::string d = dump(mod);
  CHECK(d.find("call @rec") != std::string::npos); // self-call lestari
  CHECK(d.find("call @mb") != std::string::npos);
  CHECK(d.find("call @big") != std::string::npos);
  std::cout << "testInlineGuards OK\n";
  return 0;
}

// Phase 103: slot callee di-rename agar tak tabrakan slot caller.
static int testInlineSlotRename() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &get =
      b.newFunction("get", {}, MirType::i64(), "entry");
  BasicBlock &ge = b.addBlock(get, "entry");
  Value one = b.constI64(get, ge, 1);
  b.store(ge, "x", one);
  Value r = b.load(get, ge, "x", MirType::i64());
  b.ret(get, ge, r);
  MirFunction &main = b.newFunction("main", {}, MirType::void_(), "entry");
  BasicBlock &me = b.addBlock(main, "entry");
  Value nine = b.constI64(main, me, 9);
  b.store(me, "x", nine);
  Value g = b.call(main, me, "get", {}, MirType::i64());
  Value keep = b.load(main, me, "x", MirType::i64());
  b.print(me, g);
  b.print(me, keep);
  b.ret(main, me);
  std::vector<VerifyError> errors;
  CHECK(verify(mod, errors));
  OptStats st;
  CHECK(optimizeModule(mod, &st) > 0);
  CHECK(st.inlined == 1);
  CHECK(verify(mod, errors));
  std::string d = dump(mod);
  CHECK(d.find("x.inl0") != std::string::npos); // slot salinan ter-rename
  std::cout << "testInlineSlotRename OK\n";
  return 0;
}

int main() {
  if (testBuildAndVerify()) return 1;
  if (testDumpGolden()) return 1;
  if (testControlFlow()) return 1;
  if (testRejects()) return 1;
  if (testFreshBuilder()) return 1;
  if (testFoldArith()) return 1;
  if (testFoldCmp()) return 1;
  if (testFoldDivSafety()) return 1;
  if (testDce()) return 1;
  if (testPipeline()) return 1;
  if (testCfg()) return 1;
  if (testEliminateUnreachableBlocks()) return 1;
  if (testInlineSmallCall()) return 1;
  if (testInlineGuards()) return 1;
  if (testInlineSlotRename()) return 1;
  std::cout << "ALL MIR TESTS PASSED\n";
  return 0;
}
