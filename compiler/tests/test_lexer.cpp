// Unit test lexer v0.2.0 (tanpa framework eksternal; CHECK selalu dievaluasi).
#include "compiler/lexer/Lexer.h"
#include <iostream>
#include <vector>

using namespace nusa;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK FAILED: " #cond " at " __FILE__ ":" << __LINE__      \
                << std::endl;                                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)

// v0.1.0 regression: token hello.ns tidak berubah (12 token).
static int testHelloTokens() {
  std::string src = "fn main() {\n    print(\"Hello, Nusa!\");\n}\n";
  Lexer lx("hello.ns", src);
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 12);
  CHECK(toks[0].kind == TokenKind::Fn);
  CHECK(toks[1].kind == TokenKind::Ident && toks[1].lexeme == "main");
  CHECK(toks[2].kind == TokenKind::LParen);
  CHECK(toks[3].kind == TokenKind::RParen);
  CHECK(toks[4].kind == TokenKind::LBrace);
  CHECK(toks[5].kind == TokenKind::Ident && toks[5].lexeme == "print");
  CHECK(toks[6].kind == TokenKind::LParen);
  CHECK(toks[7].kind == TokenKind::StringLit &&
        toks[7].lexeme == "Hello, Nusa!");
  CHECK(toks[8].kind == TokenKind::RParen);
  CHECK(toks[9].kind == TokenKind::Semi);
  CHECK(toks[10].kind == TokenKind::RBrace);
  CHECK(toks[11].kind == TokenKind::Eof);
  CHECK(toks[5].loc.line == 2);
  std::cout << "testHelloTokens OK\n";
  return 0;
}

static int testKeywords() {
  Lexer lx("k.ns", "fn let return if else while true false");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  std::vector<TokenKind> want = {TokenKind::Fn,    TokenKind::Let,
                                 TokenKind::Return, TokenKind::If,
                                 TokenKind::Else,   TokenKind::While,
                                 TokenKind::True,   TokenKind::False,
                                 TokenKind::Eof};
  CHECK(toks.size() == want.size());
  for (size_t i = 0; i < want.size(); i++)
    CHECK(toks[i].kind == want[i]);
  std::cout << "testKeywords OK\n";
  return 0;
}

// v1.3.0 (Phase 22): keyword trait/impl/for/self.
static int testTraitKeywords() {
  Lexer lx("k.ns", "trait Printable impl for self");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 6);
  CHECK(toks[0].kind == TokenKind::Trait);
  CHECK(toks[1].kind == TokenKind::Ident && toks[1].lexeme == "Printable");
  CHECK(toks[2].kind == TokenKind::Impl);
  CHECK(toks[3].kind == TokenKind::For);
  CHECK(toks[4].kind == TokenKind::Self);
  CHECK(toks[5].kind == TokenKind::Eof);
  std::cout << "testTraitKeywords OK\n";
  return 0;
}

// v1.7.0 (Phase 26): keyword enum.
static int testEnumKeyword() {
  Lexer lx("k.ns", "enum Color");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 3);
  CHECK(toks[0].kind == TokenKind::Enum);
  CHECK(toks[1].kind == TokenKind::Ident && toks[1].lexeme == "Color");
  CHECK(toks[2].kind == TokenKind::Eof);
  std::cout << "testEnumKeyword OK\n";
  return 0;
}

// v1.5.0 (Phase 24): `?` propagasi.
static int testQuestion() {
  Lexer lx("k.ns", "f()?");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 5);
  CHECK(toks[0].kind == TokenKind::Ident);
  CHECK(toks[1].kind == TokenKind::LParen);
  CHECK(toks[2].kind == TokenKind::RParen);
  CHECK(toks[3].kind == TokenKind::Question);
  CHECK(toks[4].kind == TokenKind::Eof);
  Lexer lx2("k.ns", "a?");
  auto t2 = lx2.tokenize();
  CHECK(lx2.errors().empty());
  CHECK(t2.size() == 3);
  CHECK(t2[1].kind == TokenKind::Question);
  std::cout << "testQuestion OK\n";
  return 0;
}

// v1.8.0 (Phase 27): `|` lambda.
static int testPipe() {
  Lexer lx("k.ns", "|x| x");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 5);
  CHECK(toks[0].kind == TokenKind::Pipe);
  CHECK(toks[1].kind == TokenKind::Ident);
  CHECK(toks[2].kind == TokenKind::Pipe);
  CHECK(toks[3].kind == TokenKind::Ident);
  CHECK(toks[4].kind == TokenKind::Eof);
  std::cout << "testPipe OK\n";
  return 0;
}

// v1.13.0 (Phase 32): keyword extern.
static int testExternKeyword() {
  Lexer lx("k.ns", "extern fn");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 3);
  CHECK(toks[0].kind == TokenKind::Extern);
  CHECK(toks[1].kind == TokenKind::Fn);
  CHECK(toks[2].kind == TokenKind::Eof);
  std::cout << "testExternKeyword OK\n";
  return 0;
}
// v2.0: keyword async/await (reserved untuk RFC-0004; tanpa sintaks).
static int testAsyncKeywords() {
  Lexer lx("k.ns", "async await");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 3);
  CHECK(toks[0].kind == TokenKind::Async);
  CHECK(toks[1].kind == TokenKind::Await);
  CHECK(toks[2].kind == TokenKind::Eof);
  std::cout << "testAsyncKeywords OK\n";
  return 0;
}
// v2.20.0 (Phase 69): keyword macro (reserved; tanpa sintaks).
static int testMacroKeyword() {
  Lexer lx("k.ns", "macro");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 2);
  CHECK(toks[0].kind == TokenKind::Macro);
  CHECK(toks[1].kind == TokenKind::Eof);
  std::cout << "testMacroKeyword OK\n";
  return 0;
}
// v2.21.0 (Phase 70, RFC-0009 slice 1): keyword const.
static int testConstKeyword() {
  Lexer lx("k.ns", "const");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 2);
  CHECK(toks[0].kind == TokenKind::Const);
  CHECK(toks[1].kind == TokenKind::Eof);
  std::cout << "testConstKeyword OK\n";
  return 0;
}
static int testTypeAliasKeyword() {
  Lexer lx("k.ns", "type Nums = [i64];");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 8);
  CHECK(toks[0].kind == TokenKind::TypeAlias);
  CHECK(toks[1].kind == TokenKind::Ident && toks[1].lexeme == "Nums");
  CHECK(toks[2].kind == TokenKind::Assign);
  CHECK(toks[3].kind == TokenKind::LBracket);
  CHECK(toks[4].kind == TokenKind::I64);
  CHECK(toks[5].kind == TokenKind::RBracket);
  CHECK(toks[6].kind == TokenKind::Semi);
  CHECK(toks[7].kind == TokenKind::Eof);
  std::cout << "testTypeAliasKeyword OK\n";
  return 0;
}

static int testIntLiterals() {
  Lexer lx("i.ns", "0 42 10 007 9223372036854775807");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 6); // 5 int + EOF
  CHECK(toks[0].kind == TokenKind::IntLit && toks[0].lexeme == "0");
  CHECK(toks[1].kind == TokenKind::IntLit && toks[1].lexeme == "42");
  CHECK(toks[4].kind == TokenKind::IntLit &&
        toks[4].lexeme == "9223372036854775807");
  std::cout << "testIntLiterals OK\n";
  return 0;
}

static int testOperators() {
  Lexer lx("o.ns", "+ - * / = == != < > <= >=");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  std::vector<TokenKind> want = {TokenKind::Plus,  TokenKind::Minus,
                                 TokenKind::Star,  TokenKind::Slash,
                                 TokenKind::Assign, TokenKind::Eq,
                                 TokenKind::NotEq, TokenKind::Lt,
                                 TokenKind::Gt,    TokenKind::LtEq,
                                 TokenKind::GtEq,  TokenKind::Eof};
  CHECK(toks.size() == want.size());
  for (size_t i = 0; i < want.size(); i++)
    CHECK(toks[i].kind == want[i]);
  std::cout << "testOperators OK\n";
  return 0;
}

static int testSymbols() {
  Lexer lx("s.ns", "( ) { } [ ] : , ;");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  std::vector<TokenKind> want = {TokenKind::LParen,   TokenKind::RParen,
                                 TokenKind::LBrace,   TokenKind::RBrace,
                                 TokenKind::LBracket, TokenKind::RBracket,
                                 TokenKind::Colon,    TokenKind::Comma,
                                 TokenKind::Semi,     TokenKind::Eof};
  CHECK(toks.size() == want.size());
  for (size_t i = 0; i < want.size(); i++)
    CHECK(toks[i].kind == want[i]);
  std::cout << "testSymbols OK\n";
  return 0;
}

static int testLocations() {
  Lexer lx("a.ns", "// komentar\nlet x = 10;\n");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks[0].kind == TokenKind::Let);
  CHECK(toks[0].loc.line == 2 && toks[0].loc.column == 1);
  CHECK(toks[1].kind == TokenKind::Ident && toks[1].lexeme == "x");
  CHECK(toks[1].loc.column == 5);
  CHECK(toks[3].kind == TokenKind::IntLit && toks[3].lexeme == "10");
  CHECK(toks[3].loc.column == 9);
  std::cout << "testLocations OK\n";
  return 0;
}

static int testSlashVsComment() {
  Lexer lx("d.ns", "10 / 2 // bagi\n");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 4);
  CHECK(toks[1].kind == TokenKind::Slash);
  std::cout << "testSlashVsComment OK\n";
  return 0;
}

static int testLoneBangFails() {
  Lexer lx("b.ns", "let x = !y;");
  auto toks = lx.tokenize();
  (void)toks;
  CHECK(!lx.errors().empty());
  std::cout << "testLoneBangFails OK\n";
  return 0;
}

static int testIntTrailingAlphaFails() {
  Lexer lx("c.ns", "let x = 123abc;");
  auto toks = lx.tokenize();
  (void)toks;
  CHECK(!lx.errors().empty());
  std::cout << "testIntTrailingAlphaFails OK\n";
  return 0;
}

static int testIntOverflowFails() {
  Lexer lx("o.ns", "let x = 9223372036854775808;");
  auto toks = lx.tokenize();
  (void)toks;
  CHECK(!lx.errors().empty());
  std::cout << "testIntOverflowFails OK\n";
  return 0;
}

static int testUnterminatedString() {
  Lexer lx("bad.ns", "fn main() { print(\"oops); }");
  auto toks = lx.tokenize();
  (void)toks;
  CHECK(!lx.errors().empty());
  std::cout << "testUnterminatedString OK\n";
  return 0;
}

static int testUnexpectedChar() {
  Lexer lx("bad2.ns", "fn main() { @ }");
  auto toks = lx.tokenize();
  (void)toks;
  CHECK(!lx.errors().empty());
  std::cout << "testUnexpectedChar OK\n";
  return 0;
}

// v1.1.0: batas error lexer — 40 karakter liar menghasilkan tepat
// kMaxErrors + 1 error (terakhir: ringkasan "too many errors").
static int testLexerErrorCap() {
  std::string src;
  for (int i = 0; i < 40; i++)
    src += "@";
  Lexer lx("cap.ns", src);
  (void)lx.tokenize();
  CHECK(lx.errors().size() == Lexer::kMaxErrors + 1);
  CHECK(lx.errors().back().find("too many errors") != std::string::npos);
  std::cout << "testLexerErrorCap OK\n";
  return 0;
}

static int testTypeKeywords() {
  Lexer lx("t.ns", "i64 bool string void");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 5);
  CHECK(toks[0].kind == TokenKind::I64);
  CHECK(toks[1].kind == TokenKind::Bool);
  CHECK(toks[2].kind == TokenKind::String);
  CHECK(toks[3].kind == TokenKind::Void);
  CHECK(toks[4].kind == TokenKind::Eof);
  std::cout << "testTypeKeywords OK\n";
  return 0;
}

static int testAnnotatedLetTokens() {
  Lexer lx("t.ns", "let age: i64 = 15;");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks.size() == 8); // let age : i64 = 15 ; EOF
  CHECK(toks[0].kind == TokenKind::Let);
  CHECK(toks[1].kind == TokenKind::Ident && toks[1].lexeme == "age");
  CHECK(toks[2].kind == TokenKind::Colon);
  CHECK(toks[3].kind == TokenKind::I64);
  CHECK(toks[4].kind == TokenKind::Assign);
  CHECK(toks[5].kind == TokenKind::IntLit && toks[5].lexeme == "15");
  CHECK(toks[6].kind == TokenKind::Semi);
  std::cout << "testAnnotatedLetTokens OK\n";
  return 0;
}

static int testArrow() {
  Lexer lx("t.ns", "fn add(a: i64) -> i64 { return a; }");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  // fn add ( a : i64 ) -> i64 { return a ; } EOF = 15
  CHECK(toks.size() == 15);
  CHECK(toks[7].kind == TokenKind::Arrow && toks[7].lexeme == "->");
  // Minus biasa tetap minus (ada spasi maupun tidak, selama bukan `->`).
  Lexer lx2("t.ns", "a - b");
  auto toks2 = lx2.tokenize();
  CHECK(lx2.errors().empty());
  CHECK(toks2[1].kind == TokenKind::Minus);
  std::cout << "testArrow OK\n";
  return 0;
}

static int testStructImportTokens() {
  Lexer lx("t.ns", "struct Person { name: string; } import math; p.age");
  auto toks = lx.tokenize();
  CHECK(lx.errors().empty());
  CHECK(toks[0].kind == TokenKind::Struct);
  CHECK(toks[1].kind == TokenKind::Ident && toks[1].lexeme == "Person");
  // struct(0) Person(1) {(2) name(3) :(4) string(5) ;(6) }(7)
  // import(8) math(9) ;(10) p(11) .(12) age(13) EOF(14)
  CHECK(toks[11].kind == TokenKind::Ident && toks[11].lexeme == "p");
  CHECK(toks[12].kind == TokenKind::Dot);
  CHECK(toks[13].kind == TokenKind::Ident && toks[13].lexeme == "age");
  bool sawImport = false;
  for (const auto &t : toks)
    if (t.kind == TokenKind::Import)
      sawImport = true;
  CHECK(sawImport);
  std::cout << "testStructImportTokens OK\n";
  return 0;
}

int main() {
  if (testHelloTokens()) return 1;
  if (testKeywords()) return 1;
  if (testTypeKeywords()) return 1;
  if (testAnnotatedLetTokens()) return 1;
  if (testArrow()) return 1;
  if (testStructImportTokens()) return 1;
  if (testIntLiterals()) return 1;
  if (testOperators()) return 1;
  if (testSymbols()) return 1;
  if (testLocations()) return 1;
  if (testSlashVsComment()) return 1;
  if (testLoneBangFails()) return 1;
  if (testIntTrailingAlphaFails()) return 1;
  if (testIntOverflowFails()) return 1;
  if (testUnterminatedString()) return 1;
  if (testUnexpectedChar()) return 1;
  if (testLexerErrorCap()) return 1;
  if (testTypeAliasKeyword()) return 1;
  if (testTraitKeywords()) return 1;
  if (testEnumKeyword()) return 1;
  if (testQuestion()) return 1;
  if (testPipe()) return 1;
  if (testExternKeyword()) return 1;
  if (testAsyncKeywords()) return 1;
  if (testMacroKeyword()) return 1;
  if (testConstKeyword()) return 1;
  std::cout << "ALL LEXER TESTS PASSED\n";
  return 0;
}
