// Property test fondasi verifikasi v2.22.0 (Phase 71, RFC-0010 slice 1).
// Deterministik penuh: keacakan semu = LCG seed tetap di-kode.
// Mengunci invarian I-LEX/I-PARSE/I-FMT/I-CHECK/I-MIR.
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/fmt/Formatter.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/mir/Mir.h"
#include "compiler/mir/Opt.h"
#include "compiler/parser/Parser.h"
#include "compiler/semantic/Semantic.h"
#include "compiler/semantic/TypeChecker.h"
#include <iostream>
#include <string>
#include <vector>

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

// LCG 32-bit deterministik (seed tetap; bukan waktu).
static uint32_t lcgNext(uint32_t &s) {
  s = s * 1664525u + 1013904223u;
  return s;
}

// I-LEX totalitas: byte arbitrer selalu selesai, error terbatas.
static int testLexTotality() {
  for (int b = 0; b < 256; b++) {
    std::string src(1, (char)b);
    Lexer lx("fuzz.ns", src);
    (void)lx.tokenize(); // selesai tanpa crash = lolos
    CHECK(lx.errors().size() <= Lexer::kMaxErrors + 1);
  }
  uint32_t s = 0xC0FFEEu;
  std::string soup;
  for (int i = 0; i < 4096; i++)
    soup += (char)(lcgNext(s) >> 7); // semua 256 pola byte
  Lexer lx("soup.ns", soup);
  (void)lx.tokenize();
  CHECK(lx.errors().size() <= Lexer::kMaxErrors + 1);
  Lexer le("empty.ns", "");
  auto toks = le.tokenize();
  CHECK(le.errors().empty());
  CHECK(toks.size() == 1); // hanya Eof
  std::cout << "testLexTotality OK\n";
  return 0;
}

static const std::vector<std::string> kFmtCorpus = {
    "fn   main(){print(   \"hi\" );let x=1+2*3;print(x);}\n",
    "struct P{name:string;age:i64;}\nfn f(c:bool)->i64{if c{return 1;}else{return 2;}}\n",
    "// top\nfn main() {\n    print(1); // one\n}\n",
    "const MAX: i64 = 100;\nconst D = MAX * 2;\nfn main() { print(D); }\n",
    "enum E { A, B }\nfn f(e: E) -> i64 {\n match e { E::A => 1, _ => 2, }\n}\nfn main() { print(f(E::A)); }\n",
    "fn add(a: i64, b: i64) -> i64 {\n return a + b;\n}\nfn main() { let i = 0;\n while i < 3 {\n i = i + 1;\n }\n print(add(1, 2));\n}\n",
    "/// doc comment\nfn main() {\n print(\"x\");\n}\n",
    "fn main() { let t = [1, 2, 3];\n print(len(t));\n}\n",
};

// I-FMT idempotence: ok => reformat identik + tetap ok.
static int testFmtIdempotent() {
  for (size_t i = 0; i < kFmtCorpus.size(); i++) {
    FormatResult r1 = formatSource("t.ns", kFmtCorpus[i]);
    CHECK(r1.ok);
    FormatResult r2 = formatSource("t.ns", r1.text);
    CHECK(r2.ok);
    CHECK(r2.text == r1.text);
  }
  std::cout << "testFmtIdempotent OK\n";
  return 0;
}

static const std::vector<std::string> kCheckCorpus = {
    "fn main() { print(1 + 2 * 3); }\n",
    "const MAX: i64 = 100;\nconst D = MAX * 2;\nfn main() { print(D); }\n",
    "fn main() { let x: i64 = \"hello\"; }\n",    // mismatch
    "fn main() { print(undefined_var); }\n",     // unknown
    "const X: bool = 1;\nfn main() { print(X); }\n", // const mismatch
    "fn main() { let x = 1/0; print(x); }\n",    // div nol
    "fn main() {\n",                             // tak-terminasi
};

static std::string parseFingerprint(const std::string &src, bool &ok) {
  Lexer lx("t.ns", src);
  auto toks = lx.tokenize();
  if (!lx.errors().empty()) {
    ok = false;
    return "lexfail";
  }
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p("t.ns", toks, lines);
  ParseResult r = p.parse();
  ok = r.ok();
  return r.ok() ? r.program.dump() : "parsefail";
}

// I-PARSE determinisme: sumber sama -> dump sama, 2 run.
static int testParseDeterministic() {
  for (size_t i = 0; i < kCheckCorpus.size(); i++) {
    bool ok1 = false, ok2 = false;
    std::string d1 = parseFingerprint(kCheckCorpus[i], ok1);
    std::string d2 = parseFingerprint(kCheckCorpus[i], ok2);
    CHECK(ok1 == ok2);
    CHECK(d1 == d2);
  }
  std::cout << "testParseDeterministic OK\n";
  return 0;
}

static std::string checkFingerprint(const std::string &src) {
  Lexer lx("t.ns", src);
  auto toks = lx.tokenize();
  if (!lx.errors().empty())
    return "lexfail";
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p("t.ns", toks, lines);
  ParseResult pres = p.parse();
  if (!pres.ok())
    return "parsefail";
  SemanticAnalyzer sema;
  auto serrs = sema.analyze(pres.program);
  std::string fp = "sem" + std::to_string(serrs.size()) + ":";
  for (auto &e : serrs)
    fp += e.message + "|";
  TypeChecker checker;
  checker.check(pres.program);
  fp += "tc" + std::to_string(checker.errors().size()) + ":";
  for (auto &e : checker.errors())
    fp += e.message + "|";
  return fp;
}

// I-CHECK determinisme: diagnostik identik 2 run berurutan.
static int testCheckDeterministic() {
  for (size_t i = 0; i < kCheckCorpus.size(); i++) {
    std::string f1 = checkFingerprint(kCheckCorpus[i]);
    std::string f2 = checkFingerprint(kCheckCorpus[i]);
    CHECK(f1 == f2);
  }
  std::cout << "testCheckDeterministic OK\n";
  return 0;
}

static size_t countInstrs(const MirModule &mod) {
  size_t n = 0;
  for (auto &fn : mod.functions)
    for (auto &bb : fn.blocks)
      n += bb.instrs.size();
  return n;
}

// Modul foldable: %2 = add(const 20, const 10); print(call @id(%2)).
static MirModule buildFoldable() {
  MirModule mod;
  MirBuilder b(mod);
  MirFunction &id =
      b.newFunction("id", {{"x", MirType::i64()}}, MirType::i64(), "entry");
  BasicBlock &ie = b.addBlock(id, "entry");
  b.ret(id, ie, Value{0, MirType::i64()});
  MirFunction &main = b.newFunction("main", {}, MirType::void_(), "entry");
  BasicBlock &me = b.addBlock(main, "entry");
  Value a = b.constI64(main, me, 20);
  Value c = b.constI64(main, me, 10);
  Value s = b.add(main, me, a, c);
  Value v = b.call(main, me, "id", {s}, MirType::i64());
  b.print(me, v);
  b.ret(main, me);
  return mod;
}

// I-MIR: verify pra dipertahankan pasca-opt; instr tak naik.
static int testMirOptPreserves() {
  for (int round = 0; round < 2; round++) {
    MirModule mod = (round == 0) ? buildFoldable() : [] {
      MirModule m;
      MirBuilder b(m);
      MirFunction &f = b.newFunction("f", {{"c", MirType::boolean()}},
                                     MirType::i64(), "cond");
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
      Value dead = b.constI64(f, merge, 7);
      Value dead2 = b.add(f, merge, dead, dead);
      (void)dead2;
      Value r = b.load(f, merge, "r", MirType::i64());
      b.ret(f, merge, r);
      return m;
    }();
    std::vector<VerifyError> pre;
    CHECK(verify(mod, pre));
    size_t before = countInstrs(mod);
    OptStats stats;
    (void)optimizeModule(mod, &stats);
    std::vector<VerifyError> post;
    CHECK(verify(mod, post));
    CHECK(countInstrs(mod) <= before);
    if (round == 0)
      CHECK(stats.folded >= 1); // 20+10 terlipat
    else
      CHECK(stats.eliminated >= 1); // dead terhapus
  }
  std::cout << "testMirOptPreserves OK\n";
  return 0;
}

int main() {
  if (testLexTotality()) return 1;
  if (testFmtIdempotent()) return 1;
  if (testParseDeterministic()) return 1;
  if (testCheckDeterministic()) return 1;
  if (testMirOptPreserves()) return 1;
  std::cout << "ALL VERIFY TESTS PASSED\n";
  return 0;
}
