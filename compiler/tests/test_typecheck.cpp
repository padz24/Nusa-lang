// Unit test type checker v0.4.0: anotasi, kompatibilitas, inferensi.
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"
#include "compiler/semantic/Semantic.h"
#include "compiler/semantic/TypeChecker.h"
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

// Pipeline front-end penuh hingga type checking. parseOk=false berarti
// gagal sebelum type checker; hasilnya adalah error type checker.
static std::vector<SemanticError> checkSrc(const std::string &src,
                                           bool &parseOk) {
  Lexer lx("t.ns", src);
  auto toks = lx.tokenize();
  if (!lx.errors().empty()) {
    parseOk = false;
    return {};
  }
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p("t.ns", toks, lines);
  ParseResult pres = p.parse();
  if (!pres.ok()) {
    parseOk = false;
    return {};
  }
  SemanticAnalyzer sema;
  if (!sema.analyze(pres.program).empty()) {
    parseOk = false; // gagal di semantic, bukan ranah test ini
    return {};
  }
  parseOk = true;
  TypeChecker checker;
  checker.check(pres.program);
  return checker.errors();
}

static int testMasterAnnotationsOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() {\n"
                       "    let age: i64 = 15;\n"
                       "    let active: bool = true;\n"
                       "    let name: string = \"Nusa\";\n"
                       "    print(age);\n"
                       "    print(active);\n"
                       "    print(name);\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMasterAnnotationsOk OK\n";
  return 0;
}

static int testMismatchRejected() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let x: i64 = \"hello\"; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("'i64'") != std::string::npos);
  CHECK(errs[0].message.find("'string'") != std::string::npos);
  std::cout << "testMismatchRejected OK: " << errs[0].message << "\n";
  return 0;
}

// Phase 70 (v2.21.0, RFC-0009 slice 1): evaluasi + substitusi const.
static int testConstEvalOk() {
  bool parseOk = false;
  auto errs = checkSrc("const MAX: i64 = 100;\n"
                       "const DOUBLE = MAX * 2;\n"
                       "const OK = DOUBLE == 200;\n"
                       "fn main() { print(DOUBLE); print(OK); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testConstEvalOk OK\n";
  return 0;
}

static int testConstMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("const X: bool = 1;\nfn main() { print(X); }",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched types in 'const'") !=
        std::string::npos);
  std::cout << "testConstMismatch OK\n";
  return 0;
}

static int testConstCycle() {
  bool parseOk = false;
  auto errs = checkSrc("const A = B;\nconst B = A;\n"
                       "fn main() { print(A); }",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("circular constant") != std::string::npos);
  std::cout << "testConstCycle OK\n";
  return 0;
}

static int testConstDivZero() {
  bool parseOk = false;
  auto errs = checkSrc("const Z = 1 / 0;\nfn main() { print(Z); }",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("division by zero") != std::string::npos);
  std::cout << "testConstDivZero OK\n";
  return 0;
}

static int testConstNonConst() {
  bool parseOk = false;
  auto errs = checkSrc("const X = parse_int(\"1\");\n"
                       "fn main() { print(X); }",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("not a constant expression") !=
        std::string::npos);
  std::cout << "testConstNonConst OK\n";
  return 0;
}

static int testBoolMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let b: bool = 1; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  std::cout << "testBoolMismatch OK\n";
  return 0;
}

static int testVoidVarRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x: void = 1; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("'void'") != std::string::npos);
  std::cout << "testVoidVarRejected OK\n";
  return 0;
}

static int testInference() {
  // Tanpa anotasi: i64 disimpulkan, pemakaian dicek terhadapnya.
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x = 10; print(x + 1); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  auto errs2 =
      checkSrc("fn main() { let x = 10; print(x + true); }", parseOk);
  CHECK(parseOk);
  CHECK(errs2.size() == 1);
  std::cout << "testInference OK\n";
  return 0;
}

static int testArithMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { print(1 + true); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("i64") != std::string::npos);
  std::cout << "testArithMismatch OK\n";
  return 0;
}

static int testCompareMismatch() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let b = \"a\" < \"b\"; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  std::cout << "testCompareMismatch OK\n";
  return 0;
}

static int testStringEqualityRejected() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let b = \"a\" == \"b\"; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("string") != std::string::npos);
  std::cout << "testStringEqualityRejected OK\n";
  return 0;
}

static int testUnaryMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { print(-true); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  std::cout << "testUnaryMismatch OK\n";
  return 0;
}

static int testEqMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let b = 1 == true; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  std::cout << "testEqMismatch OK\n";
  return 0;
}

static int testMainFuncType() {
  // Function type checking: main bertipe fn() -> void.
  Lexer lx("t.ns", "fn main() { print(1); }");
  auto toks = lx.tokenize();
  auto lines = DiagnosticEngine::splitLines("fn main() { print(1); }");
  Parser p("t.ns", toks, lines);
  ParseResult pres = p.parse();
  CHECK(pres.ok());
  TypeChecker checker;
  CHECK(checker.check(pres.program));
  CHECK(checker.mainFuncType() == Type::func({}, Type::void_()));
  CHECK(checker.mainFuncType().name() == "fn() -> void");
  std::cout << "testMainFuncType OK\n";
  return 0;
}

static int testCallOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                       "fn main() { let r: i64 = add(10, 20); print(r); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testCallOk OK\n";
  return 0;
}

static int testArityMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                       "fn main() { print(add(1)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("wrong number of arguments") !=
        std::string::npos);
  std::cout << "testArityMismatch OK\n";
  return 0;
}

static int testArgTypeMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                       "fn main() { print(add(1, true)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched argument type") !=
        std::string::npos);
  std::cout << "testArgTypeMismatch OK\n";
  return 0;
}

static int testReturnMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("fn f() -> i64 { return true; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched return type") != std::string::npos);
  std::cout << "testReturnMismatch OK\n";
  return 0;
}

static int testMissingReturn() {
  bool parseOk = false;
  auto errs = checkSrc("fn f() -> i64 { let x = 1; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("must return a value") != std::string::npos);
  std::cout << "testMissingReturn OK\n";
  return 0;
}

static int testReturnValueInVoid() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { return 1; }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("void function") != std::string::npos);
  std::cout << "testReturnValueInVoid OK\n";
  return 0;
}

static int testBareReturnInNonVoid() {
  bool parseOk = false;
  auto errs = checkSrc("fn f() -> i64 { return; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  // bare return + missing valued return → minimal 1 error
  CHECK(!errs.empty());
  std::cout << "testBareReturnInNonVoid OK\n";
  return 0;
}

static int testUnreachableAfterReturn() {
  bool parseOk = false;
  auto errs = checkSrc("fn f() -> i64 { return 1; print(2); }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(!errs.empty());
  CHECK(errs[0].message.find("unreachable") != std::string::npos);
  std::cout << "testUnreachableAfterReturn OK\n";
  return 0;
}

static int testVoidParamRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: void) { print(1); }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("'void'") != std::string::npos);
  std::cout << "testVoidParamRejected OK\n";
  return 0;
}

static int testRecursionOk() {
  // Rekursi typecheck-OK ( runtime-nya tak diuji di sini ).
  bool parseOk = false;
  auto errs = checkSrc("fn f(n: i64) -> i64 { return f(n); }\n"
                       "fn main() { print(f(1)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testRecursionOk OK\n";
  return 0;
}

static int testCallResultType() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                       "fn main() { let b: bool = add(1, 2); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched types") != std::string::npos);
  std::cout << "testCallResultType OK\n";
  return 0;
}

static int testNonBoolIfCond() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { if 1 { print(1); } }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("if condition must be of type 'bool'") !=
        std::string::npos);
  std::cout << "testNonBoolIfCond OK\n";
  return 0;
}

static int testNonBoolWhileCond() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { while 5 { print(1); } }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("while condition must be of type 'bool'") !=
        std::string::npos);
  std::cout << "testNonBoolWhileCond OK\n";
  return 0;
}

static int testIfElseBothReturn() {
  // Tiap cabang return → fungsi non-void lengkap.
  bool parseOk = false;
  auto errs = checkSrc("fn f(c: bool) -> i64 {\n"
                       "    if c {\n"
                       "        return 1;\n"
                       "    } else {\n"
                       "        return 2;\n"
                       "    }\n"
                       "}\n"
                       "fn main() { print(f(true)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testIfElseBothReturn OK\n";
  return 0;
}

static int testIfNoElseMissingReturn() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(c: bool) -> i64 {\n"
                       "    if c {\n"
                       "        return 1;\n"
                       "    }\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("on every path") != std::string::npos);
  std::cout << "testIfNoElseMissingReturn OK\n";
  return 0;
}

static int testWhileReturnNotEnough() {
  // Loop bisa nol iterasi → tak dihitung sebagai return.
  bool parseOk = false;
  auto errs = checkSrc("fn f(c: bool) -> i64 {\n"
                       "    while c {\n"
                       "        return 1;\n"
                       "    }\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("on every path") != std::string::npos);
  std::cout << "testWhileReturnNotEnough OK\n";
  return 0;
}

static int testUnreachableInBranch() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { if true { return; print(1); } }",
                       parseOk);
  CHECK(parseOk);
  CHECK(!errs.empty());
  CHECK(errs[0].message.find("unreachable") != std::string::npos);
  std::cout << "testUnreachableInBranch OK\n";
  return 0;
}

static int testReturnValueInVoidBranch() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { if true { return 1; } }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("void function") != std::string::npos);
  std::cout << "testReturnValueInVoidBranch OK\n";
  return 0;
}

static int testAssignMismatch() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let n: i64 = 1; n = true; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched types in assignment") !=
        std::string::npos);
  std::cout << "testAssignMismatch OK\n";
  return 0;
}

static int testAssignOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let n = 1; n = n + 1; print(n); }",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAssignOk OK\n";
  return 0;
}

static int testMasterArrayOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() {\n"
                       "    let numbers: [i64] = [1, 2, 3, 4];\n"
                       "    print(numbers[0]);\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMasterArrayOk OK\n";
  return 0;
}

static int testArrayMismatch() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let x: [i64] = [true]; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  std::cout << "testArrayMismatch OK: " << errs[0].message << "\n";
  return 0;
}

static int testArrayMixedElems() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x = [1, true]; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched array element type") !=
        std::string::npos);
  std::cout << "testArrayMixedElems OK\n";
  return 0;
}

static int testEmptyArrayInferred() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x = []; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("empty array") != std::string::npos);
  std::cout << "testEmptyArrayInferred OK\n";
  return 0;
}

static int testNestedArrayRejected() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let x = [[1]]; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("nested arrays") != std::string::npos);
  std::cout << "testNestedArrayRejected OK\n";
  return 0;
}

static int testIndexNonArray() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x = 5; print(x[0]); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot index") != std::string::npos);
  std::cout << "testIndexNonArray OK\n";
  return 0;
}

static int testIndexNonInt() {
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let a = [1]; print(a[true]); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("must be of type 'i64'") != std::string::npos);
  std::cout << "testIndexNonInt OK\n";
  return 0;
}

static int testLenOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let a = [1, 2, 3]; print(len(a)); }",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLenOk OK\n";
  return 0;
}

static int testLenNonArray() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { print(len(5)); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("len() requires an array") !=
        std::string::npos);
  std::cout << "testLenNonArray OK\n";
  return 0;
}

static int testLenArity() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let a = [1]; print(len()); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("wrong number of arguments") !=
        std::string::npos);
  std::cout << "testLenArity OK\n";
  return 0;
}

static int testPrintArrayRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let a = [1]; print(a); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot print") != std::string::npos);
  std::cout << "testPrintArrayRejected OK\n";
  return 0;
}

static int testArrayEqualityRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let a = [1]; let b = a == a; }",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot compare arrays") != std::string::npos);
  std::cout << "testArrayEqualityRejected OK\n";
  return 0;
}

static int testArrayParamRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(a: [i64]) -> i64 { return a[0]; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("array parameters") != std::string::npos);
  std::cout << "testArrayParamRejected OK\n";
  return 0;
}

static int testArrayReturnRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn f() -> [i64] { return [1]; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("array return") != std::string::npos);
  std::cout << "testArrayReturnRejected OK\n";
  return 0;
}

static int testStructFieldTypesOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct Person { name: string; age: i64; }\n"
                       "fn main() {\n"
                       "    let p = Person { name: \"Nusa\", age: 1 };\n"
                       "    print(p.age);\n"
                       "    print(p.name);\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testStructFieldTypesOk OK\n";
  return 0;
}

static int testStructFieldMismatch() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { age: i64; }\n"
                       "fn main() { let p = P { age: \"x\" }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched field type") != std::string::npos);
  std::cout << "testStructFieldMismatch OK\n";
  return 0;
}

static int testUnknownFieldAccess() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "fn main() { let p = P { x: 1 }; print(p.y); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "unknown field 'y' for struct 'P'");
  std::cout << "testUnknownFieldAccess OK\n";
  return 0;
}

static int testFieldOnScalar() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x = 5; print(x.y); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot access field") != std::string::npos);
  std::cout << "testFieldOnScalar OK\n";
  return 0;
}

static int testUnknownType() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x: Ghost = 1; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "unknown type 'Ghost'");
  std::cout << "testUnknownType OK\n";
  return 0;
}

static int testPrintStructRejected() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "fn main() { let p = P { x: 1 }; print(p); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot print") != std::string::npos);
  std::cout << "testPrintStructRejected OK\n";
  return 0;
}

static int testStructEqualityRejected() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "fn main() {\n"
                       "    let a = P { x: 1 };\n"
                       "    let b = a == a;\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot compare structs") != std::string::npos);
  std::cout << "testStructEqualityRejected OK\n";
  return 0;
}

static int testStructParamOk() {
  // v1.2.0 (Phase 21d): struct by-value didukung — dulu ditolak.
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "fn f(p: P) -> i64 { return p.x; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testStructParamOk OK\n";
  return 0;
}

static int testStructReturnOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "fn mk(v: i64) -> P { return P { x: v }; }\n"
                       "fn main() { let p = mk(7); print(p.x); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testStructReturnOk OK\n";
  return 0;
}

static int testNestedStructOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct Inner { v: i64; }\n"
                       "struct Outer { inner: Inner; n: i64; }\n"
                       "fn main() {\n"
                       "    let o = Outer { inner: Inner { v: 7 }, n: 1 };\n"
                       "    print(o.inner.v);\n"
                       "    print(o.n);\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testNestedStructOk OK\n";
  return 0;
}

static int testDivByZeroLiteral() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { print(1 / 0); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "division by zero");
  std::cout << "testDivByZeroLiteral OK\n";
  return 0;
}

static int testDivByZeroDynamicOk() {
  // Pembagi variabel NOL: lolos statis (nilai runtime), ditangani
  // runtime check di codegen.
  bool parseOk = false;
  auto errs =
      checkSrc("fn main() { let z = 0; print(10 / z); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testDivByZeroDynamicOk OK\n";
  return 0;
}

// v1.2.0 (Phase 21a): alias tipe dikanonikalisasi ke targetnya.
static int testAliasBasicOk() {
  bool parseOk = false;
  auto errs = checkSrc("type Answer = i64;\n"
                       "fn main() { let x: Answer = 1; print(x); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAliasBasicOk OK\n";
  return 0;
}

static int testAliasForwardOk() {
  bool parseOk = false;
  auto errs = checkSrc("type B = A;\ntype A = i64;\n"
                       "fn main() { let x: B = 7; print(x); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAliasForwardOk OK\n";
  return 0;
}

static int testAliasChainOk() {
  bool parseOk = false;
  auto errs = checkSrc("type A = i64;\ntype B = A;\ntype C = [B];\n"
                       "fn main() { let x: C = [1]; print(x[0]); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAliasChainOk OK\n";
  return 0;
}

static int testAliasUnknownFails() {
  bool parseOk = false;
  auto errs = checkSrc("type A = Nope;\nfn main() { }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "unknown type 'Nope'");
  std::cout << "testAliasUnknownFails OK\n";
  return 0;
}

// Target rusak yang dipakai 2x tetap dilaporkan tepat sekali.
static int testAliasUnknownUsedOnce() {
  bool parseOk = false;
  auto errs = checkSrc("type A = Nope;\n"
                       "fn main() { let x: A = 1; let y: A = 2; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "unknown type 'Nope'");
  std::cout << "testAliasUnknownUsedOnce OK\n";
  return 0;
}

static int testAliasSelfCycleFails() {
  bool parseOk = false;
  auto errs = checkSrc("type A = A;\nfn main() { }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "circular type alias 'A'");
  std::cout << "testAliasSelfCycleFails OK\n";
  return 0;
}

static int testAliasMutualCycleFails() {
  bool parseOk = false;
  auto errs = checkSrc("type A = B;\ntype B = A;\nfn main() { }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("circular type alias") != std::string::npos);
  std::cout << "testAliasMutualCycleFails OK\n";
  return 0;
}

static int testAliasDeepChainFails() {
  std::string src;
  for (int i = 0; i < 105; i++)
    src += "type A" + std::to_string(i) + " = A" + std::to_string(i + 1) +
           ";\n";
  src += "type A105 = i64;\nfn main() { }\n";
  bool parseOk = false;
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("too deep") != std::string::npos);
  std::cout << "testAliasDeepChainFails OK\n";
  return 0;
}

// Duplikat alias di level checker (bypass semantic): error + first wins.
static int testAliasDupDirectFails() {
  Program prog;
  prog.file = "t.ns";
  TypeAliasDecl a1;
  a1.name = "A";
  a1.target = Type::i64();
  a1.loc = Location{"t.ns", 1, 1};
  TypeAliasDecl a2 = a1;
  a2.target = Type::boolean();
  a2.loc = Location{"t.ns", 2, 1};
  prog.aliases.push_back(a1);
  prog.aliases.push_back(a2);
  FunctionDecl main;
  main.name = "main";
  main.loc = Location{"t.ns", 3, 1};
  main.retType = Type::void_();
  prog.functions.push_back(std::move(main));
  TypeChecker checker;
  CHECK(!checker.check(prog));
  CHECK(checker.errors().size() == 1);
  CHECK(checker.errors()[0].message == "duplicate type alias 'A'");
  std::cout << "testAliasDupDirectFails OK\n";
  return 0;
}

// Tabrakan alias-vs-struct di level checker (bypass semantic).
static int testAliasStructCollisionDirectFails() {
  Program prog;
  prog.file = "t.ns";
  StructDecl sd;
  sd.name = "S";
  sd.loc = Location{"t.ns", 1, 1};
  prog.structs.push_back(sd);
  TypeAliasDecl ad;
  ad.name = "S";
  ad.target = Type::i64();
  ad.loc = Location{"t.ns", 2, 1};
  prog.aliases.push_back(ad);
  FunctionDecl main;
  main.name = "main";
  main.loc = Location{"t.ns", 3, 1};
  main.retType = Type::void_();
  prog.functions.push_back(std::move(main));
  TypeChecker checker;
  CHECK(!checker.check(prog));
  bool found = false;
  for (const auto &e : checker.errors()) {
    if (e.message == "duplicate type 'S'")
      found = true;
  }
  CHECK(found);
  std::cout << "testAliasStructCollisionDirectFails OK\n";
  return 0;
}

// Alias array sebagai param tetap ditolak aturan array (kanonis bekerja).
static int testAliasArrayParamRejected() {
  bool parseOk = false;
  auto errs = checkSrc("type Nums = [i64];\n"
                       "fn f(a: Nums) -> i64 { return 1; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  bool found = false;
  for (const auto &e : errs) {
    if (e.message.find("array parameters are not supported") !=
        std::string::npos)
      found = true;
  }
  CHECK(found);
  std::cout << "testAliasArrayParamRejected OK\n";
  return 0;
}

// Field struct via alias i64 lolos (kanonis bekerja di layout).
static int testAliasStructFieldOk() {
  bool parseOk = false;
  auto errs = checkSrc("type Age = i64;\nstruct P { age: Age; }\n"
                       "fn main() { let p = P { age: 1 }; print(p.age); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAliasStructFieldOk OK\n";
  return 0;
}

// Alias void sebagai return lolos (aturan void berlaku).
static int testAliasVoidReturnOk() {
  bool parseOk = false;
  auto errs = checkSrc("type V = void;\nfn f() -> V { }\nfn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAliasVoidReturnOk OK\n";
  return 0;
}

// v1.2.0 (Phase 21b): function reference + indirect call.
static int testFnRefOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                       "fn apply(f: fn(i64, i64) -> i64, x: i64, y: i64) "
                       "-> i64 { return f(x, y); }\n"
                       "fn main() { let op = add; print(op(1, 2)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testFnRefOk OK\n";
  return 0;
}

static int testFnIndirectArityFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                       "fn main() { let f = add; print(f(1)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("wrong number of arguments for 'f'") !=
        std::string::npos);
  std::cout << "testFnIndirectArityFails OK\n";
  return 0;
}

static int testFnIndirectMismatchFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64) -> i64 { return a; }\n"
                       "fn main() { let f = add; print(f(\"s\")); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched argument type for 'f'") !=
        std::string::npos);
  std::cout << "testFnIndirectMismatchFails OK\n";
  return 0;
}

static int testCallNonFnFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let x = 5; print(x(1)); }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot call 'x' of type 'i64'");
  std::cout << "testCallNonFnFails OK\n";
  return 0;
}

static int testFnVoidValueOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn v() { print(1); }\n"
                       "fn run(g: fn() -> void) { g(); }\n"
                       "fn main() { run(v); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testFnVoidValueOk OK\n";
  return 0;
}

static int testFnReturnFnOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                       "fn mk() -> fn(i64, i64) -> i64 { return add; }\n"
                       "fn main() { let f = mk(); print(f(1, 2)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testFnReturnFnOk OK\n";
  return 0;
}

static int testFnPrintRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64) -> i64 { return a; }\n"
                       "fn main() { print(add); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot print value of type 'fn(i64) -> i64'") !=
        std::string::npos);
  std::cout << "testFnPrintRejected OK\n";
  return 0;
}

static int testFnCompareRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64) -> i64 { return a; }\n"
                       "fn main() { let f = add; print(f == f); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot compare functions") !=
        std::string::npos);
  std::cout << "testFnCompareRejected OK\n";
  return 0;
}

static int testFnArrayRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn add(a: i64) -> i64 { return a; }\n"
                       "fn main() { let fs = [add]; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("arrays of functions") != std::string::npos);
  std::cout << "testFnArrayRejected OK\n";
  return 0;
}

static int testFnFieldRejected() {
  bool parseOk = false;
  auto errs = checkSrc("fn cb() -> i64 { return 1; }\n"
                       "struct S { f: fn() -> i64; }\nfn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("function fields are not supported") !=
        std::string::npos);
  std::cout << "testFnFieldRejected OK\n";
  return 0;
}

// v1.2.0 (Phase 21c): monomorphisasi fungsi generik.
static int testGenericIdentityOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn identity<T>(value: T) -> T { return value; }\n"
                       "fn main() { let a = identity(1); print(a); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericIdentityOk OK\n";
  return 0;
}

static int testGenericConflictFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f<T>(a: T, b: T) -> T { return a; }\n"
                       "fn main() { print(f(1, \"s\")); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched argument type for 'f'") !=
        std::string::npos);
  std::cout << "testGenericConflictFails OK\n";
  return 0;
}

static int testGenericUninferrableFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn zero<T>() -> T { return 1; }\n"
                       "fn main() { print(zero()); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot infer type argument 'T'") !=
        std::string::npos);
  std::cout << "testGenericUninferrableFails OK\n";
  return 0;
}

static int testGenericArityFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f<T>(a: T) -> T { return a; }\n"
                       "fn main() { print(f(1, 2)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("wrong number of arguments for 'f'") !=
        std::string::npos);
  std::cout << "testGenericArityFails OK\n";
  return 0;
}

static int testGenericAsValueFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f<T>(x: T) -> T { return x; }\n"
                       "fn main() { let g = f; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot use generic function 'f' as a value") !=
        std::string::npos);
  std::cout << "testGenericAsValueFails OK\n";
  return 0;
}

// Generik memanggil generik (instansiasi bersarang).
static int testGenericNestedOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn f<T>(x: T) -> T { return x; }\n"
                       "fn g<T>(x: T) -> T { return f(x); }\n"
                       "fn main() { print(g(1)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericNestedOk OK\n";
  return 0;
}

// Rekursi tipe-sama memakai instance yang sama (cache).
static int testGenericRecursionOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn f<T>(x: T) -> T { return f(x); }\n"
                       "fn main() { print(f(1)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericRecursionOk OK\n";
  return 0;
}

// Generik higher-order: unifikasi menembus tipe fn.
static int testGenericHigherOrderOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn apply<T, U>(f: fn(T) -> U, x: T) -> U { "
                       "return f(x); }\n"
                       "fn add1(x: i64) -> i64 { return x + 1; }\n"
                       "fn main() { print(apply(add1, 10)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericHigherOrderOk OK\n";
  return 0;
}

// Argumen struct: by-value didukung (Phase 21d) — dulu ditolak ABI.
static int testGenericStructArgOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "fn id<T>(v: T) -> T { return v; }\n"
                       "fn main() { let p = P { x: 1 }; print(id(p).x); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericStructArgOk OK\n";
  return 0;
}

// v1.2.0 (Phase 21d): inferensi + instance struct generik.
static int testGenericStructInferOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct Point<T> { x: T; y: T; }\n"
                       "fn main() { let p = Point { x: 1, y: 2 }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericStructInferOk OK\n";
  return 0;
}

static int testGenericStructConflictFails() {
  bool parseOk = false;
  auto errs = checkSrc("struct P<T> { x: T; y: T; }\n"
                       "fn main() { let p = P { x: 1, y: \"s\" }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched field type: 'P.y'") !=
        std::string::npos);
  std::cout << "testGenericStructConflictFails OK\n";
  return 0;
}

static int testGenericStructUninferrableFails() {
  bool parseOk = false;
  auto errs = checkSrc("struct P<T, U> { x: T; }\n"
                       "fn main() { let p = P { x: 1 }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot infer type argument 'U'") !=
        std::string::npos);
  std::cout << "testGenericStructUninferrableFails OK\n";
  return 0;
}

static int testGenericStructExplicitOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct Point<T> { x: T; y: T; }\n"
                       "fn main() { let p: Point<i64> = "
                       "Point<i64> { x: 1, y: 2 }; print(p.x); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericStructExplicitOk OK\n";
  return 0;
}

static int testGenericStructExplicitArityFails() {
  bool parseOk = false;
  auto errs = checkSrc("struct P<T> { x: T; }\n"
                       "fn main() { let p = P<i64, i64> { x: 1 }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("wrong number of type arguments") !=
        std::string::npos);
  std::cout << "testGenericStructExplicitArityFails OK\n";
  return 0;
}

static int testGenericStructBareFails() {
  bool parseOk = false;
  auto errs = checkSrc("struct P<T> { x: T; }\n"
                       "fn main() { let p: P = P { x: 1 }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(!errs.empty());
  bool found = false;
  for (const auto &e : errs) {
    if (e.message.find("used without type arguments") != std::string::npos)
      found = true;
  }
  CHECK(found);
  std::cout << "testGenericStructBareFails OK\n";
  return 0;
}

static int testNonGenericWithArgsFails() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "fn main() { let p: P<i64> = P { x: 1 }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("is not generic") != std::string::npos);
  std::cout << "testNonGenericWithArgsFails OK\n";
  return 0;
}

static int testGenericStructInfiniteFails() {
  bool parseOk = false;
  auto errs = checkSrc("struct L<T> { tail: L<T>; }\n"
                       "fn mk() -> L<i64> { return mk(); }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("has infinite size") != std::string::npos);
  std::cout << "testGenericStructInfiniteFails OK\n";
  return 0;
}

static int testGenericStructNestedOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct Point<T> { x: T; y: T; }\n"
                       "struct Line<T> { a: Point<T>; b: Point<T>; }\n"
                       "fn main() { let l = Line { a: Point { x: 1, y: 2 }, "
                       "b: Point { x: 3, y: 4 } }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericStructNestedOk OK\n";
  return 0;
}

// Fungsi generik atas struct generik (by-value).
static int testGenericStructFnOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct Point<T> { x: T; y: T; }\n"
                       "fn getx<T>(p: Point<T>) -> T { return p.x; }\n"
                       "fn main() { let q = Point { x: 9, y: 8 }; "
                       "print(getx(q)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testGenericStructFnOk OK\n";
  return 0;
}

// v1.3.0 (Phase 22): contoh master (trait + impl + method call).
static int testTraitMasterOk() {
  bool parseOk = false;
  auto errs = checkSrc("trait Printable { fn print(self); "
                       "fn describe(self) -> string; }\n"
                       "struct Person { name: string; }\n"
                       "impl Printable for Person {\n"
                       "fn print(self) { print(self.name); }\n"
                       "fn describe(self) -> string { return self.name; } }\n"
                       "fn main() { let p = Person { name: \"N\" }; "
                       "p.print(); print(p.describe()); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testTraitMasterOk OK\n";
  return 0;
}

static int testTraitMismatchRetFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m(self) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "impl P for A { fn m(self) -> string { return \"x\"; } }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatches trait") != std::string::npos);
  std::cout << "testTraitMismatchRetFails OK\n";
  return 0;
}

static int testTraitMismatchParamFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m(self, x: i64) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "impl P for A { fn m(self, x: bool) -> i64 { return 1; } }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatches trait") != std::string::npos);
  std::cout << "testTraitMismatchParamFails OK\n";
  return 0;
}

static int testTraitMissingMethodFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m(self) -> i64; fn n(self) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "impl P for A { fn m(self) -> i64 { return 1; } }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("missing method 'n'") != std::string::npos);
  std::cout << "testTraitMissingMethodFails OK\n";
  return 0;
}

static int testTraitExtraMethodFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m(self) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "impl P for A { fn z(self) -> i64 { return 1; } }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  // 'z' bukan anggota + 'm' hilang.
  CHECK(errs.size() == 2);
  std::cout << "testTraitExtraMethodFails OK\n";
  return 0;
}

static int testTraitDupImplFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m(self) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "impl P for A { fn m(self) -> i64 { return 1; } }\n"
                       "impl P for A { fn m(self) -> i64 { return 2; } }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("duplicate impl") != std::string::npos);
  std::cout << "testTraitDupImplFails OK\n";
  return 0;
}

static int testTraitNonSelfFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m(x: i64) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("must be 'self'") != std::string::npos);
  std::cout << "testTraitNonSelfFails OK\n";
  return 0;
}

static int testTraitUnknownMethodFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m(self) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "impl P for A { fn m(self) -> i64 { return 1; } }\n"
                       "fn main() { let a = A { x: 1 }; print(a.z()); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("unknown method 'z'") != std::string::npos);
  std::cout << "testTraitUnknownMethodFails OK\n";
  return 0;
}

static int testTraitAmbiguousFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait A { fn m(self) -> i64; }\n"
                       "trait B { fn m(self) -> i64; }\n"
                       "struct P { x: i64; }\n"
                       "impl A for P { fn m(self) -> i64 { return 1; } }\n"
                       "impl B for P { fn m(self) -> i64 { return 2; } }\n"
                       "fn main() { let p = P { x: 0 }; print(p.m()); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("ambiguous method") != std::string::npos);
  std::cout << "testTraitAmbiguousFails OK\n";
  return 0;
}

static int testTraitPrimOk() {
  bool parseOk = false;
  // Impl primitif lolos checker (runtime di e2e).
  auto errs = checkSrc("trait P { fn show(self) -> string; }\n"
                       "impl P for i64 { fn show(self) -> string { return \"n\"; } }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testTraitPrimOk OK\n";
  return 0;
}

static int testTraitGenericInstanceOk() {
  bool parseOk = false;
  auto errs = checkSrc("struct Point<T> { x: T; y: T; }\n"
                       "trait D { fn sum(self) -> i64; }\n"
                       "impl D for Point<i64> { fn sum(self) -> i64 { "
                       "return self.x + self.y; } }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testTraitGenericInstanceOk OK\n";
  return 0;
}

static int testTraitBoundOk() {
  bool parseOk = false;
  auto errs = checkSrc("trait Printable { fn show(self) -> string; }\n"
                       "struct A { x: i64; }\n"
                       "impl Printable for A { fn show(self) -> string { "
                       "return \"A\"; } }\n"
                       "fn f<T: Printable>(v: T) -> string { return v.show(); }\n"
                       "fn main() { print(f(A { x: 1 })); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testTraitBoundOk OK\n";
  return 0;
}

static int testTraitBoundMultiOk() {
  bool parseOk = false;
  auto errs = checkSrc("trait A { fn a(self) -> i64; }\n"
                       "trait B { fn b(self) -> i64; }\n"
                       "struct P { x: i64; }\n"
                       "impl A for P { fn a(self) -> i64 { return 1; } }\n"
                       "impl B for P { fn b(self) -> i64 { return 2; } }\n"
                       "fn f<T: A + B>(v: T) -> i64 { return v.a() + v.b(); }\n"
                       "fn main() { print(f(P { x: 0 })); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testTraitBoundMultiOk OK\n";
  return 0;
}

static int testTraitBoundFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait Printable { fn show(self) -> string; }\n"
                       "struct B { y: i64; }\n"
                       "fn f<T: Printable>(v: T) -> string { return v.show(); }\n"
                       "fn main() { print(f(B { y: 1 })); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("does not implement trait") !=
        std::string::npos);
  std::cout << "testTraitBoundFails OK\n";
  return 0;
}

static int testTraitBoundStructOk() {
  bool parseOk = false;
  auto errs = checkSrc("trait A { fn a(self) -> i64; }\n"
                       "struct P { x: i64; }\n"
                       "impl A for P { fn a(self) -> i64 { return self.x; } }\n"
                       "struct Box<T: A> { v: T; }\n"
                       "fn main() { let b: Box<P> = Box { v: P { x: 1 } }; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testTraitBoundStructOk OK\n";
  return 0;
}

static int testTraitGenericMethodFails() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn m<T>(self, x: T) -> T; }\n"
                       "struct A { x: i64; }\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("generic methods are not supported") !=
        std::string::npos);
  std::cout << "testTraitGenericMethodFails OK\n";
  return 0;
}

static int testTraitMethodAsFieldHint() {
  bool parseOk = false;
  auto errs = checkSrc("trait P { fn get(self) -> i64; }\n"
                       "struct A { x: i64; }\n"
                       "impl P for A { fn get(self) -> i64 { return self.x; } }\n"
                       "fn main() { let p = A { x: 5 }; print(p.get); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("unknown field 'get'") != std::string::npos);
  std::cout << "testTraitMethodAsFieldHint OK\n";
  return 0;
}

// v1.5.0 (Phase 24): propagasi `?` (Result lokal; std di e2e).
static const char *kResultDecl =
    "enum Result<T, E> { Ok(T), Err(E) }\n";

static int testPropagateOk() {
  bool parseOk = false;
  std::string src = std::string(kResultDecl) +
                    "fn f(x: i64) -> Result<i64, string> {\n"
                    "    if x < 0 { return Result<i64, string>.Err(\"neg\"); }\n"
                    "    return Result<i64, string>.Ok(x);\n"
                    "}\n"
                    "fn g(x: i64) -> Result<i64, string> {\n"
                    "    let y = f(x)?;\n"
                    "    return Result<i64, string>.Ok(y + 1);\n"
                    "}\n"
                    "fn main() { }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testPropagateOk OK\n";
  return 0;
}

static int testPropagateNonResultFails() {
  bool parseOk = false;
  std::string src = std::string(kResultDecl) +
                    "fn main() { let x = 5; print(x?); }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("with '?'") != std::string::npos);
  std::cout << "testPropagateNonResultFails OK\n";
  return 0;
}

static int testPropagateVoidFnFails() {
  bool parseOk = false;
  std::string src = std::string(kResultDecl) +
                    "fn f() -> Result<i64, string> {\n"
                    "    return Result<i64, string>.Ok(1);\n"
                    "}\n"
                    "fn main() { print(f()?); }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("returning 'void'") != std::string::npos);
  std::cout << "testPropagateVoidFnFails OK\n";
  return 0;
}

static int testPropagateMismatchFails() {
  bool parseOk = false;
  std::string src = std::string(kResultDecl) +
                    "fn f() -> Result<i64, string> {\n"
                    "    return Result<i64, string>.Ok(1);\n"
                    "}\n"
                    "fn g() -> Result<i64, i64> {\n"
                    "    let x = f()?;\n"
                    "    return Result<i64, i64>.Ok(x);\n"
                    "}\n"
                    "fn main() { }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("in function returning") != std::string::npos);
  std::cout << "testPropagateMismatchFails OK\n";
  return 0;
}

static int testPropagateStmtOk() {
  bool parseOk = false;
  std::string src = std::string(kResultDecl) +
                    "fn f() -> Result<i64, string> {\n"
                    "    return Result<i64, string>.Ok(1);\n"
                    "}\n"
                    "fn g() -> Result<i64, string> {\n"
                    "    f()?;\n"
                    "    return Result<i64, string>.Ok(2);\n"
                    "}\n"
                    "fn main() { }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testPropagateStmtOk OK\n";
  return 0;
}

// v2.0 (B1): Option/Result natif — enum generik + ctor qualified.
static const char *kOptionDecl =
    "enum Option<T> { Some(T), None }\n";

static int testMatchIntOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        1 => { return 10; },\n"
                       "        2 => { return 20; },\n"
                       "        _ => { return 30; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMatchIntOk OK\n";
  return 0;
}

static int testMatchBoolFullOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(b: bool) -> i64 {\n"
                       "    match b {\n"
                       "        true => { return 1; },\n"
                       "        false => { return 0; },\n"
                       "    }\n"
                       "    return 9;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMatchBoolFullOk OK\n";
  return 0;
}

static int testMatchBindOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        n => { return n; },\n"
                       "    }\n"
                       "    return 9;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMatchBindOk OK\n";
  return 0;
}

static int testMatchGuardOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        n if n > 10 => { return 1; },\n"
                       "        _ => { return 0; },\n"
                       "    }\n"
                       "    return 9;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMatchGuardOk OK\n";
  return 0;
}

static int testMatchOptionFullOk() {
  bool parseOk = false;
  std::string src =
      std::string(kOptionDecl) +
      "fn f(o: Option<i64>) -> i64 {\n"
      "    match o {\n"
      "        Some(v) => { return v; },\n"
      "        None => { return 0 - 1; },\n"
      "    }\n"
      "    return 9;\n"
      "}\n"
      "fn main() { }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMatchOptionFullOk OK\n";
  return 0;
}

static int testMatchResultFullOk() {  bool parseOk = false;
  std::string src =
      std::string(kResultDecl) +
      "fn f(r: Result<i64, string>) -> string {\n"
      "    match r {\n"
      "        Ok(v) => { return \"ok\"; },\n"
      "        Err(e) => { return e; },\n"
      "    }\n"
      "    return \"?\";\n"
      "}\n"
      "fn main() { }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMatchResultFullOk OK\n";
  return 0;
}

// v2.0 (B1): encoding struct v1.x untuk varian DITOLAK (breaking).
static int testMatchOptionStructRejects() {
  bool parseOk = false;
  auto errs = checkSrc("struct Option { has_value: bool; value: i64; }\n"
                       "fn f(o: Option) -> i64 {\n"
                       "    match o {\n"
                       "        Some(v) => { return v; },\n"
                       "        None => { return 0 - 1; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 2);
  CHECK(errs[0].message.find("cannot match variant 'Some'") !=
        std::string::npos);
  CHECK(errs[1].message.find("cannot match variant 'None'") !=
        std::string::npos);
  std::cout << "testMatchOptionStructRejects OK\n";
  return 0;
}

static int testMatchNonExhaustiveFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        1 => { return 1; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("non-exhaustive") != std::string::npos);
  std::cout << "testMatchNonExhaustiveFails OK\n";
  return 0;
}

static int testMatchUnreachableFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        _ => { return 1; },\n"
                       "        2 => { return 2; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("unreachable pattern") != std::string::npos);
  std::cout << "testMatchUnreachableFails OK\n";
  return 0;
}

static int testMatchDupLitFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        1 => { return 1; },\n"
                       "        1 => { return 2; },\n"
                       "        _ => { return 3; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("unreachable pattern") != std::string::npos);
  std::cout << "testMatchDupLitFails OK\n";
  return 0;
}

static int testMatchBoolMissingFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(b: bool) -> i64 {\n"
                       "    match b {\n"
                       "        true => { return 1; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("missing pattern for 'false'") !=
        std::string::npos);
  std::cout << "testMatchBoolMissingFails OK\n";
  return 0;
}

static int testMatchTypeMismatchFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        \"a\" => { return 1; },\n"
                       "        _ => { return 2; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("invalid pattern") != std::string::npos);
  std::cout << "testMatchTypeMismatchFails OK\n";
  return 0;
}

static int testMatchGuardNonBoolFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        n if n => { return 1; },\n"
                       "        _ => { return 2; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("must be of type 'bool'") != std::string::npos);
  std::cout << "testMatchGuardNonBoolFails OK\n";
  return 0;
}

static int testMatchGuardedWildNotExhaustiveFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        _ if x > 0 => { return 1; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("non-exhaustive") != std::string::npos);
  std::cout << "testMatchGuardedWildNotExhaustiveFails OK\n";
  return 0;
}

static int testMatchUnknownVariantFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn f(x: i64) -> i64 {\n"
                       "    match x {\n"
                       "        Red(1) => { return 1; },\n"
                       "        _ => { return 2; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot match variant 'Red'") !=
        std::string::npos);
  std::cout << "testMatchUnknownVariantFails OK\n";
  return 0;
}

// v1.7.0 (Phase 26): enum & algebraic data types.
static const char *kColorDecl = "enum Color { Red, Green, Blue }\n";

static int testEnumDeclCtorMatchOk() {
  bool parseOk = false;
  std::string src =
      std::string(kColorDecl) +
      "fn is_red(c: Color) -> i64 {\n"
      "    match c {\n"
      "        Red => { return 1; },\n"
      "        Green => { return 0; },\n"
      "        Blue => { return 0; },\n"
      "    }\n"
      "    return 9;\n"
      "}\n"
      "fn main() { let c: Color = Color.Red; print(is_red(c)); }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testEnumDeclCtorMatchOk OK\n";
  return 0;
}

static int testEnumPayloadOk() {
  bool parseOk = false;
  auto errs = checkSrc("enum Shape { Circle(i64), Rect(i64, i64), Dot }\n"
                       "fn tag(s: Shape) -> i64 {\n"
                       "    match s {\n"
                       "        Circle(r) => { return r; },\n"
                       "        Rect(w, h) => { return w; },\n"
                       "        Dot => { return 0 - 1; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { let s: Shape = Shape.Circle(5); print(tag(s)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testEnumPayloadOk OK\n";
  return 0;
}

static int testEnumGenericInferOk() {
  bool parseOk = false;
  std::string src =
      "enum Wrap<T> { Val(T), Empty }\n"
      "fn get(w: Wrap<i64>) -> i64 {\n"
      "    match w {\n"
      "        Val(v) => { return v; },\n"
      "        Empty => { return 0 - 1; },\n"
      "    }\n"
      "    return 0;\n"
      "}\n"
      "fn main() { print(get(Wrap.Val(7))); }\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testEnumGenericInferOk OK\n";
  return 0;
}

static int testEnumExplicitTargsOk() {
  bool parseOk = false;
  auto errs = checkSrc("enum Wrap<T> { Val(T), Empty }\n"
                       "fn get(w: Wrap<i64>) -> i64 {\n"
                       "    match w {\n"
                       "        Val(v) => { return v; },\n"
                       "        Empty => { return 0 - 1; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { print(get(Wrap<i64>.Empty)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testEnumExplicitTargsOk OK\n";
  return 0;
}

static int testEnumArityFails() {
  bool parseOk = false;
  auto errs = checkSrc("enum Shape { Circle(i64), Dot }\n"
                       "fn main() { let s: Shape = Shape.Circle(1, 2); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("expects 1 argument(s)") != std::string::npos);
  std::cout << "testEnumArityFails OK\n";
  return 0;
}

static int testEnumUnknownVariantFails() {
  bool parseOk = false;
  auto errs = checkSrc(std::string(kColorDecl) +
                       "fn main() { let c: Color = Color.Yellow; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("unknown variant 'Yellow'") !=
        std::string::npos);
  std::cout << "testEnumUnknownVariantFails OK\n";
  return 0;
}

static int testEnumNonExhaustiveFails() {
  bool parseOk = false;
  auto errs = checkSrc(std::string(kColorDecl) +
                       "fn f(c: Color) -> i64 {\n"
                       "    match c {\n"
                       "        Red => { return 1; },\n"
                       "        Green => { return 2; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("missing pattern(s) for 'Blue'") !=
        std::string::npos);
  std::cout << "testEnumNonExhaustiveFails OK\n";
  return 0;
}

static int testEnumPayloadMismatchFails() {
  bool parseOk = false;
  auto errs = checkSrc("enum Shape { Circle(i64), Dot }\n"
                       "fn main() { let s: Shape = Shape.Circle(\"x\"); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched variant payload") !=
        std::string::npos);
  std::cout << "testEnumPayloadMismatchFails OK\n";
  return 0;
}

static int testEnumForgeFails() {
  bool parseOk = false;
  // Semantic menolak duluan (parseOk=false Ishak); pastikan checker
  // defensif tak crash bila semantic dilewati — di sini cukup pesan.
  auto errs = checkSrc(std::string(kColorDecl) +
                       "fn main() { let c: Color = Color { tag: 0 }; print(c); }\n",
                       parseOk);
  CHECK(!parseOk || !errs.empty());
  std::cout << "testEnumForgeFails OK\n";
  return 0;
}

static int testEnumInferFails() {
  bool parseOk = false;
  auto errs = checkSrc("enum Wrap<T> { Val(T), Empty }\n"
                       "fn get(w: Wrap<i64>) -> i64 {\n"
                       "    match w {\n"
                       "        Val(v) => { return v; },\n"
                       "        Empty => { return 0 - 1; },\n"
                       "    }\n"
                       "    return 0;\n"
                       "}\n"
                       "fn main() { print(get(Wrap.Empty)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot infer type argument 'T'") !=
        std::string::npos);
  std::cout << "testEnumInferFails OK\n";
  return 0;
}

// v1.8.0 (Phase 27): closures.
static int testLambdaAnnotatedOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn apply(f: fn(i64) -> i64, x: i64) -> i64 {\n"
                       "    return f(x);\n"
                       "}\n"
                       "fn main() { print(apply(|x: i64| x + 1, 41)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLambdaAnnotatedOk OK\n";
  return 0;
}

static int testLambdaUnannotatedPropagatedOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn apply(f: fn(i64) -> i64, x: i64) -> i64 {\n"
                       "    return f(x);\n"
                       "}\n"
                       "fn main() {\n"
                       "    let k = 10;\n"
                       "    print(apply(|x| x + k, 1));\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLambdaUnannotatedPropagatedOk OK\n";
  return 0;
}

static int testLambdaLetAnnotatedOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() {\n"
                       "    let f: fn(i64) -> i64 = |x| x * 2;\n"
                       "    print(f(21));\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLambdaLetAnnotatedOk OK\n";
  return 0;
}

static int testLambdaGenericCombinatorOk() {
  bool parseOk = false;
  std::string src =
      std::string(kOptionDecl) +
      "fn opt_map<T, U>(o: Option<T>, f: fn(T) -> U) -> Option<U> {\n"
      "    match o {\n"
      "        Some(v) => { return Option<U>.Some(f(v)); },\n"
      "        None => { return Option<U>.None(); },\n"
      "    }\n"
      "    return Option<U>.None();\n"
      "}\n"
      "fn main() {\n"
      "    let o: Option<i64> = Option<i64>.Some(21);\n"
      "    opt_map(o, |x| x * 2);\n"
      "}\n";
  auto errs = checkSrc(src, parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLambdaGenericCombinatorOk OK\n";
  return 0;
}

static int testLambdaNoContextFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() { let g = |x| x + 1; }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot infer type of lambda parameter") !=
        std::string::npos);
  std::cout << "testLambdaNoContextFails OK\n";
  return 0;
}

static int testLambdaArityFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn apply(f: fn(i64) -> i64, x: i64) -> i64 {\n"
                       "    return f(x);\n"
                       "}\n"
                       "fn main() { print(apply(|x: i64, y: i64| x, 1)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("lambda expects 1 parameter(s)") !=
        std::string::npos);
  std::cout << "testLambdaArityFails OK\n";
  return 0;
}

static int testLambdaRetMismatchFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn apply(f: fn(i64) -> i64, x: i64) -> i64 {\n"
                       "    return f(x);\n"
                       "}\n"
                       "fn main() { print(apply(|x: i64| \"s\", 1)); }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("mismatched return type") != std::string::npos);
  std::cout << "testLambdaRetMismatchFails OK\n";
  return 0;
}

static int testLambdaArrayCaptureFails() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() {\n"
                       "    let a: [i64] = [1, 2];\n"
                       "    let f: fn(i64) -> i64 = |x: i64| x + a[0];\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot capture array") != std::string::npos);
  std::cout << "testLambdaArrayCaptureFails OK\n";
  return 0;
}

static int testLambdaNestedOk() {
  bool parseOk = false;
  auto errs = checkSrc("fn main() {\n"
                       "    let mk = |a: i64| |b: i64| a + b;\n"
                       "    let add5 = mk(5);\n"
                       "    print(add5(3));\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLambdaNestedOk OK\n";
  return 0;
}

// v1.13.0 (Phase 32): FFI.
static int testExternSigOk() {
  bool parseOk = false;
  auto errs = checkSrc("extern fn strlen(s: string) -> i64;\n"
                       "extern fn isupper(c: i64) -> bool;\n"
                       "extern fn flush() -> void;\n"
                       "fn main() {\n"
                       "    print(strlen(\"ab\"));\n"
                       "    print(isupper(65));\n"
                       "    flush();\n"
                       "}\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testExternSigOk OK\n";
  return 0;
}

static int testExternBadParamFails() {
  bool parseOk = false;
  auto errs = checkSrc("struct P { x: i64; }\n"
                       "extern fn f(p: P) -> i64;\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot have type 'P'") != std::string::npos);
  std::cout << "testExternBadParamFails OK\n";
  return 0;
}

static int testExternStringRetFails() {
  bool parseOk = false;
  auto errs = checkSrc("extern fn f() -> string;\n"
                       "fn main() { }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot return 'string'") != std::string::npos);
  std::cout << "testExternStringRetFails OK\n";
  return 0;
}

static int testExternAsValueFails() {
  bool parseOk = false;
  auto errs = checkSrc("extern fn strlen(s: string) -> i64;\n"
                       "fn main() { let f = strlen; }\n",
                       parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("cannot use extern function") !=
        std::string::npos);
  std::cout << "testExternAsValueFails OK\n";
  return 0;
}

int main() {
  if (testMasterAnnotationsOk()) return 1;
  if (testMismatchRejected()) return 1;
  if (testConstEvalOk()) return 1;
  if (testConstMismatch()) return 1;
  if (testConstCycle()) return 1;
  if (testConstDivZero()) return 1;
  if (testConstNonConst()) return 1;
  if (testBoolMismatch()) return 1;
  if (testVoidVarRejected()) return 1;
  if (testInference()) return 1;
  if (testArithMismatch()) return 1;
  if (testCompareMismatch()) return 1;
  if (testStringEqualityRejected()) return 1;
  if (testUnaryMismatch()) return 1;
  if (testEqMismatch()) return 1;
  if (testMainFuncType()) return 1;
  if (testCallOk()) return 1;
  if (testArityMismatch()) return 1;
  if (testArgTypeMismatch()) return 1;
  if (testReturnMismatch()) return 1;
  if (testMissingReturn()) return 1;
  if (testReturnValueInVoid()) return 1;
  if (testBareReturnInNonVoid()) return 1;
  if (testUnreachableAfterReturn()) return 1;
  if (testVoidParamRejected()) return 1;
  if (testRecursionOk()) return 1;
  if (testCallResultType()) return 1;
  if (testNonBoolIfCond()) return 1;
  if (testNonBoolWhileCond()) return 1;
  if (testIfElseBothReturn()) return 1;
  if (testIfNoElseMissingReturn()) return 1;
  if (testWhileReturnNotEnough()) return 1;
  if (testUnreachableInBranch()) return 1;
  if (testReturnValueInVoidBranch()) return 1;
  if (testAssignMismatch()) return 1;
  if (testAssignOk()) return 1;
  if (testMasterArrayOk()) return 1;
  if (testArrayMismatch()) return 1;
  if (testArrayMixedElems()) return 1;
  if (testEmptyArrayInferred()) return 1;
  if (testNestedArrayRejected()) return 1;
  if (testIndexNonArray()) return 1;
  if (testIndexNonInt()) return 1;
  if (testLenOk()) return 1;
  if (testLenNonArray()) return 1;
  if (testLenArity()) return 1;
  if (testPrintArrayRejected()) return 1;
  if (testArrayEqualityRejected()) return 1;
  if (testArrayParamRejected()) return 1;
  if (testArrayReturnRejected()) return 1;
  if (testStructFieldTypesOk()) return 1;
  if (testStructFieldMismatch()) return 1;
  if (testUnknownFieldAccess()) return 1;
  if (testFieldOnScalar()) return 1;
  if (testUnknownType()) return 1;
  if (testPrintStructRejected()) return 1;
  if (testStructEqualityRejected()) return 1;
  if (testStructParamOk()) return 1;
  if (testStructReturnOk()) return 1;
  if (testNestedStructOk()) return 1;
  if (testDivByZeroLiteral()) return 1;
  if (testDivByZeroDynamicOk()) return 1;
  if (testAliasBasicOk()) return 1;
  if (testAliasForwardOk()) return 1;
  if (testAliasChainOk()) return 1;
  if (testAliasUnknownFails()) return 1;
  if (testAliasUnknownUsedOnce()) return 1;
  if (testAliasSelfCycleFails()) return 1;
  if (testAliasMutualCycleFails()) return 1;
  if (testAliasDeepChainFails()) return 1;
  if (testAliasDupDirectFails()) return 1;
  if (testAliasStructCollisionDirectFails()) return 1;
  if (testAliasArrayParamRejected()) return 1;
  if (testAliasStructFieldOk()) return 1;
  if (testAliasVoidReturnOk()) return 1;
  if (testFnRefOk()) return 1;
  if (testFnIndirectArityFails()) return 1;
  if (testFnIndirectMismatchFails()) return 1;
  if (testCallNonFnFails()) return 1;
  if (testFnVoidValueOk()) return 1;
  if (testFnReturnFnOk()) return 1;
  if (testFnPrintRejected()) return 1;
  if (testFnCompareRejected()) return 1;
  if (testFnArrayRejected()) return 1;
  if (testFnFieldRejected()) return 1;
  if (testGenericIdentityOk()) return 1;
  if (testGenericConflictFails()) return 1;
  if (testGenericUninferrableFails()) return 1;
  if (testGenericArityFails()) return 1;
  if (testGenericAsValueFails()) return 1;
  if (testGenericNestedOk()) return 1;
  if (testGenericRecursionOk()) return 1;
  if (testGenericHigherOrderOk()) return 1;
  if (testGenericStructArgOk()) return 1;
  if (testGenericStructInferOk()) return 1;
  if (testGenericStructConflictFails()) return 1;
  if (testGenericStructUninferrableFails()) return 1;
  if (testGenericStructExplicitOk()) return 1;
  if (testGenericStructExplicitArityFails()) return 1;
  if (testGenericStructBareFails()) return 1;
  if (testNonGenericWithArgsFails()) return 1;
  if (testGenericStructInfiniteFails()) return 1;
  if (testGenericStructNestedOk()) return 1;
  if (testGenericStructFnOk()) return 1;
  if (testTraitMasterOk()) return 1;
  if (testTraitMismatchRetFails()) return 1;
  if (testTraitMismatchParamFails()) return 1;
  if (testTraitMissingMethodFails()) return 1;
  if (testTraitExtraMethodFails()) return 1;
  if (testTraitDupImplFails()) return 1;
  if (testTraitNonSelfFails()) return 1;
  if (testTraitUnknownMethodFails()) return 1;
  if (testTraitAmbiguousFails()) return 1;
  if (testTraitPrimOk()) return 1;
  if (testTraitGenericInstanceOk()) return 1;
  if (testTraitBoundOk()) return 1;
  if (testTraitBoundMultiOk()) return 1;
  if (testTraitBoundFails()) return 1;
  if (testTraitBoundStructOk()) return 1;
  if (testTraitGenericMethodFails()) return 1;
  if (testTraitMethodAsFieldHint()) return 1;
  if (testPropagateOk()) return 1;
  if (testPropagateNonResultFails()) return 1;
  if (testPropagateVoidFnFails()) return 1;
  if (testPropagateMismatchFails()) return 1;
  if (testPropagateStmtOk()) return 1;
  if (testMatchIntOk()) return 1;
  if (testMatchBoolFullOk()) return 1;
  if (testMatchBindOk()) return 1;
  if (testMatchGuardOk()) return 1;
  if (testMatchOptionFullOk()) return 1;
  if (testMatchResultFullOk()) return 1;
  if (testMatchOptionStructRejects()) return 1;
  if (testMatchNonExhaustiveFails()) return 1;
  if (testMatchUnreachableFails()) return 1;
  if (testMatchDupLitFails()) return 1;
  if (testMatchBoolMissingFails()) return 1;
  if (testMatchTypeMismatchFails()) return 1;
  if (testMatchGuardNonBoolFails()) return 1;
  if (testMatchGuardedWildNotExhaustiveFails()) return 1;
  if (testMatchUnknownVariantFails()) return 1;
  if (testEnumDeclCtorMatchOk()) return 1;
  if (testEnumPayloadOk()) return 1;
  if (testEnumGenericInferOk()) return 1;
  if (testEnumExplicitTargsOk()) return 1;
  if (testEnumArityFails()) return 1;
  if (testEnumUnknownVariantFails()) return 1;
  if (testEnumNonExhaustiveFails()) return 1;
  if (testEnumPayloadMismatchFails()) return 1;
  if (testEnumForgeFails()) return 1;
  if (testEnumInferFails()) return 1;
  if (testLambdaAnnotatedOk()) return 1;
  if (testLambdaUnannotatedPropagatedOk()) return 1;
  if (testLambdaLetAnnotatedOk()) return 1;
  if (testLambdaGenericCombinatorOk()) return 1;
  if (testLambdaNoContextFails()) return 1;
  if (testLambdaArityFails()) return 1;
  if (testLambdaRetMismatchFails()) return 1;
  if (testLambdaArrayCaptureFails()) return 1;
  if (testLambdaNestedOk()) return 1;
  if (testExternSigOk()) return 1;
  if (testExternBadParamFails()) return 1;
  if (testExternStringRetFails()) return 1;
  if (testExternAsValueFails()) return 1;
  std::cout << "ALL TYPECHECK TESTS PASSED\n";
  return 0;
}
