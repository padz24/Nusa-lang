// Unit test semantic v0.3.0: symbol table + undefined variables.
// Tanpa assert() — CHECK selalu dievaluasi (no-op di Release dilarang).
#include "compiler/diagnostics/Diagnostic.h"
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

// Parse harus sukses, lalu jalankan semantic. Mengembalikan error semantic.
static std::vector<SemanticError> analyzeSrc(const std::string &src,
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
  parseOk = pres.ok();
  if (!parseOk)
    return {};
  SemanticAnalyzer sema;
  return sema.analyze(pres.program);
}

static int testMilestoneProgramOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() {\n"
                         "    let a = 10;\n"
                         "    let b = 20;\n"
                         "    let result = a + b;\n"
                         "    print(result);\n"
                         "}\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMilestoneProgramOk OK\n";
  return 0;
}

static int testUndefinedPrint() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() {\n    print(x);\n}\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'x'");
  CHECK(errs[0].loc.line == 2);
  std::cout << "testUndefinedPrint OK\n";
  return 0;
}

static int testUseBeforeDef() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() {\n"
                         "    let b = a;\n"
                         "    let a = 1;\n"
                         "}\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'a'");
  std::cout << "testUseBeforeDef OK\n";
  return 0;
}

static int testSelfInit() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let x = x; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'x'");
  std::cout << "testSelfInit OK\n";
  return 0;
}

static int testDuplicate() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { let x = 1; let x = 2; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate variable 'x'");
  CHECK(!errs[0].hint.empty());
  std::cout << "testDuplicate OK\n";
  return 0;
}

static int testNestedUndefined() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let a = 1; let r = a + b * 2; }",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'b'");
  std::cout << "testNestedUndefined OK\n";
  return 0;
}

static int testMultipleUndefined() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { print(a); print(b); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 2); // semua dilaporkan, tanpa crash
  std::cout << "testMultipleUndefined OK\n";
  return 0;
}

static int testBoolAndStringVarsOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() {\n"
                         "    let b = 10 > 5;\n"
                         "    let s = \"Nusa\";\n"
                         "    print(b);\n"
                         "    print(s);\n"
                         "}\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testBoolAndStringVarsOk OK\n";
  return 0;
}

static int testMasterFunctionsOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn add(a: i64, b: i64) -> i64 {\n"
                         "    return a + b;\n"
                         "}\n"
                         "fn main() {\n"
                         "    let result = add(10, 20);\n"
                         "    print(result);\n"
                         "}\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMasterFunctionsOk OK\n";
  return 0;
}

static int testUndefinedFunction() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { let x = foo(1); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined function 'foo'");
  std::cout << "testUndefinedFunction OK\n";
  return 0;
}

static int testDuplicateFunction() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn f() -> i64 { return 1; }\n"
                         "fn f() -> i64 { return 2; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate function 'f'");
  std::cout << "testDuplicateFunction OK\n";
  return 0;
}

// Phase 70 (v2.21.0, RFC-0009 slice 1): konstanta top-level.
static int testConstOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("const MAX = 100;\n"
                         "fn main() { print(MAX); }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testConstOk OK\n";
  return 0;
}

static int testDuplicateConst() {
  bool parseOk = false;
  auto errs = analyzeSrc("const X = 1;\n"
                         "const X = 2;\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate constant 'X'");
  std::cout << "testDuplicateConst OK\n";
  return 0;
}

static int testConstFunctionClash() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn MAX() -> i64 { return 1; }\n"
                         "const MAX = 2;\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate definition 'MAX'");
  std::cout << "testConstFunctionClash OK\n";
  return 0;
}

static int testAssignToConst() {
  bool parseOk = false;
  auto errs = analyzeSrc("const X = 1;\n"
                         "fn main() { X = 2; }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot assign to constant 'X'");
  std::cout << "testAssignToConst OK\n";
  return 0;
}

static int testMissingMain() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn f() -> i64 { return 1; }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "no 'fn main' found");
  std::cout << "testMissingMain OK\n";
  return 0;
}

static int testParamScope() {
  // Parameter terlihat di body; antar-fungsi tidak bocor.
  bool parseOk = false;
  auto errs = analyzeSrc("fn add(a: i64, b: i64) -> i64 { return a + b; }\n"
                         "fn main() { print(a); }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'a'");
  std::cout << "testParamScope OK\n";
  return 0;
}

static int testDuplicateParam() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn f(a: i64, a: i64) -> i64 { return a; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate parameter 'a'");
  std::cout << "testDuplicateParam OK\n";
  return 0;
}

static int testCallMainRejected() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { main(); }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot call 'main'");
  std::cout << "testCallMainRejected OK\n";
  return 0;
}

static int testReservedPrintRejected() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn print(x: i64) -> i64 { return x; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot declare function 'print'");
  std::cout << "testReservedPrintRejected OK\n";
  return 0;
}

static int testShadowOk() {
  // Shadowing antar-scope diizinkan.
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let x = 1; if true { let x = 2; "
                         "print(x); } print(x); }",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testShadowOk OK\n";
  return 0;
}

static int testInnerNotVisibleOutside() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { if true { let x = 1; } print(x); }",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'x'");
  std::cout << "testInnerNotVisibleOutside OK\n";
  return 0;
}

static int testOuterVisibleInside() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let x = 1; while true { print(x); } }",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testOuterVisibleInside OK\n";
  return 0;
}

static int testSameScopeDupInBlock() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { if true { let x = 1; let x = 2; } }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate variable 'x'");
  std::cout << "testSameScopeDupInBlock OK\n";
  return 0;
}

static int testAssignAfterLetOk() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { let n = 1; n = n + 1; print(n); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAssignAfterLetOk OK\n";
  return 0;
}

static int testAssignUndefined() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { x = 1; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'x'");
  std::cout << "testAssignUndefined OK\n";
  return 0;
}

static int testLenBuiltinOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let n = [1, 2]; print(len(n)); }",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLenBuiltinOk OK\n";
  return 0;
}

static int testLenFnReserved() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn len(x: i64) -> i64 { return x; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot declare function 'len'");
  std::cout << "testLenFnReserved OK\n";
  return 0;
}

static int testIndexSubexprsOk() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { let a = [1]; print(a[len(a)]); }", parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testIndexSubexprsOk OK\n";
  return 0;
}

static int testMasterStructOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct Person {\n"
                         "    name: string;\n"
                         "    age: i64;\n"
                         "}\n"
                         "fn main() {\n"
                         "    let p = Person { name: \"Nusa\", age: 1 };\n"
                         "    print(p.age);\n"
                         "}\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testMasterStructOk OK\n";
  return 0;
}

static int testUndefinedStruct() {
  bool parseOk = false;
  auto errs =
      analyzeSrc("fn main() { let p = Ghost { x: 1 }; }", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined struct 'Ghost'");
  std::cout << "testUndefinedStruct OK\n";
  return 0;
}

static int testDuplicateStruct() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct P { x: i64; }\n"
                         "struct P { y: i64; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate struct 'P'");
  std::cout << "testDuplicateStruct OK\n";
  return 0;
}

// v1.7.0 (Phase 26): enum menempati namespace tipe.
static int testDuplicateEnum() {
  bool parseOk = false;
  auto errs = analyzeSrc("enum E { A, B }\n"
                         "enum E { C }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate type 'E'");
  std::cout << "testDuplicateEnum OK\n";
  return 0;
}

static int testEnumStructClash() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct P { x: i64; }\n"
                         "enum P { A }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate type 'P'");
  std::cout << "testEnumStructClash OK\n";
  return 0;
}

static int testDuplicateVariant() {
  bool parseOk = false;
  auto errs = analyzeSrc("enum E { A, A }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate variant 'A' in enum 'E'");
  std::cout << "testDuplicateVariant OK\n";
  return 0;
}

static int testEnumAsValue() {
  bool parseOk = false;
  auto errs = analyzeSrc("enum E { A }\n"
                         "fn main() { let x = E; print(x); }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot use enum 'E' as a value");
  std::cout << "testEnumAsValue OK\n";
  return 0;
}

static int testBareVariantCallHint() {
  bool parseOk = false;
  auto errs = analyzeSrc("enum E { A(i64) }\n"
                         "fn main() { let x = A(1); print(x); }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined function 'A'");
  std::cout << "testBareVariantCallHint OK\n";
  return 0;
}

// v1.8.0 (Phase 27): scope lambda.
static int testLambdaDupParam() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let f = |x, x| x; print(f); }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate parameter 'x' in lambda");
  std::cout << "testLambdaDupParam OK\n";
  return 0;
}

static int testLambdaUndefined() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let f = |x| x + zz; print(f); }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'zz'");
  std::cout << "testLambdaUndefined OK\n";
  return 0;
}

static int testLambdaCaptureOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() {\n"
                         "    let k = 1;\n"
                         "    let f = |x| x + k;\n"
                         "    print(f);\n"
                         "}\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testLambdaCaptureOk OK\n";
  return 0;
}

// v1.13.0 (Phase 32): batasan extern.
static int testExternGenericFails() {
  bool parseOk = false;
  auto errs = analyzeSrc("extern fn f<T>(x: T) -> T;\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "extern function 'f' cannot be generic");
  std::cout << "testExternGenericFails OK\n";
  return 0;
}

static int testExternMainFails() {
  bool parseOk = false;
  auto errs = analyzeSrc("extern fn main() -> void;\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 2); // extern-main + duplikat main
  CHECK(errs[0].message == "cannot declare 'main' as extern");
  std::cout << "testExternMainFails OK\n";
  return 0;
}

static int testUnknownField() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct P { x: i64; }\n"
                         "fn main() { let p = P { y: 1 }; }\n",
                         parseOk);
  CHECK(parseOk);
  // Dua diagnostic valid: field tak dikenal + field wajib yang hilang.
  CHECK(errs.size() == 2);
  CHECK(errs[0].message == "unknown field 'y' for struct 'P'");
  CHECK(errs[1].message == "missing field 'x' for struct 'P'");
  std::cout << "testUnknownField OK\n";
  return 0;
}

static int testMissingField() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct P { x: i64; y: i64; }\n"
                         "fn main() { let p = P { x: 1 }; }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "missing field 'y' for struct 'P'");
  std::cout << "testMissingField OK\n";
  return 0;
}

static int testDuplicateFieldInit() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct P { x: i64; }\n"
                         "fn main() { let p = P { x: 1, x: 2 }; }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate field 'x' for struct 'P'");
  std::cout << "testDuplicateFieldInit OK\n";
  return 0;
}

// v1.2.0 (Phase 21a): namespace tipe untuk alias.
static int testAliasOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("type Nums = [i64];\nfn main() { }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testAliasOk OK\n";
  return 0;
}

static int testDuplicateAlias() {
  bool parseOk = false;
  auto errs = analyzeSrc("type A = i64;\ntype A = bool;\nfn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate type 'A'");
  std::cout << "testDuplicateAlias OK\n";
  return 0;
}

static int testAliasVsStruct() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct A { x: i64; }\ntype A = i64;\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate type 'A'");
  std::cout << "testAliasVsStruct OK\n";
  return 0;
}

static int testStructVsAlias() {
  bool parseOk = false;
  auto errs = analyzeSrc("type A = i64;\nstruct A { x: i64; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  // Struct loop jalan dulu (lolos), lalu alias loop menolak duplikat.
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate type 'A'");
  std::cout << "testStructVsAlias OK\n";
  return 0;
}

static int testAliasReserved() {
  bool parseOk = false;
  auto errs = analyzeSrc("type print = i64;\nfn main() { }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot declare type alias 'print'");
  std::cout << "testAliasReserved OK\n";
  return 0;
}

static int testAliasStructLitRejected() {
  bool parseOk = false;
  auto errs = analyzeSrc("type P2 = P;\nstruct P { x: i64; }\n"
                         "fn main() { let p = P2 { x: 1 }; }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot construct type alias 'P2'");
  std::cout << "testAliasStructLitRejected OK\n";
  return 0;
}

// v1.2.0 (Phase 21b): nama fungsi sebagai value.
static int testFnValueOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn add(a: i64) -> i64 { return a; }\n"
                         "fn main() { let f = add; }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testFnValueOk OK\n";
  return 0;
}

static int testMainAsValueRejected() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let f = main; }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot use 'main' as a value");
  std::cout << "testMainAsValueRejected OK\n";
  return 0;
}

static int testBuiltinAsValueRejected() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let f = len; }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot use builtin 'len' as a value");
  std::cout << "testBuiltinAsValueRejected OK\n";
  return 0;
}

static int testPrintAsValueRejected() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn main() { let f = print; }\n", parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "cannot use 'print' as a value");
  std::cout << "testPrintAsValueRejected OK\n";
  return 0;
}

// Panggilan via parameter dilewati semantic (ranah typechecker).
static int testCallViaParamOk() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn apply(f: fn(i64) -> i64, x: i64) -> i64 { "
                         "return f(x); }\nfn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.empty());
  std::cout << "testCallViaParamOk OK\n";
  return 0;
}

// v1.2.0 (Phase 21c): duplikat parameter tipe.
static int testDupTparam() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn f<T, T>(x: T) -> T { return x; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate type parameter 'T' in 'f'");
  std::cout << "testDupTparam OK\n";
  return 0;
}

// v1.2.0 (Phase 21d): duplikat parameter tipe struct.
static int testDupStructTparam() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct P<T, T> { x: T; }\nfn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate type parameter 'T' in 'P'");
  std::cout << "testDupStructTparam OK\n";
  return 0;
}

// v1.3.0 (Phase 22): namespace trait + validasi impl.
static int testTraitDup() {
  bool parseOk = false;
  auto errs = analyzeSrc("trait P { fn m(self) -> i64; }\n"
                         "trait P { fn n(self) -> i64; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate trait 'P'");
  std::cout << "testTraitDup OK\n";
  return 0;
}

static int testTraitDupMethod() {
  bool parseOk = false;
  auto errs = analyzeSrc("trait P { fn m(self) -> i64; fn m(self) -> i64; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate method 'm' in trait 'P'");
  std::cout << "testTraitDupMethod OK\n";
  return 0;
}

static int testTraitMainMethod() {
  bool parseOk = false;
  auto errs = analyzeSrc("trait P { fn main(); }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "'fn main' cannot be a trait method");
  std::cout << "testTraitMainMethod OK\n";
  return 0;
}

static int testImplUnknownTrait() {
  bool parseOk = false;
  auto errs = analyzeSrc("struct A { x: i64; }\n"
                         "impl Nope for A { fn m(self) -> i64 { return 1; } }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "unknown trait 'Nope'");
  std::cout << "testImplUnknownTrait OK\n";
  return 0;
}

static int testImplDupMethod() {
  bool parseOk = false;
  auto errs = analyzeSrc("trait P { fn m(self) -> i64; }\n"
                         "struct A { x: i64; }\n"
                         "impl P for A { fn m(self) -> i64 { return 1; }\n"
                         "fn m(self) -> i64 { return 2; } }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "duplicate method 'm' in impl of 'P'");
  std::cout << "testImplDupMethod OK\n";
  return 0;
}

static int testImplMainMethod() {
  bool parseOk = false;
  auto errs = analyzeSrc("trait P { fn m(self) -> i64; }\n"
                         "struct A { x: i64; }\n"
                         "impl P for A { fn main() { } }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "'fn main' cannot be a trait method");
  std::cout << "testImplMainMethod OK\n";
  return 0;
}

// Body metode ikut diperiksa (variabel tak dikenal).
static int testImplBodyWalked() {
  bool parseOk = false;
  auto errs = analyzeSrc("trait P { fn m(self) -> i64; }\n"
                         "struct A { x: i64; }\n"
                         "impl P for A { fn m(self) -> i64 { return nope; } }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message == "undefined variable 'nope'");
  std::cout << "testImplBodyWalked OK\n";
  return 0;
}

static int testBoundUnknownTrait() {
  bool parseOk = false;
  auto errs = analyzeSrc("fn f<T: Nope>(x: T) -> T { return x; }\n"
                         "fn main() { }\n",
                         parseOk);
  CHECK(parseOk);
  CHECK(errs.size() == 1);
  CHECK(errs[0].message.find("unknown trait 'Nope'") != std::string::npos);
  std::cout << "testBoundUnknownTrait OK\n";
  return 0;
}

int main() {
  if (testMilestoneProgramOk()) return 1;
  if (testUndefinedPrint()) return 1;
  if (testUseBeforeDef()) return 1;
  if (testSelfInit()) return 1;
  if (testDuplicate()) return 1;
  if (testNestedUndefined()) return 1;
  if (testMultipleUndefined()) return 1;
  if (testBoolAndStringVarsOk()) return 1;
  if (testMasterFunctionsOk()) return 1;
  if (testUndefinedFunction()) return 1;
  if (testDuplicateFunction()) return 1;
  if (testConstOk()) return 1;
  if (testDuplicateConst()) return 1;
  if (testConstFunctionClash()) return 1;
  if (testAssignToConst()) return 1;
  if (testMissingMain()) return 1;
  if (testParamScope()) return 1;
  if (testDuplicateParam()) return 1;
  if (testCallMainRejected()) return 1;
  if (testReservedPrintRejected()) return 1;
  if (testShadowOk()) return 1;
  if (testInnerNotVisibleOutside()) return 1;
  if (testOuterVisibleInside()) return 1;
  if (testSameScopeDupInBlock()) return 1;
  if (testAssignAfterLetOk()) return 1;
  if (testAssignUndefined()) return 1;
  if (testLenBuiltinOk()) return 1;
  if (testLenFnReserved()) return 1;
  if (testIndexSubexprsOk()) return 1;
  if (testMasterStructOk()) return 1;
  if (testUndefinedStruct()) return 1;
  if (testDuplicateStruct()) return 1;
  if (testDuplicateEnum()) return 1;
  if (testEnumStructClash()) return 1;
  if (testDuplicateVariant()) return 1;
  if (testEnumAsValue()) return 1;
  if (testBareVariantCallHint()) return 1;
  if (testLambdaDupParam()) return 1;
  if (testLambdaUndefined()) return 1;
  if (testLambdaCaptureOk()) return 1;
  if (testExternGenericFails()) return 1;
  if (testExternMainFails()) return 1;
  if (testUnknownField()) return 1;
  if (testMissingField()) return 1;
  if (testDuplicateFieldInit()) return 1;
  if (testAliasOk()) return 1;
  if (testDuplicateAlias()) return 1;
  if (testAliasVsStruct()) return 1;
  if (testStructVsAlias()) return 1;
  if (testAliasReserved()) return 1;
  if (testAliasStructLitRejected()) return 1;
  if (testFnValueOk()) return 1;
  if (testMainAsValueRejected()) return 1;
  if (testBuiltinAsValueRejected()) return 1;
  if (testPrintAsValueRejected()) return 1;
  if (testCallViaParamOk()) return 1;
  if (testDupTparam()) return 1;
  if (testDupStructTparam()) return 1;
  if (testTraitDup()) return 1;
  if (testTraitDupMethod()) return 1;
  if (testTraitMainMethod()) return 1;
  if (testImplUnknownTrait()) return 1;
  if (testImplDupMethod()) return 1;
  if (testImplMainMethod()) return 1;
  if (testImplBodyWalked()) return 1;
  if (testBoundUnknownTrait()) return 1;
  std::cout << "ALL SEMANTIC TESTS PASSED\n";
  return 0;
}
