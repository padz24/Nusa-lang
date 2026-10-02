#pragma once
#include "compiler/ast/AST.h"
#include "compiler/lexer/Token.h"
#include <memory>
#include <string>
#include <vector>

namespace nusa {

struct ParseError {
  Location loc;
  std::string message;
  std::string hint; // opsional, boleh kosong
  int length = 1;   // underline span (lihat SemanticError)
};

struct ParseResult {
  Program program;
  std::vector<ParseError> errors;
  bool ok() const { return errors.empty(); }
};

// Recursive-descent parser v0.5.0: multi-fungsi + precedence climbing
// (equality → comparison → term → factor → unary → primary).
// Lihat compiler/ast/AST.h untuk grammar lengkap.
class Parser {
public:
  Parser(std::string file, std::vector<Token> tokens,
         std::vector<std::string> sourceLines);

  ParseResult parse();

  // Batas error anti-banjir (v1.1.0): errorAt membuang error
  // ke-(kMaxErrors+1) dst, lalu tepat satu ringkasan "too many errors".
  static constexpr size_t kMaxErrors = 25;

private:
  const Token &peek() const;
  const Token &peekNext() const; // lookahead (Eof bila habis)
  const Token &peekAt(size_t n) const; // lookahead-n (Eof bila habis)
  const Token &previous() const;
  bool atEnd() const;
  bool check(TokenKind k) const;
  bool match(TokenKind k);
  void errorAt(const Location &loc, const std::string &msg,
               const std::string &hint = "");
  // Sinkronisasi: lompat ke Semi/RBrace/Eof agar satu error tidak
  // menenggelamkan error lain.
  void synchronize();
  // Sinkronisasi top-level (v1.1.0): lompat ke awal item berikutnya
  // (`import`/`type`/`struct`/`fn`/Eof) agar error di satu item tidak
  // menenggelamkan error di item lain.
  void synchronizeTopLevel();
  // Sinkronisasi body trait/impl (Phase 22): ke `fn`/`}`/`;`/Eof.
  void synchronizeTrait();
  bool capped_ = false;

  bool parseFuncDecl();
  bool parseStructDecl();  bool parseImport();
  // Enum `enum E { V, W(T) }` (Phase 26, v1.7.0).
  bool parseEnumDecl();
  // Alias tipe `type X = T;` (Phase 21a, v1.2.0).
  bool parseTypeAlias();
  // Konstanta `const N [: T] = <expr>;` (Phase 70, v2.21.0, RFC-0009).
  bool parseConstDecl();
  // Trait `trait T { ... }` dan impl `impl T for U { ... }` (Phase 22).
  bool parseTrait();
  bool parseImpl();
  // Target metode trait/impl untuk parseFuncDecl (Phase 22): bila
  // non-null, hasil ditaruh ke vec (bukan functions); noBody untuk
  // metode trait (signature saja, tanpa `{...}`).
  struct MethodOut {
    std::vector<FunctionDecl> *vec = nullptr;
    bool noBody = false;
  };
  MethodOut *methodOut_ = nullptr;
  // `extern fn` berikutnya (Phase 32): signature tanpa body.
  bool externNext_ = false;
  bool parseBlock(Block &out);
  // Mengembalikan false jika token saat ini bukan awal statement yang valid.
  bool parseStmt(Block &out);
  // Statement `match` (Phase 25) + pola.
  bool parseMatchStmt(Block &out);
  std::unique_ptr<Pat> parsePattern();
  // True bila statement diawali panggilan method (`p.m(...)`, Phase 22).
  bool isMethodCallStart();
  bool parseLetStmt(Block &out);
  bool parsePrintStmt(Block &out);
  bool parseReturnStmt(Block &out);
  bool parseIfStmt(Block &out);
  bool parseWhileStmt(Block &out);
  // Parse anotasi tipe `i64 | bool | string | void` (v0.4.0).
  // nullptr jika token saat ini bukan tipe.
  std::unique_ptr<Type> parseType();

  std::unique_ptr<Expr> parseExpr();
  std::unique_ptr<Expr> parseEquality();
  std::unique_ptr<Expr> parseComparison();
  std::unique_ptr<Expr> parseTerm();
  std::unique_ptr<Expr> parseFactor();
  std::unique_ptr<Expr> parseUnary();
  std::unique_ptr<Expr> parsePrimary();
  // Atom tanpa postfix, lalu `[...]` berantai (v0.7.0).
  std::unique_ptr<Expr> parseAtom();

  // Konsumsi ';' opsional (v0.1.0 menerima tanpa ';' — dipertahankan).
  void consumeOptionalSemi();

  // Batas rekursi anti-stack-overflow (v0.9.5 hardening): parser
  // recursive-descent runtuh (SIGSEGV) pada input nesting puluhan ribu.
  // Guard RAII menolak bersih di atas batas yang jauh di luar kebutuhan
  // wajar (< 20), sekaligus membatasi rekursi tahap hilir (AST ≤ batas).
  static constexpr int kMaxDepth = 500;
  struct DepthGuard {
    Parser *parser;
    bool exceeded = false;
    DepthGuard(Parser *parser, const Location &loc, const std::string &msg)
        : parser(parser) {
      if (++parser->depth_ > Parser::kMaxDepth) {
        exceeded = true;
         parser->errorAt(loc, msg, "simplify this nested construct");
      }
    }
    ~DepthGuard() { parser->depth_--; }
  };

  std::string file_;
  std::vector<Token> tokens_;
  size_t pos_ = 0;
  std::vector<std::string> lines_;
  ParseResult result_;
  int depth_ = 0; // dipakai DepthGuard (lihat atas)
};

} // namespace nusa
