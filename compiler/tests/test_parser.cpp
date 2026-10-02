// Unit test parser v0.2.0: `let` + operator precedence (via AST dump).
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"
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

static ParseResult parseSrc(const std::string &file, const std::string &src,
                            bool &lexOk) {
  Lexer lx(file, src);
  auto toks = lx.tokenize();
  lexOk = lx.errors().empty();
  auto lines = DiagnosticEngine::splitLines(src);
  Parser p(file, toks, lines);
  return p.parse();
}

// Wajib: `10 + (20 * 3)`, bukan `(10 + 20) * 3`.
static int testPrecedenceMulOverAdd() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { let x = 10 + 20 * 3; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let x (+ 10 (* 20 3)))))");
  std::cout << "testPrecedenceMulOverAdd OK\n";
  return 0;
}

static int testLeftAssocAdd() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { let x = 1 + 2 + 3; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let x (+ (+ 1 2) 3))))");
  std::cout << "testLeftAssocAdd OK\n";
  return 0;
}

static int testMulThenAdd() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { let x = 10 * 20 + 3; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let x (+ (* 10 20) 3))))");
  std::cout << "testMulThenAdd OK\n";
  return 0;
}

static int testParensOverride() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { let x = (10 + 20) * 3; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let x (* (+ 10 20) 3))))");
  std::cout << "testParensOverride OK\n";
  return 0;
}

static int testEqualityLowest() {
  bool lexOk = false;
  auto r =
      parseSrc("p.ns", "fn main() { let b = 1 + 2 * 3 == 7; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let b (== (+ 1 (* 2 3)) 7))))");
  std::cout << "testEqualityLowest OK\n";
  return 0;
}

static int testComparisons() {
  bool lexOk = false;
  auto r = parseSrc("p.ns",
                    "fn main() { let a = x != y; let b = x <= 5; let c = x "
                    "> 1; let d = x >= y; }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let a (!= x y)) (let b (<= x 5)) (let c "
        "(> x 1)) (let d (>= x y))))");
  std::cout << "testComparisons OK\n";
  return 0;
}

static int testUnary() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { let y = -x * 2; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let y (* (- x) 2))))");
  std::cout << "testUnary OK\n";
  return 0;
}

static int testBoolAndDiv() {
  bool lexOk = false;
  auto r = parseSrc("p.ns",
                    "fn main() { let b = true; let c = false; let d = 10 / "
                    "2 - 1; }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let b true) (let c false) (let d (- (/ 10 2) 1))))");
  std::cout << "testBoolAndDiv OK\n";
  return 0;
}

// v0.1.0 regression: hello tetap ter-parse (print string = ekspresi).
static int testHelloParses() {
  bool lexOk = false;
  auto r = parseSrc("hello.ns", "fn main() {\n    print(\"Hello, Nusa!\");\n}\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (print \"Hello, Nusa!\")))");
  std::cout << "testHelloParses OK\n";
  return 0;
}

// v0.2.0: print kini menerima ekspresi umum (codegen-nya v0.3.0).
static int testPrintIntParses() {
  bool lexOk = false;
  auto r = parseSrc("a.ns", "fn main() { print(10 + 20); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (print (+ 10 20))))");
  std::cout << "testPrintIntParses OK\n";
  return 0;
}

static int testNoSemicolonOk() {
  bool lexOk = false;
  auto r = parseSrc("a.ns", "fn main() { let x = 1 print(\"Hi\") }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  std::cout << "testNoSemicolonOk OK\n";
  return 0;
}

static int testMissingFnFails() {
  Lexer lx("b.ns", "main() {}");
  auto toks = lx.tokenize();
  auto lines = DiagnosticEngine::splitLines("main() {}");
  Parser p("b.ns", toks, lines);
  auto r = p.parse();
  CHECK(!r.ok());
  std::cout << "testMissingFnFails OK\n";
  return 0;
}

static int testLetDoubleEqFails() {
  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { let x == 3; }", lexOk);
  CHECK(lexOk); // lexer OK, parser yang menolak
  CHECK(!r.ok());
  std::cout << "testLetDoubleEqFails OK\n";
  return 0;
}

static int testLetMissingExprFails() {
  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { let x = 10 + ; }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testLetMissingExprFails OK\n";
  return 0;
}

static int testReturnParses() {
  // v0.5.0: `return` valid di parser (typecheck yang menilai).
  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { return; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (return)))");
  auto r2 =
      parseSrc("c.ns", "fn f() -> i64 { return 1; } fn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(r2.ok());
  std::cout << "testReturnParses OK\n";
  return 0;
}

static int testMasterControl() {
  // Program milestone v0.6.0 verbatim.
  bool lexOk = false;
  auto r = parseSrc("c.ns",
                    "fn main() {\n"
                    "    let x: i64 = 10;\n"
                    "\n"
                    "    if x > 5 {\n"
                    "        print(x);\n"
                    "    } else {\n"
                    "        print(0);\n"
                    "    }\n"
                    "}\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let x: i64 10) (if (> x 5) ((print "
        "x)) ((print 0)))))");
  std::cout << "testMasterControl OK\n";
  return 0;
}

static int testIfNoElse() {
  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { if x { print(x); } }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (if x ((print x)))))");
  std::cout << "testIfNoElse OK\n";
  return 0;
}

static int testWhile() {
  bool lexOk = false;
  auto r = parseSrc("c.ns",
                    "fn main() { while x > 0 { print(x); } }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (while (> x 0) "
                            "((print x)))))");
  std::cout << "testWhile OK\n";
  return 0;
}

static int testNestedIfWhile() {
  bool lexOk = false;
  auto r = parseSrc("c.ns",
                    "fn main() { if a { if b { print(1); } } "
                    "while c { while d { print(2); } } }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (if a ((if b ((print 1))))) (while c "
        "((while d ((print 2)))))))");
  std::cout << "testNestedIfWhile OK\n";
  return 0;
}

static int testElseIfChain() {
  bool lexOk = false;
  auto r = parseSrc("c.ns",
                    "fn main() { if a { print(1); } else if b { print(2); } "
                    "else { print(3); } }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (if a ((print 1)) ((if b ((print 2)) "
        "((print 3)))))))");
  std::cout << "testElseIfChain OK\n";
  return 0;
}

static int testMasterStruct() {
  // Program struct milestone v0.8.0 verbatim.
  bool lexOk = false;
  auto r = parseSrc("p.ns",
                    "struct Person {\n"
                    "    name: string;\n"
                    "    age: i64;\n"
                    "}\n"
                    "fn main() {\n"
                    "    let person = Person {\n"
                    "        name: \"Nusa\",\n"
                    "        age: 1\n"
                    "    };\n"
                    "    print(person.age);\n"
                    "}\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (struct Person (name: string) (age: i64)) (fn main() -> "
        "void (let person (Person (name \"Nusa\") (age 1))) (print (field "
        "person age))))");
  std::cout << "testMasterStruct OK\n";
  return 0;
}

static int testImport() {
  bool lexOk = false;
  auto r = parseSrc("m.ns", "import math;\nfn main() { }\n", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (import math) (fn main() -> void ))");
  std::cout << "testImport OK\n";
  return 0;
}

// v1.10.0 (Phase 29): import bertitik kanonis.
static int testDottedImport() {
  bool lexOk = false;
  auto r = parseSrc("m.ns", "import std.time;\nfn main() { }\n", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.imports.size() == 1);
  CHECK(r.program.imports[0].module == "std.time");
  std::cout << "testDottedImport OK\n";
  return 0;
}

// v1.13.0 (Phase 32): deklarasi extern.
static int testExternParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "extern fn strlen(s: string) -> i64;\n"
                    "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.functions.size() == 2);
  CHECK(r.program.functions[0].name == "strlen");
  CHECK(r.program.functions[0].isExtern);
  CHECK(!r.program.functions[1].isExtern);
  std::cout << "testExternParses OK\n";
  return 0;
}

static int testExternBodyFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "extern fn f(x: i64) -> i64 {\n"
                    "    return x;\n"
                    "}\n"
                    "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testExternBodyFails OK\n";
  return 0;
}

static int testFieldChain() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { print(a.b.c); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (print (field "
                            "(field a b) c))))");
  std::cout << "testFieldChain OK\n";
  return 0;
}

static int testFieldIndexMix() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { print(a.b[0]); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (print (index "
                            "(field a b) 0))))");
  std::cout << "testFieldIndexMix OK\n";
  return 0;
}

static int testMissingFieldColonFails() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "struct P { name string; }\nfn main() { }\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testMissingFieldColonFails OK\n";
  return 0;
}

static int testUnclosedStructFails() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "struct P { name: string;\nfn main() { }\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testUnclosedStructFails OK\n";
  return 0;
}

static int testBareIdentCondition() {
  // Regresi: `if c {` — `{` milik block, BUKAN struct literal.
  bool lexOk = false;
  auto r = parseSrc("c.ns",
                    "fn main() { if c { print(1); } else { print(0); } }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (if c ((print 1)) "
                            "((print 0)))))");
  auto r2 =
      parseSrc("c.ns", "fn main() { while done { print(1); } }", lexOk);
  CHECK(lexOk);
  CHECK(r2.ok());
  std::cout << "testBareIdentCondition OK\n";
  return 0;
}

static int testMissingDotFieldFails() {
  bool lexOk = false;
  auto r = parseSrc("p.ns", "fn main() { print(p.); }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testMissingDotFieldFails OK\n";
  return 0;
}

static int testMasterArray() {
  // Program array milestone v0.7.0 verbatim.
  bool lexOk = false;
  auto r = parseSrc("a.ns",
                    "fn main() {\n"
                    "    let numbers: [i64] = [1, 2, 3, 4];\n"
                    "    print(numbers[0]);\n"
                    "}\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let numbers: [i64] (array 1 2 3 4)) "
        "(print (index numbers 0))))");
  std::cout << "testMasterArray OK\n";
  return 0;
}

static int testArrayTypeNested() {
  bool lexOk = false;
  auto r = parseSrc("a.ns", "fn main() { let x: [[i64]] = [[1]]; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok()); // ter-parse; nested ditolak typechecker
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let x: [[i64]] (array (array 1)))))");
  std::cout << "testArrayTypeNested OK\n";
  return 0;
}

static int testIndexChain() {
  bool lexOk = false;
  auto r = parseSrc("a.ns", "fn main() { print(a[0][1]); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok()); // ter-parse; dicek typechecker
  CHECK(r.program.dump() == "(program (fn main() -> void (print (index "
                            "(index a 0) 1))))");
  std::cout << "testIndexChain OK\n";
  return 0;
}

static int testUnclosedIndexFails() {
  bool lexOk = false;
  auto r = parseSrc("a.ns", "fn main() { print(a[0); }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testUnclosedIndexFails OK\n";
  return 0;
}

static int testUnclosedArrayFails() {
  bool lexOk = false;
  auto r = parseSrc("a.ns", "fn main() { let x = [1, 2; }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testUnclosedArrayFails OK\n";
  return 0;
}

static int testAssign() {  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { let n = 3; n = n - 1; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let n 3) (set n (- n 1))))");
  std::cout << "testAssign OK\n";
  return 0;
}

static int testMissingBraceFails() {
  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { if x > 5 print(x); }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testMissingBraceFails OK\n";
  return 0;
}

static int testStrayElseFails() {
  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { else { print(1); } }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testStrayElseFails OK\n";
  return 0;
}

static int testMasterFunctions() {
  // Program milestone v0.5.0 verbatim.
  bool lexOk = false;
  auto r = parseSrc("f.ns",
                    "fn add(a: i64, b: i64) -> i64 {\n"
                    "    return a + b;\n"
                    "}\n"
                    "\n"
                    "fn main() {\n"
                    "    let result = add(10, 20);\n"
                    "    print(result);\n"
                    "}\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn add(a: i64, b: i64) -> i64 (return (+ a b))) (fn "
        "main() -> void (let result (call add 10 20)) (print result)))");
  std::cout << "testMasterFunctions OK\n";
  return 0;
}

static int testNestedCalls() {
  bool lexOk = false;
  auto r = parseSrc("f.ns",
                    "fn main() { print(add(1, mul(2, 3))); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (print (call add 1 "
                            "(call mul 2 3)))))");
  std::cout << "testNestedCalls OK\n";
  return 0;
}

static int testEmptyArgsCall() {
  bool lexOk = false;
  auto r = parseSrc("f.ns", "fn main() { print(get()); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (print (call get))))");
  std::cout << "testEmptyArgsCall OK\n";
  return 0;
}

static int testMainWithParamsFails() {
  bool lexOk = false;
  auto r = parseSrc("f.ns", "fn main(a: i64) { print(a); }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok()); // parser: main tanpa parameter
  std::cout << "testMainWithParamsFails OK\n";
  return 0;
}

static int testMissingArrowTypeFails() {
  bool lexOk = false;
  auto r = parseSrc("f.ns", "fn add(a: i64) -> { return a; }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testMissingArrowTypeFails OK\n";
  return 0;
}

static int testUnclosedCallFails() {
  bool lexOk = false;
  auto r = parseSrc("f.ns", "fn main() { print(add(1, 2); }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testUnclosedCallFails OK\n";
  return 0;
}

static int testParamMissingColonFails() {
  bool lexOk = false;
  auto r = parseSrc("f.ns", "fn add(a i64) -> i64 { return a; }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testParamMissingColonFails OK\n";
  return 0;
}

static int testDeepNestingOk() {
  // 100-dalam harus tetap lolos (jauh di bawah batas 500).
  std::string src = "fn main() { print(";
  for (int i = 0; i < 100; i++)
    src += "(";
  src += "1";
  for (int i = 0; i < 100; i++)
    src += ")";
  src += "); }";
  bool lexOk = false;
  auto r = parseSrc("t.ns", src, lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  std::cout << "testDeepNestingOk OK\n";
  return 0;
}

static int testTooDeepNestingFails() {
  // 10000-dalam harus DITOLAK bersih (tanpa stack overflow).
  std::string src = "fn main() { print(";
  for (int i = 0; i < 10000; i++)
    src += "(";
  src += "1";
  for (int i = 0; i < 10000; i++)
    src += ")";
  src += "); }";
  bool lexOk = false;
  auto r = parseSrc("t.ns", src, lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  bool found = false;
  for (const auto &e : r.errors) {
    if (e.message.find("too deeply nested") != std::string::npos)
      found = true;
  }
  CHECK(found);
  std::cout << "testTooDeepNestingFails OK\n";
  return 0;
}

static int testTooDeepBlockFails() {
  std::string src = "fn main() {";
  for (int i = 0; i < 10000; i++)
    src += "if true {";
  src += "print(1);";
  for (int i = 0; i < 10000; i++)
    src += "}";
  src += "}";
  bool lexOk = false;
  auto r = parseSrc("t.ns", src, lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testTooDeepBlockFails OK\n";
  return 0;
}

static int testUnclosedParenFails() {
  bool lexOk = false;
  auto r = parseSrc("c.ns", "fn main() { print((1 + 2); }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testUnclosedParenFails OK\n";
  return 0;
}

static int testMultiplePrints() {
  bool lexOk = false;
  auto r =
      parseSrc("d.ns", "fn main() { print(\"a\"); print(\"b\"); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.functions.size() == 1);
  CHECK(r.program.functions[0].body.stmts.size() == 2);
  std::cout << "testMultiplePrints OK\n";
  return 0;
}

static int testAnnotatedLet() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "fn main() { let age: i64 = 15; let active: bool = true; "
                    "let name: string = \"Nusa\"; }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let age: i64 15) (let active: bool "
        "true) (let name: string \"Nusa\")))");
  std::cout << "testAnnotatedLet OK\n";
  return 0;
}

static int testUnannotatedStillOk() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let x = 10 + 20 * 3; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let x (+ 10 (* 20 3)))))");
  std::cout << "testUnannotatedStillOk OK\n";
  return 0;
}

static int testMissingTypeFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let x: = 10; }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testMissingTypeFails OK\n";
  return 0;
}

static int testNamedTypeParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let x: foo = 10; }", lexOk);
  CHECK(lexOk);
  // v0.8.0: `foo` ter-parse sebagai tipe bernama; penolakan
  // "unknown type" adalah ranah typechecker (lihat testUnknownType).
  CHECK(r.ok());
  CHECK(r.program.dump() == "(program (fn main() -> void (let x: foo 10)))");
  std::cout << "testNamedTypeParses OK\n";
  return 0;
}

// v1.1.0: recovery top-level — error di dua fungsi dilaporkan dua-duanya
// (dulu: berhenti di fungsi pertama).
static int testRecoveryAcrossFunctions() {
  bool lexOk = false;
  auto r = parseSrc("r.ns",
                    "fn a() { let x = ; }\nfn b() { let y = ; }\nfn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  CHECK(r.errors.size() == 2);
  CHECK(r.errors[0].loc.line == 1);
  CHECK(r.errors[1].loc.line == 2);
  CHECK(r.errors[0].message == "expected expression");
  CHECK(r.errors[1].message == "expected expression");
  std::cout << "testRecoveryAcrossFunctions OK\n";
  return 0;
}

// v1.1.0: token liar top-level dilaporkan lalu dilewati — item valid
// sesudahnya tetap ter-parse.
static int testRecoveryStrayTopLevel() {
  bool lexOk = false;
  auto r =
      parseSrc("r.ns", "let x = 1; fn main() { print(1); }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  CHECK(r.program.functions.size() == 1);
  CHECK(r.program.functions[0].name == "main");
  std::cout << "testRecoveryStrayTopLevel OK\n";
  return 0;
}

// v1.1.0: batas error parser — 40 token liar menghasilkan tepat
// kMaxErrors + 1 error (terakhir: ringkasan "too many errors").
static int testParserErrorCap() {
  std::string src;
  for (int i = 0; i < 40; i++)
    src += "1 ";
  bool lexOk = false;
  auto r = parseSrc("r.ns", src, lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  CHECK(r.errors.size() == Parser::kMaxErrors + 1);
  CHECK(r.errors.back().message == "too many errors, stopping");
  std::cout << "testParserErrorCap OK\n";
  return 0;
}

// v1.1.0: token Unknown (kerusakan yang sudah dilaporkan lexer) dibuang
// parser — tidak ada error parser duplikat, item valid tetap ter-parse.
static int testUnknownTokensFiltered() {
  bool lexOk = false;
  auto r = parseSrc("r.ns", "@ fn main() { print(1); }", lexOk);
  CHECK(!lexOk); // lexer tetap melaporkan '@'
  CHECK(r.ok()); // parser: tanpa error duplikat
  CHECK(r.program.functions.size() == 1);
  CHECK(r.program.functions[0].name == "main");
  std::cout << "testUnknownTokensFiltered OK\n";
  return 0;
}

// v1.2.0 (Phase 21a): deklarasi alias tipe top-level.
static int testTypeAliasParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "type Nums = [i64]; fn main() { print(1); }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.aliases.size() == 1);
  CHECK(r.program.aliases[0].name == "Nums");
  CHECK(r.program.dump() ==
        "(program (type Nums [i64]) (fn main() -> void (print 1)))");
  std::cout << "testTypeAliasParses OK\n";
  return 0;
}

static int testTypeAliasMissingNameFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "type = [i64]; fn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testTypeAliasMissingNameFails OK\n";
  return 0;
}

static int testTypeAliasMissingAssignFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "type Nums [i64]; fn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testTypeAliasMissingAssignFails OK\n";
  return 0;
}

static int testTypeAliasMissingTargetFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "type Nums = ; fn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testTypeAliasMissingTargetFails OK\n";
  return 0;
}

// Phase 70 (v2.21.0, RFC-0009 slice 1): item `const`.
static int testConstParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "const MAX: i64 = 100; const D = MAX * 2; fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.consts.size() == 2);
  CHECK(r.program.consts[0].name == "MAX");
  CHECK(r.program.consts[0].declaredType.has_value());
  CHECK(!r.program.consts[1].declaredType.has_value());
  CHECK(r.program.dump() ==
        "(program (const MAX: i64 100) (const D (* MAX 2)) "
        "(fn main() -> void ))");
  std::cout << "testConstParses OK\n";
  return 0;
}

static int testConstMissingNameFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "const = 1; fn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testConstMissingNameFails OK\n";
  return 0;
}

static int testConstMissingValueFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "const MAX = ; fn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testConstMissingValueFails OK\n";
  return 0;
}

// v1.2.0 (Phase 21b): tipe fungsi di posisi anotasi.
static int testFnTypeParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "fn apply(f: fn(i64) -> i64, x: i64) -> i64 { "
                    "return f(x); } fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn apply(f: fn(i64) -> i64, x: i64) -> i64 "
        "(return (call f x))) (fn main() -> void ))");
  std::cout << "testFnTypeParses OK\n";
  return 0;
}

static int testFnTypeEmptyParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn run(g: fn() -> void) { g(); } "
                            "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  std::cout << "testFnTypeEmptyParses OK\n";
  return 0;
}

static int testFnTypeMissingParenFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn f(g: fn i64) -> i64 { return 1; } "
                            "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testFnTypeMissingParenFails OK\n";
  return 0;
}

// v1.2.0 (Phase 21c): parameter tipe generik.
static int testGenericFnParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "fn identity<T>(value: T) -> T { return value; } "
                    "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.functions[0].tparams.size() == 1);
  CHECK(r.program.functions[0].tparams[0] == "T");
  CHECK(r.program.dump() ==
        "(program (fn identity<T>(value: T) -> T (return value)) "
        "(fn main() -> void ))");
  std::cout << "testGenericFnParses OK\n";
  return 0;
}

static int testGenericFnMainFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main<T>() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testGenericFnMainFails OK\n";
  return 0;
}

static int testGenericFnMissingTparamFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn f<>(x: i64) -> i64 { return x; } "
                            "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testGenericFnMissingTparamFails OK\n";
  return 0;
}

// v1.2.0 (Phase 21d): struct generik + aplikasi tipe + literal eksplisit.
static int testGenericStructParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "struct Point<T> { x: T; y: T; } fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.structs[0].tparams.size() == 1);
  CHECK(r.program.dump() ==
        "(program (struct Point<T> (x: T) (y: T)) (fn main() -> void ))");
  std::cout << "testGenericStructParses OK\n";
  return 0;
}

static int testTypeAppParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "struct Point<T> { x: T; }\n"
                    "fn f(p: Point<i64>) -> i64 { return p.x; }\n"
                    "fn main() { }\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  std::cout << "testTypeAppParses OK\n";
  return 0;
}

static int testExplicitStructLitParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "struct Point<T> { x: T; }\n"
                    "fn main() { let p = Point<i64> { x: 1 }; }\n",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  std::cout << "testExplicitStructLitParses OK\n";
  return 0;
}

static int testGenericStructMissingTparamFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "struct P<> { x: i64; } fn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testGenericStructMissingTparamFails OK\n";
  return 0;
}

// v1.3.0 (Phase 22): trait + impl + method call.
static int testTraitParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "trait Printable { fn print(self); "
                    "fn describe(self) -> string; } fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.traits.size() == 1);
  CHECK(r.program.traits[0].methods.size() == 2);
  CHECK(r.program.dump() ==
        "(program (trait Printable (fn print(self) -> void ) "
        "(fn describe(self) -> string )) (fn main() -> void ))");
  std::cout << "testTraitParses OK\n";
  return 0;
}

static int testImplParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "struct P { x: i64; } trait T { fn m(self) -> i64; } "
                    "impl T for P { fn m(self) -> i64 { return self.x; } } "
                    "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.impls.size() == 1);
  CHECK(r.program.impls[0].trait == "T");
  std::cout << "testImplParses OK\n";
  return 0;
}

static int testMethodCallParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let p = q(); p.print(1); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let p (call q)) "
        "(do (call-method p print 1))))");
  std::cout << "testMethodCallParses OK\n";
  return 0;
}

static int testTraitBodyFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "trait T { fn m(self) -> i64 { return 1; } } "
                    "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testTraitBodyFails OK\n";
  return 0;
}

static int testImplMissingForFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "struct P { x: i64; } trait T { fn m(self) -> i64; } "
                    "impl T P { fn m(self) -> i64 { return 1; } } "
                    "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testImplMissingForFails OK\n";
  return 0;
}

static int testSelfAnnotationFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn f(self: i64) -> i64 { return 1; } "
                            "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testSelfAnnotationFails OK\n";
  return 0;
}

// v1.5.0 (Phase 24): postfix `?` (paling ketat, berantai).
static int testPropagateParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let x = f()?; print(x); }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump() ==
        "(program (fn main() -> void (let x (propagate (call f))) "
        "(print x)))");
  std::cout << "testPropagateParses OK\n";
  return 0;
}

// v1.7.0 (Phase 26): deklarasi enum + ctor eksplisit.
static int testEnumParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "enum Color { Red, Green, Blue }\n"
                    "enum Wrap<T> { Val(T), Empty }\n"
                    "fn main() { }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.enums.size() == 2);
  CHECK(r.program.enums[0].name == "Color");
  CHECK(r.program.enums[0].variants.size() == 3);
  CHECK(r.program.enums[0].variants[0].name == "Red");
  CHECK(r.program.enums[0].variants[0].payload.empty());
  CHECK(r.program.enums[1].name == "Wrap");
  CHECK(r.program.enums[1].tparams.size() == 1);
  CHECK(r.program.enums[1].variants.size() == 2);
  CHECK(r.program.enums[1].variants[0].name == "Val");
  CHECK(r.program.enums[1].variants[0].payload.size() == 1);
  CHECK(r.program.dump().find("(enum Color (Red) (Green) (Blue))") !=
        std::string::npos);
  std::cout << "testEnumParses OK\n";
  return 0;
}

static int testEnumCtorTargsParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "fn main() { let w = Wrap<i64>.Val(1); print(w); }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump().find("(call-method Wrap Val<i64> 1)") !=
        std::string::npos);
  std::cout << "testEnumCtorTargsParses OK\n";
  return 0;
}

static int testEnumEmptyFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "enum E { }\nfn main() { }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testEnumEmptyFails OK\n";
  return 0;
}

// v1.8.0 (Phase 27): lambda ekspresi + blok + anotasi.
static int testLambdaParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let f = |x: i64| x + 1; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump().find("(lambda (x: i64) (return (+ x 1)))") !=
        std::string::npos);
  std::cout << "testLambdaParses OK\n";
  return 0;
}

static int testLambdaBlockParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns",
                    "fn main() { let f = |x| { print(x); return x; }; }",
                    lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump().find("(lambda (x)") != std::string::npos);
  std::cout << "testLambdaBlockParses OK\n";
  return 0;
}

static int testLambdaEmptyParamsParses() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let f = || 42; }", lexOk);
  CHECK(lexOk);
  CHECK(r.ok());
  CHECK(r.program.dump().find("(lambda () (return 42))") !=
        std::string::npos);
  std::cout << "testLambdaEmptyParamsParses OK\n";
  return 0;
}

static int testLambdaMissingCloseFails() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { let f = |x; }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  std::cout << "testLambdaMissingCloseFails OK\n";
  return 0;
}

// Phase 101 (3.2-frontend): hint "expected statement" menyebut bentuk
// statement aktual (let/print/return/if/while/match/assign/call),
// bukan klaim versi basi ("v0.6.0 supports ...").
static int testStmtHintListsCurrentForms() {
  bool lexOk = false;
  auto r = parseSrc("t.ns", "fn main() { frobnicate; }", lexOk);
  CHECK(lexOk);
  CHECK(!r.ok());
  CHECK(!r.errors.empty());
  CHECK(r.errors[0].message == "expected statement");
  CHECK(r.errors[0].hint.find("match") != std::string::npos);
  CHECK(r.errors[0].hint.find("v0.6.0") == std::string::npos);
  std::cout << "testStmtHintListsCurrentForms OK\n";
  return 0;
}

int main() {

  if (testAnnotatedLet()) return 1;
  if (testUnannotatedStillOk()) return 1;
  if (testMissingTypeFails()) return 1;
  if (testNamedTypeParses()) return 1;
  if (testPrecedenceMulOverAdd()) return 1;
  if (testLeftAssocAdd()) return 1;
  if (testMulThenAdd()) return 1;
  if (testParensOverride()) return 1;
  if (testEqualityLowest()) return 1;
  if (testComparisons()) return 1;
  if (testUnary()) return 1;
  if (testBoolAndDiv()) return 1;
  if (testHelloParses()) return 1;
  if (testPrintIntParses()) return 1;
  if (testNoSemicolonOk()) return 1;
  if (testMissingFnFails()) return 1;
  if (testLetDoubleEqFails()) return 1;
  if (testLetMissingExprFails()) return 1;
  if (testReturnParses()) return 1;
  if (testMasterFunctions()) return 1;
  if (testNestedCalls()) return 1;
  if (testEmptyArgsCall()) return 1;
  if (testMainWithParamsFails()) return 1;
  if (testMissingArrowTypeFails()) return 1;
  if (testUnclosedCallFails()) return 1;
  if (testParamMissingColonFails()) return 1;
  if (testMasterControl()) return 1;
  if (testIfNoElse()) return 1;
  if (testWhile()) return 1;
  if (testNestedIfWhile()) return 1;
  if (testElseIfChain()) return 1;
  if (testMissingBraceFails()) return 1;
  if (testStrayElseFails()) return 1;
  if (testMasterArray()) return 1;
  if (testMasterStruct()) return 1;
  if (testImport()) return 1;
  if (testFieldChain()) return 1;
  if (testFieldIndexMix()) return 1;
  if (testMissingFieldColonFails()) return 1;
  if (testUnclosedStructFails()) return 1;
  if (testMissingDotFieldFails()) return 1;
  if (testBareIdentCondition()) return 1;
  if (testArrayTypeNested()) return 1;
  if (testIndexChain()) return 1;
  if (testUnclosedIndexFails()) return 1;
  if (testUnclosedArrayFails()) return 1;
  if (testAssign()) return 1;
  if (testDeepNestingOk()) return 1;
  if (testTooDeepNestingFails()) return 1;
  if (testTooDeepBlockFails()) return 1;
  if (testUnclosedParenFails()) return 1;
  if (testMultiplePrints()) return 1;
  if (testRecoveryAcrossFunctions()) return 1;
  if (testRecoveryStrayTopLevel()) return 1;
  if (testParserErrorCap()) return 1;
  if (testUnknownTokensFiltered()) return 1;
  if (testTypeAliasParses()) return 1;
  if (testTypeAliasMissingNameFails()) return 1;
  if (testTypeAliasMissingAssignFails()) return 1;
  if (testTypeAliasMissingTargetFails()) return 1;
  if (testConstParses()) return 1;
  if (testConstMissingNameFails()) return 1;
  if (testConstMissingValueFails()) return 1;
  if (testFnTypeParses()) return 1;
  if (testFnTypeEmptyParses()) return 1;
  if (testFnTypeMissingParenFails()) return 1;
  if (testGenericFnParses()) return 1;
  if (testGenericFnMainFails()) return 1;
  if (testGenericFnMissingTparamFails()) return 1;
  if (testGenericStructParses()) return 1;
  if (testTypeAppParses()) return 1;
  if (testExplicitStructLitParses()) return 1;
  if (testGenericStructMissingTparamFails()) return 1;
  if (testTraitParses()) return 1;
  if (testImplParses()) return 1;
  if (testMethodCallParses()) return 1;
  if (testTraitBodyFails()) return 1;
  if (testImplMissingForFails()) return 1;
  if (testSelfAnnotationFails()) return 1;
  if (testPropagateParses()) return 1;
  if (testEnumParses()) return 1;
  if (testEnumCtorTargsParses()) return 1;
  if (testEnumEmptyFails()) return 1;
  if (testLambdaParses()) return 1;
  if (testLambdaBlockParses()) return 1;
  if (testLambdaEmptyParamsParses()) return 1;
  if (testLambdaMissingCloseFails()) return 1;
  if (testDottedImport()) return 1;
  if (testExternParses()) return 1;
  if (testExternBodyFails()) return 1;
  if (testStmtHintListsCurrentForms()) return 1;
  std::cout << "ALL PARSER TESTS PASSED\n";
  return 0;
}
