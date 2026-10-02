// Unit test codegen v0.3.0: variables & expressions -> LLVM IR.
// Pipeline parse (front-end teruji) lalu generate; IR diperiksa bentuknya.
// Nilai runtime yang sebenarnya diverifikasi e2e_variables.sh.
#include "compiler/codegen/Codegen.h"
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/driver/Driver.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"
#include "compiler/semantic/Semantic.h"
#include <iostream>

using namespace nusa;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

// Parse + semantic, lalu generate. ok=false berarti generate menolak.
static std::string genIR(const std::string &src, bool &ok, std::string &err) {
  Lexer lx("t.ns", src);
  auto toks = lx.tokenize();
  if (!lx.errors().empty()) {
    ok = false;
    err = "lexer error";
    return "";
  }
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p("t.ns", toks, lines);
  ParseResult pres = p.parse();
  if (!pres.ok()) {
    ok = false;
    err = "parser error";
    return "";
  }
  SemanticAnalyzer sema;
  if (!sema.analyze(pres.program).empty()) {
    ok = false;
    err = "semantic error";
    return "";
  }
  Codegen cg("nusa", "t.ns");
  ok = cg.generate(pres.program, err);
  if (!ok)
    return "";
  return cg.emitIRToString();
}

static int testMilestoneAdd() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() {\n"
                         "    let a = 10;\n"
                         "    let b = 20;\n"
                         "    let result = a + b;\n"
                         "    print(result);\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("add i64") != std::string::npos);
  CHECK(ir.find("@printf") != std::string::npos); // call vararg: (ptr, ...)
  CHECK(ir.find("ret i32 0") != std::string::npos);
  std::cout << "testMilestoneAdd OK\n";
  return 0;
}

static int testPrecedenceMulAdd() {
  bool ok = false;
  std::string err;
  // Variabel (bukan literal) agar LLVM tidak constant-folding;
  // urutan mul lalu add tetap terlihat di IR.
  std::string ir = genIR("fn main() { let a = 10; let b = 20; let c = 3; "
                         "let x = a + b * c; print(x); }",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("mul i64") != std::string::npos);
  CHECK(ir.find("add i64") != std::string::npos);
  std::cout << "testPrecedenceMulAdd OK\n";
  return 0;
}

static int testSubDiv() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() { let a = 20; let b = 4; let c = 1; "
                         "print(a / b - c); }",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("sdiv i64") != std::string::npos);
  CHECK(ir.find("sub i64") != std::string::npos);
  std::cout << "testSubDiv OK\n";
  return 0;
}

static int testBoolCompare() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() { let a = 10; let b = 5; "
                         "let f = a > b; print(f); }",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("icmp sgt") != std::string::npos);
  CHECK(ir.find("select") != std::string::npos); // bool -> "true"/"false"
  std::cout << "testBoolCompare OK\n";
  return 0;
}

static int testUnaryNeg() {
  bool ok = false;
  std::string err;
  std::string ir =
      genIR("fn main() { let a = 5; let n = -a; print(n); }", ok, err);
  CHECK(ok);
  CHECK(ir.find("sub i64 0,") != std::string::npos); // neg via CreateNeg
  std::cout << "testUnaryNeg OK\n";
  return 0;
}

static int testStringVar() {
  bool ok = false;
  std::string err;
  std::string ir =
      genIR("fn main() { let s = \"Nusa\"; print(s); }", ok, err);
  CHECK(ok);
  CHECK(ir.find("call i32 @puts") != std::string::npos);
  CHECK(ir.find("Nusa") != std::string::npos);
  std::cout << "testStringVar OK\n";
  return 0;
}

static int testHelloStillWorks() {
  bool ok = false;
  std::string err;
  std::string ir =
      genIR("fn main() { print(\"Hello, Nusa!\"); }", ok, err);
  CHECK(ok);
  CHECK(ir.find("define i32 @main(i32") != std::string::npos);
  CHECK(ir.find("Hello, Nusa!") != std::string::npos);
  std::cout << "testHelloStillWorks OK\n";
  return 0;
}

static int testMismatchRejected() {
  bool ok = true;
  std::string err;
  genIR("fn main() { let x = 1 + true; print(x); }", ok, err);
  CHECK(!ok); // guard codegen (typechecker menolak lebih dulu di driver)
  CHECK(err.find("mismatched") != std::string::npos);
  CHECK(err.find("type checker") != std::string::npos);
  std::cout << "testMismatchRejected OK\n";
  return 0;
}

static int testAnnotatedProgram() {
  // Anotasi v0.4.0 tidak mengubah IR (checker memvalidasi; codegen sama).
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() {\n"
                         "    let age: i64 = 15;\n"
                         "    let active: bool = true;\n"
                         "    print(age);\n"
                         "    print(active);\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("alloca i64") != std::string::npos);
  CHECK(ir.find("alloca i1") != std::string::npos);
  CHECK(ir.find("@printf") != std::string::npos);
  std::cout << "testAnnotatedProgram OK\n";
  return 0;
}

static int testFunctionIR() {
  // LLVM function + call yang benar (tuntutan MASTER PROMPT v0.5.0).
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn add(a: i64, b: i64) -> i64 {\n"
                         "    return a + b;\n"
                         "}\n"
                         "fn main() {\n"
                         "    let result = add(10, 20);\n"
                         "    print(result);\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("define i64 @add(i64") != std::string::npos);
  CHECK(ir.find("define i32 @main(i32") != std::string::npos);
  CHECK(ir.find("call i64 @add(i64 10, i64 20)") != std::string::npos);
  CHECK(ir.find("ret i64") != std::string::npos);
  CHECK(ir.find("ret i32 0") != std::string::npos);
  std::cout << "testFunctionIR OK\n";
  return 0;
}

static int testForwardCallIR() {
  // Panggilan ke fungsi yang dideklarasikan belakangan (two-pass).
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() { print(add(1, 2)); }\n"
                         "fn add(a: i64, b: i64) -> i64 { return a + b; }\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("call i64 @add") != std::string::npos);
  std::cout << "testForwardCallIR OK\n";
  return 0;
}

static int testBoolStringFuncIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn is_big(n: i64) -> bool { return n > 100; }\n"
                         "fn greet(name: string) { print(name); }\n"
                         "fn main() { print(is_big(200)); greet(\"Nusa\"); }\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("define i1 @is_big(i64") != std::string::npos);
  CHECK(ir.find("define void @greet(ptr") != std::string::npos);
  CHECK(ir.find("ret void") != std::string::npos);
  std::cout << "testBoolStringFuncIR OK\n";
  return 0;
}

static int testIfIR() {
  // Basic blocks entry/then/else/merge (tuntutan MASTER PROMPT v0.6.0).
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() {\n"
                         "    let x: i64 = 10;\n"
                         "    if x > 5 {\n"
                         "        print(x);\n"
                         "    } else {\n"
                         "        print(0);\n"
                         "    }\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("then:") != std::string::npos);
  CHECK(ir.find("else:") != std::string::npos);
  CHECK(ir.find("merge:") != std::string::npos);
  CHECK(ir.find("br i1") != std::string::npos);
  std::cout << "testIfIR OK\n";
  return 0;
}

static int testWhileIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() {\n"
                         "    while true {\n"
                         "        print(1);\n"
                         "    }\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("loop.cond:") != std::string::npos);
  CHECK(ir.find("loop.body:") != std::string::npos);
  CHECK(ir.find("loop.after:") != std::string::npos);
  CHECK(ir.find("br label") != std::string::npos);
  std::cout << "testWhileIR OK\n";
  return 0;
}

static int testBranchReturnIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn f(c: bool) -> i64 {\n"
                         "    if c {\n"
                         "        return 1;\n"
                         "    } else {\n"
                         "        return 2;\n"
                         "    }\n"
                         "}\n"
                         "fn main() { print(f(true)); }\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("ret i64 1") != std::string::npos);
  CHECK(ir.find("ret i64 2") != std::string::npos);
  std::cout << "testBranchReturnIR OK\n";
  return 0;
}

static int testArrayIR() {
  // alloca [N x T], GEP indexing, bounds-check, exit (v0.7.0).
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() {\n"
                         "    let numbers: [i64] = [1, 2, 3, 4];\n"
                         "    print(numbers[0]);\n"
                         "    print(len(numbers));\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("alloca [4 x i64]") != std::string::npos);
  CHECK(ir.find("getelementptr") != std::string::npos);
  CHECK(ir.find("oob.fail") != std::string::npos);
  CHECK(ir.find("declare void @exit(i32)") != std::string::npos);
  CHECK(ir.find("index out of bounds") != std::string::npos);
  std::cout << "testArrayIR OK\n";
  return 0;
}

static int testArrayCopyIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn main() {\n"
                         "    let a = [1, 2];\n"
                         "    let b = a;\n"
                         "    b = a;\n"
                         "    print(b[1]);\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("alloca [2 x i64]") != std::string::npos);
  std::cout << "testArrayCopyIR OK\n";
  return 0;
}

static int testStructIR() {
  // Named struct type + GEP field + loads (v0.8.0).
  bool ok = false;
  std::string err;
  std::string ir = genIR("struct Person {\n"
                         "    name: string;\n"
                         "    age: i64;\n"
                         "}\n"
                         "fn main() {\n"
                         "    let p = Person { name: \"Nusa\", age: 1 };\n"
                         "    print(p.age);\n"
                         "    print(p.name);\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("%Person = type { ptr, i64 }") != std::string::npos);
  CHECK(ir.find("alloca %Person") != std::string::npos);
  std::cout << "testStructIR OK\n";
  return 0;
}

static int testNestedStructIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("struct Inner { v: i64; }\n"
                         "struct Outer { inner: Inner; n: i64; }\n"
                         "fn main() {\n"
                         "    let o = Outer { inner: Inner { v: 7 }, n: 1 };\n"
                         "    print(o.inner.v);\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("%Outer = type { %Inner, i64 }") != std::string::npos);
  std::cout << "testNestedStructIR OK\n";
  return 0;
}

static int testDivZeroGuardIR() {
  // Runtime check pembagi-nol selalu diemisikan (v0.9.5 hardening).
  bool ok = false;
  std::string err;
  std::string ir =
      genIR("fn main() { let z = 0; print(10 / z); }", ok, err);
  CHECK(ok);
  CHECK(ir.find("div.zero") != std::string::npos);
  CHECK(ir.find("division by zero") != std::string::npos);
  std::cout << "testDivZeroGuardIR OK\n";
  return 0;
}

// v1.2.0 (Phase 21b): function reference + indirect call (ptr).
static int testFnRefIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn add(a: i64, b: i64) -> i64 {\n"
                         "    return a + b;\n"
                         "}\n"
                         "fn apply(f: fn(i64, i64) -> i64, x: i64) -> i64 {\n"
                         "    return f(x, 1);\n"
                         "}\n"
                         "fn main() {\n"
                         "    let op = add;\n"
                         "    print(apply(op, 10));\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  // Param fungsi = pointer; indirect call via loaded pointer.
  CHECK(ir.find("define i64 @apply(ptr") != std::string::npos);
  CHECK(ir.find("call i64 %") != std::string::npos);
  std::cout << "testFnRefIR OK\n";
  return 0;
}

// Regression: indirect call void TANPA nama hasil (IR invalid bila bernama).
static int testFnVoidIndirectIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("fn v() {\n"
                         "    print(1);\n"
                         "}\n"
                         "fn run(g: fn() -> void) {\n"
                         "    g();\n"
                         "}\n"
                         "fn main() {\n"
                         "    run(v);\n"
                         "}\n",
                         ok, err);
  CHECK(ok);
  CHECK(ir.find("call void %") != std::string::npos);
  std::cout << "testFnVoidIndirectIR OK\n";
  return 0;
}

// v1.2.0 (Phase 21d): struct by-value (param + return record).
static int testStructByValueIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIR("struct P { x: i64; }\n"
                         "fn mk(v: i64) -> P { return P { x: v }; }\n"
                         "fn get(p: P) -> i64 { return p.x; }\n"
                         "fn main() { print(get(mk(7))); }\n",
                         ok, err);
  CHECK(ok);
  // Param/return bertipe record; panggilan tak langsung via value.
  CHECK(ir.find("define %P @mk(i64") != std::string::npos);
  CHECK(ir.find("define i64 @get(%P") != std::string::npos);
  CHECK(ir.find("ret %P") != std::string::npos);
  std::cout << "testStructByValueIR OK\n";
  return 0;
}

// v1.14.0 (Phase 33): triple target di IR + resolusi target.
static std::string genIRTriple(const std::string &src,
                               const std::string &triple, bool &ok,
                               std::string &err) {
  Lexer lx("t.ns", src);
  auto toks = lx.tokenize();
  if (!lx.errors().empty()) {
    ok = false;
    err = "lexer error";
    return "";
  }
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p("t.ns", toks, lines);
  ParseResult pres = p.parse();
  if (!pres.ok()) {
    ok = false;
    err = "parser error";
    return "";
  }
  SemanticAnalyzer sema;
  if (!sema.analyze(pres.program).empty()) {
    ok = false;
    err = "semantic error";
    return "";
  }
  Codegen cg("nusa", "t.ns");
  if (!triple.empty())
    cg.setTargetTriple(triple);
  ok = cg.generate(pres.program, err);
  if (!ok)
    return "";
  return cg.emitIRToString();
}

static int testTargetTripleIR() {
  bool ok = false;
  std::string err;
  std::string ir = genIRTriple("fn main() { print(1); }\n",
                               "x86_64-linux-gnu", ok, err);
  CHECK(ok);
  CHECK(ir.find("target triple = \"x86_64-linux-gnu\"") != std::string::npos);
  // Default: tanpa triple (perilaku lama).
  std::string ir2 = genIRTriple("fn main() { print(1); }\n", "", ok, err);
  CHECK(ok);
  CHECK(ir2.find("target triple") == std::string::npos);
  std::cout << "testTargetTripleIR OK\n";
  return 0;
}

// v1.18.0 (Phase 37): metadata DWARF di IR (bila -g).
static std::string genIRDebug(const std::string &src, bool debug, bool &ok,
                              std::string &err) {
  Lexer lx("t.ns", src);
  auto toks = lx.tokenize();
  if (!lx.errors().empty()) {
    ok = false;
    err = "lexer error";
    return "";
  }
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p("t.ns", toks, lines);
  ParseResult pres = p.parse();
  if (!pres.ok()) {
    ok = false;
    err = "parser error";
    return "";
  }
  SemanticAnalyzer sema;
  if (!sema.analyze(pres.program).empty()) {
    ok = false;
    err = "semantic error";
    return "";
  }
  Codegen cg("nusa", "t.ns");
  if (debug)
    cg.setDebug(true);
  ok = cg.generate(pres.program, err);
  if (!ok)
    return "";
  return cg.emitIRToString();
}

static int testDebugIR() {
  bool ok = false;
  std::string err;
  std::string src = "fn add(a: i64, b: i64) -> i64 {\n"
                    "    let c = a + b;\n"
                    "    return c;\n"
                    "}\n"
                    "fn main() { print(add(1, 2)); }\n";
  std::string ir = genIRDebug(src, true, ok, err);
  CHECK(ok);
  CHECK(ir.find("DICompileUnit") != std::string::npos);
  CHECK(ir.find("DISubprogram(name: \"add\"") != std::string::npos);
  CHECK(ir.find("DISubprogram(name: \"main\"") != std::string::npos);
  CHECK(ir.find("DILocalVariable(name: \"c\"") != std::string::npos);
  CHECK(ir.find("DILocalVariable(name: \"a\"") != std::string::npos);
  CHECK(ir.find("dbg_declare") != std::string::npos);
  CHECK(ir.find("DILocation(line: 2") != std::string::npos);
  CHECK(ir.find("Debug Info Version") != std::string::npos);
  // Default: tanpa metadata.
  std::string ir2 = genIRDebug(src, false, ok, err);
  CHECK(ok);
  CHECK(ir2.find("DICompileUnit") == std::string::npos);
  CHECK(ir2.find("dbg_declare") == std::string::npos);
  std::cout << "testDebugIR OK\n";
  return 0;
}

// Phase 64 (v2.15.0): DWARF composite (struct/array) + formal params.
static int testDebugCompositeIR() {
  bool ok = false;
  std::string err;
  std::string src =
      "struct Person {\n"
      "    name: string;\n"
      "    age: i64;\n"
      "}\n"
      "fn greet(p: Person) -> i64 {\n"
      "    return p.age;\n"
      "}\n"
      "fn main() {\n"
      "    let p = Person { name: \"Nusa\", age: 30 };\n"
      "    let a: [i64] = [1, 2, 3];\n"
      "    print(greet(p));\n"
      "}\n";
  std::string ir = genIRDebug(src, true, ok, err);
  CHECK(ok);
  CHECK(ir.find("DW_TAG_structure_type") != std::string::npos);
  CHECK(ir.find("DW_TAG_member") != std::string::npos);
  CHECK(ir.find("DILocalVariable(name: \"p\"") != std::string::npos);
  CHECK(ir.find("DW_TAG_array_type") != std::string::npos);
  CHECK(ir.find("DISubrange") != std::string::npos);
  CHECK(ir.find("DILocalVariable(name: \"a\"") != std::string::npos);
  // Parameter formal (arg: 1), bukan auto-variable.
  CHECK(ir.find("DILocalVariable(name: \"p\", arg: 1") != std::string::npos);
  std::cout << "testDebugCompositeIR OK\n";
  return 0;
}

static int testResolveTarget() {
  nusa::ResolvedTarget rt;
  std::string err;
  std::string host = nusa::Driver::hostTriple();
  CHECK(!host.empty());
  CHECK(nusa::Driver::resolveTarget("linux-x86_64", rt, err));
  CHECK(rt.triple == "x86_64-linux-gnu");
  CHECK(rt.isHost == (rt.triple == host));
  CHECK(nusa::Driver::resolveTarget("linux-aarch64", rt, err));
  CHECK(rt.triple == "aarch64-linux-gnu");
  CHECK(rt.isHost == (rt.triple == host));
  CHECK(nusa::Driver::resolveTarget("android-aarch64", rt, err));
  CHECK(rt.triple == host && rt.isHost);
  CHECK(nusa::Driver::resolveTarget(host, rt, err));
  CHECK(rt.isHost);
  // Phase 67 (v2.18.0): wasm32 bare-metal (non-host).
  CHECK(nusa::Driver::resolveTarget("wasm32", rt, err));
  CHECK(rt.triple == "wasm32-unknown-unknown");
  CHECK(!rt.isHost);
  CHECK(nusa::Driver::resolveTarget("wasm32-unknown-unknown", rt, err));
  CHECK(rt.triple == "wasm32-unknown-unknown");
  CHECK(!nusa::Driver::resolveTarget("windows-x64", rt, err));
  CHECK(err.find("Supported:") != std::string::npos);
  std::cout << "testResolveTarget OK\n";
  return 0;
}

int main() {
  if (testStructIR()) return 1;
  if (testNestedStructIR()) return 1;
  if (testArrayIR()) return 1;
  if (testArrayCopyIR()) return 1;
  if (testIfIR()) return 1;
  if (testWhileIR()) return 1;
  if (testBranchReturnIR()) return 1;
  if (testFunctionIR()) return 1;
  if (testForwardCallIR()) return 1;
  if (testBoolStringFuncIR()) return 1;
  if (testAnnotatedProgram()) return 1;
  if (testMilestoneAdd()) return 1;
  if (testPrecedenceMulAdd()) return 1;
  if (testSubDiv()) return 1;
  if (testBoolCompare()) return 1;
  if (testUnaryNeg()) return 1;
  if (testStringVar()) return 1;
  if (testHelloStillWorks()) return 1;
  if (testMismatchRejected()) return 1;
  if (testDivZeroGuardIR()) return 1;
  if (testFnRefIR()) return 1;
  if (testFnVoidIndirectIR()) return 1;
  if (testStructByValueIR()) return 1;
  if (testTargetTripleIR()) return 1;
  if (testDebugIR()) return 1;
  if (testDebugCompositeIR()) return 1;
  if (testResolveTarget()) return 1;
  std::cout << "ALL CODEGEN TESTS PASSED\n";
  return 0;
}
