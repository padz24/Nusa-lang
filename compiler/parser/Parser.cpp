#include "compiler/parser/Parser.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <utility>

namespace nusa {

Parser::Parser(std::string file, std::vector<Token> tokens,
               std::vector<std::string> sourceLines)
    : file_(std::move(file)), tokens_(std::move(tokens)),
      lines_(std::move(sourceLines)) {
  // Token Unknown = kerusakan yang SUDAH dilaporkan lexer ("unexpected
  // character", string tak-terminasi, ...). Buang di sini agar parser
  // tidak melaporkan error duplikat untuk kerusakan yang sama (v1.1.0).
  // Token Eof (selalu terakhir) tidak pernah Unknown — aman.
  tokens_.erase(std::remove_if(tokens_.begin(), tokens_.end(),
                               [](const Token &t) {
                                 return t.kind == TokenKind::Unknown;
                               }),
                tokens_.end());
}

const Token &Parser::peek() const { return tokens_[pos_]; }
const Token &Parser::peekNext() const {
  return peekAt(1);
}
const Token &Parser::peekAt(size_t n) const {
  if (pos_ + n < tokens_.size())
    return tokens_[pos_ + n];
  return tokens_.back(); // selalu Eof
}
const Token &Parser::previous() const { return tokens_[pos_ - 1]; }
bool Parser::atEnd() const { return peek().kind == TokenKind::Eof; }
bool Parser::check(TokenKind k) const {
  if (atEnd() && k != TokenKind::Eof)
    return false;
  return peek().kind == k;
}
bool Parser::match(TokenKind k) {
  if (check(k)) {
    if (!atEnd())
      pos_++;
    return true;
  }
  return false;
}

void Parser::errorAt(const Location &loc, const std::string &msg,
                     const std::string &hint) {
  if (capped_)
    return;
  if (result_.errors.size() >= kMaxErrors) {
    capped_ = true;
    result_.errors.push_back(ParseError{loc, "too many errors, stopping", ""});
    return;
  }
  result_.errors.push_back(ParseError{loc, msg, hint});
}

void Parser::synchronize() {
  while (!atEnd()) {
    if (peek().kind == TokenKind::Semi || peek().kind == TokenKind::RBrace) {
      if (!atEnd())
        pos_++;
      return;
    }
    pos_++;
  }
}

void Parser::synchronizeTopLevel() {
  while (!atEnd()) {
    TokenKind k = peek().kind;
    if (k == TokenKind::Import || k == TokenKind::Struct ||
        k == TokenKind::Fn || k == TokenKind::TypeAlias ||
        k == TokenKind::Trait || k == TokenKind::Impl ||
        k == TokenKind::Const)
      return;
    pos_++;
  }
}

void Parser::synchronizeTrait() {
  while (!atEnd()) {
    TokenKind k = peek().kind;
    if (k == TokenKind::Fn || k == TokenKind::RBrace)
      return;
    if (k == TokenKind::Semi) {
      pos_++;
      return;
    }
    pos_++;
  }
}

void Parser::consumeOptionalSemi() { match(TokenKind::Semi); }

ParseResult Parser::parse() {
  // v0.8.0: item top-level bebas campur — `import`, `struct`, `fn`
  // (`fn main` wajib ada — dicek di semantic agar pesannya konsisten).
  result_.program.file = file_;
  while (!atEnd()) {
    if (capped_)
      break;
    size_t start = pos_;
    bool ok = false;
    if (check(TokenKind::Import))
      ok = parseImport();
    else if (check(TokenKind::TypeAlias))
      ok = parseTypeAlias();
    else if (check(TokenKind::Const))
      ok = parseConstDecl();
    else if (check(TokenKind::Trait))
      ok = parseTrait();
    else if (check(TokenKind::Impl))
      ok = parseImpl();
    else if (check(TokenKind::Struct))
      ok = parseStructDecl();
    else if (check(TokenKind::Enum))
      ok = parseEnumDecl();
    else if (check(TokenKind::Extern)) {
      match(TokenKind::Extern);
      if (!check(TokenKind::Fn)) {
        errorAt(peek().loc, "expected 'fn' after 'extern'",
                "example: extern fn puts(message: string) -> i64;");
        ok = false;
      } else {
        externNext_ = true;
        ok = parseFuncDecl();
        externNext_ = false;
      }
    } else if (check(TokenKind::Fn))
      ok = parseFuncDecl();
    else {
      errorAt(peek().loc, "expected 'import', 'type', 'const', 'trait', 'impl', 'struct', 'enum', 'extern', or 'fn' declaration",
              "example: import math;  type Nums = [i64];  const MAX = 100;  trait P { fn print(self); }  impl P for Person { ... }  struct P { x: i64; }  enum Color { Red, Green }  extern fn puts(message: string) -> i64;  fn f() { }");
      // Lewati token liar lalu lanjut (recovery v1.1.0) — bukan berhenti.
      if (pos_ < tokens_.size())
        pos_++;
      continue;
    }
    if (!ok) {
      // Recovery v1.1.0: lanjut ke item berikutnya agar error di satu
      // fungsi/struct tidak menenggelamkan error di item lain.
      synchronizeTopLevel();
      if (pos_ == start)
        break; // pengaman anti-hang: tak ada progres
    }
  }
  return std::move(result_);
}

bool Parser::parseImport() {
  Location loc = peek().loc;
  match(TokenKind::Import);
  // Nama modul = komponen path: Ident biasa, atau keyword tipe yang
  // kebetulan cocok (`import string;` → std/string.ns). Keyword struktural
  // (fn/let/...) tetap ditolak.
  TokenKind k = peek().kind;
  bool nameOk = (k == TokenKind::Ident || k == TokenKind::I64 ||
                 k == TokenKind::Bool || k == TokenKind::String ||
                 k == TokenKind::Void);
  if (!nameOk) {
    errorAt(peek().loc, "expected module name after 'import'",
            "example: import math;");
    return false;
  }
  std::string module = peek().lexeme;
  match(k);
  // Path bertitik `std.time` (Phase 29): rantai Ident yang digabung.
  while (check(TokenKind::Dot) && peekAt(1).kind == TokenKind::Ident) {
    match(TokenKind::Dot);
    module += ".";
    module += peek().lexeme;
    match(TokenKind::Ident);
  }
  consumeOptionalSemi();
  Import im;
  im.module = std::move(module);
  im.loc = loc;
  result_.program.imports.push_back(std::move(im));
  return true;
}

bool Parser::parseTypeAlias() {
  Location loc = peek().loc;
  match(TokenKind::TypeAlias);
  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected type alias name after 'type'",
            "example: type Nums = [i64];");
    return false;
  }
  std::string name = peek().lexeme;
  match(TokenKind::Ident);
  if (!match(TokenKind::Assign)) {
    errorAt(peek().loc, "expected '=' after type alias name '" + name + "'",
            "example: type " + name + " = [i64];");
    return false;
  }
  auto target = parseType();
  if (!target)
    return false;
  consumeOptionalSemi();
  TypeAliasDecl ad;
  ad.name = std::move(name);
  ad.target = std::move(*target);
  ad.loc = loc;
  result_.program.aliases.push_back(std::move(ad));
  return true;
}

bool Parser::parseConstDecl() {
  Location loc = peek().loc;
  match(TokenKind::Const);
  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected constant name after 'const'",
            "example: const MAX = 100;");
    return false;
  }
  std::string name = peek().lexeme;
  match(TokenKind::Ident);
  std::optional<Type> annotated;
  if (check(TokenKind::Colon)) {
    match(TokenKind::Colon);
    auto target = parseType();
    if (!target)
      return false;
    annotated = std::move(*target);
  }
  if (!match(TokenKind::Assign)) {
    errorAt(peek().loc, "expected '=' after constant name '" + name + "'",
            "example: const " + name + " = 100;");
    return false;
  }
  auto value = parseExpr();
  if (!value)
    return false;
  consumeOptionalSemi();
  ConstDecl cd;
  cd.name = std::move(name);
  cd.loc = loc;
  cd.declaredType = std::move(annotated);
  cd.value = std::move(value);
  result_.program.consts.push_back(std::move(cd));
  return true;
}

bool Parser::parseStructDecl() {
  Location structLoc = peek().loc;
  match(TokenKind::Struct);
  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected struct name after 'struct'",
            "example: struct Person { name: string; age: i64; }");
    return false;
  }
  std::string name = peek().lexeme;
  match(TokenKind::Ident);
  // Parameter tipe generik (Phase 21d): `struct Point<T> { ... }`.
  // Batasan trait (Phase 22): `struct B<T: Printable> { ... }`.
  std::vector<std::string> tparams;
  std::unordered_map<std::string, std::vector<std::string>> tparamBounds;
  if (match(TokenKind::Lt)) {
    if (!check(TokenKind::Ident)) {
      errorAt(peek().loc, "expected type parameter name after '<'",
              "example: struct " + name + "<T> { ... }");
      return false;
    }
    while (true) {
      if (!check(TokenKind::Ident)) {
        errorAt(peek().loc, "expected type parameter name",
                "example: struct " + name + "<T, U> { ... }");
        return false;
      }
      std::string tp = peek().lexeme;
      match(TokenKind::Ident);
      if (match(TokenKind::Colon)) {
        std::vector<std::string> bounds;
        while (true) {
          if (!check(TokenKind::Ident)) {
            errorAt(peek().loc, "expected trait name in bound",
                    "example: struct " + name + "<T: Printable> { ... }");
            return false;
          }
          bounds.push_back(peek().lexeme);
          match(TokenKind::Ident);
          if (match(TokenKind::Plus))
            continue;
          break;
        }
        tparamBounds[tp] = std::move(bounds);
      }
      tparams.push_back(std::move(tp));
      if (match(TokenKind::Comma))
        continue;
      break;
    }
    if (!match(TokenKind::Gt)) {
      errorAt(peek().loc, "expected '>' after type parameters",
              "example: struct " + name + "<T> { ... }");
      return false;
    }
  }
  if (!match(TokenKind::LBrace)) {
    errorAt(peek().loc, "expected '{' after struct name",
            "example: struct " + name + " { ... }");
    return false;
  }
  StructDecl decl;
  decl.name = std::move(name);
  decl.loc = structLoc;
  decl.tparams = std::move(tparams);
  decl.tparamBounds = std::move(tparamBounds);
  while (!check(TokenKind::RBrace) && !atEnd()) {
    if (!check(TokenKind::Ident)) {
      errorAt(peek().loc, "expected field name",
              "example: name: string;");
      return false;
    }
    StructField field;
    field.name = peek().lexeme;
    field.loc = peek().loc;
    match(TokenKind::Ident);
    if (!match(TokenKind::Colon)) {
      errorAt(peek().loc,
              "expected ':' after field '" + field.name + "'",
              "example: " + field.name + ": i64;");
      return false;
    }
    auto ty = parseType();
    if (!ty)
      return false;
    field.type = *ty;
    decl.fields.push_back(std::move(field));
    consumeOptionalSemi();
  }
  if (!match(TokenKind::RBrace)) {
    errorAt(peek().loc, "expected '}' to close struct declaration");
    return false;
  }
  result_.program.structs.push_back(std::move(decl));
  return result_.errors.empty();
}

bool Parser::parseEnumDecl() {
  Location enumLoc = peek().loc;
  match(TokenKind::Enum);
  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected enum name after 'enum'",
            "example: enum Color { Red, Green }");
    return false;
  }
  std::string name = peek().lexeme;
  match(TokenKind::Ident);
  // Parameter tipe generik (cermin struct): `enum Result<T, E> { ... }`.
  std::vector<std::string> tparams;
  std::unordered_map<std::string, std::vector<std::string>> tparamBounds;
  if (match(TokenKind::Lt)) {
    if (!check(TokenKind::Ident)) {
      errorAt(peek().loc, "expected type parameter name after '<'",
              "example: enum " + name + "<T> { ... }");
      return false;
    }
    while (true) {
      if (!check(TokenKind::Ident)) {
        errorAt(peek().loc, "expected type parameter name",
                "example: enum " + name + "<T, E> { ... }");
        return false;
      }
      std::string tp = peek().lexeme;
      match(TokenKind::Ident);
      if (match(TokenKind::Colon)) {
        std::vector<std::string> bounds;
        while (true) {
          if (!check(TokenKind::Ident)) {
            errorAt(peek().loc, "expected trait name in bound",
                    "example: enum " + name + "<T: Printable> { ... }");
            return false;
          }
          bounds.push_back(peek().lexeme);
          match(TokenKind::Ident);
          if (match(TokenKind::Plus))
            continue;
          break;
        }
        tparamBounds[tp] = std::move(bounds);
      }
      tparams.push_back(std::move(tp));
      if (match(TokenKind::Comma))
        continue;
      break;
    }
    if (!match(TokenKind::Gt)) {
      errorAt(peek().loc, "expected '>' after type parameters",
              "example: enum " + name + "<T> { ... }");
      return false;
    }
  }
  if (!match(TokenKind::LBrace)) {
    errorAt(peek().loc, "expected '{' after enum name",
            "example: enum " + name + " { Red, Green }");
    return false;
  }
  EnumDecl decl;
  decl.name = std::move(name);
  decl.loc = enumLoc;
  decl.tparams = std::move(tparams);
  decl.tparamBounds = std::move(tparamBounds);
  if (check(TokenKind::RBrace)) {
    errorAt(peek().loc, "enum needs at least one variant",
            "example: enum Color { Red }");
    return false;
  }
  while (!check(TokenKind::RBrace) && !atEnd()) {
    if (!check(TokenKind::Ident)) {
      errorAt(peek().loc, "expected variant name",
              "example: Red, Green(i64)");
      return false;
    }
    EnumVariant v;
    v.name = peek().lexeme;
    v.loc = peek().loc;
    match(TokenKind::Ident);
    // Payload posisional opsional: `Ok(T)`, `Red()` = unit.
    if (match(TokenKind::LParen)) {
      if (!check(TokenKind::RParen)) {
        while (true) {
          auto ty = parseType();
          if (!ty)
            return false;
          v.payload.push_back(*ty);
          if (match(TokenKind::Comma))
            continue;
          break;
        }
      }
      if (!match(TokenKind::RParen)) {
        errorAt(peek().loc, "expected ')' after variant payload types",
                "example: Ok(T)");
        return false;
      }
    }
    decl.variants.push_back(std::move(v));
    if (!match(TokenKind::Comma) && !check(TokenKind::RBrace) && !atEnd()) {
      errorAt(peek().loc, "expected ',' between enum variants",
              "example: Red, Green(i64),");
      return false;
    }
  }
  if (!match(TokenKind::RBrace)) {
    errorAt(peek().loc, "expected '}' to close enum declaration");
    return false;
  }
  result_.program.enums.push_back(std::move(decl));
  return result_.errors.empty();
}

bool Parser::parseFuncDecl() {
  Location fnLoc = peek().loc;
  match(TokenKind::Fn);

  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected function name after 'fn'");
    return false;
  }
  std::string name = peek().lexeme;
  Location nameLoc = peek().loc;
  match(TokenKind::Ident);

  // Parameter tipe generik (Phase 21c): `fn f<T, U>(...)`.
  // Batasan trait (Phase 22): `fn f<T: Printable + Debug>(...)`.
  std::vector<std::string> tparams;
  std::unordered_map<std::string, std::vector<std::string>> tparamBounds;
  if (match(TokenKind::Lt)) {
    if (!check(TokenKind::Ident)) {
      errorAt(peek().loc, "expected type parameter name after '<'",
              "example: fn " + name + "<T>(x: T) -> T { ... }");
      return false;
    }
    while (true) {
      if (!check(TokenKind::Ident)) {
        errorAt(peek().loc, "expected type parameter name",
                "example: fn " + name + "<T, U>(...) { ... }");
        return false;
      }
      std::string tp = peek().lexeme;
      match(TokenKind::Ident);
      // Batasan opsional: `T: TraitA + TraitB`.
      if (match(TokenKind::Colon)) {
        std::vector<std::string> bounds;
        while (true) {
          if (!check(TokenKind::Ident)) {
            errorAt(peek().loc, "expected trait name in bound",
                    "example: fn " + name + "<T: Printable>(x: T) { ... }");
            return false;
          }
          bounds.push_back(peek().lexeme);
          match(TokenKind::Ident);
          if (match(TokenKind::Plus))
            continue;
          break;
        }
        tparamBounds[tp] = std::move(bounds);
      }
      tparams.push_back(std::move(tp));
      if (match(TokenKind::Comma))
        continue;
      break;
    }
    if (!match(TokenKind::Gt)) {
      errorAt(peek().loc, "expected '>' after type parameters",
              "example: fn " + name + "<T>(x: T) -> T { ... }");
      return false;
    }
  if (name == "main" && !externNext_) {
      errorAt(nameLoc, "'fn main' cannot be generic",
              "'fn main' is the entry point: fn main() { ... }");
      return false;
    }
  }

  if (!match(TokenKind::LParen)) {
    errorAt(peek().loc, "expected '(' after function name",
            "example: fn " + name + "(a: i64) -> i64 { ... }");
    return false;
  }
  // Parameter: `name: Type` dipisah koma (boleh kosong).
  // Receiver `self` (Phase 22): hanya posisi pertama, tanpa anotasi.
  std::vector<Param> params;
  if (!check(TokenKind::RParen)) {
    while (true) {
      if (params.empty() && check(TokenKind::Self)) {
        Param param;
        param.name = "self";
        param.loc = peek().loc;
        match(TokenKind::Self);
        if (check(TokenKind::Colon)) {
          errorAt(peek().loc, "unexpected ':' after 'self'",
                  "'self' takes no type annotation");
          return false;
        }
        // Marker; checker mengisi tipe target di metode impl.
        param.type = Type::named("self");
        params.push_back(std::move(param));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
      if (!check(TokenKind::Ident)) {
        errorAt(peek().loc, "expected parameter name",
                "example: fn " + name + "(a: i64, b: i64) -> i64 { ... }");
        return false;
      }
      Param param;
      param.name = peek().lexeme;
      param.loc = peek().loc;
      match(TokenKind::Ident);
      if (!match(TokenKind::Colon)) {
        errorAt(peek().loc,
                "expected ':' after parameter '" + param.name + "'",
                "example: " + param.name + ": i64");
        return false;
      }
      auto ty = parseType();
      if (!ty)
        return false;
      param.type = *ty;
      params.push_back(std::move(param));
      if (match(TokenKind::Comma))
        continue;
      break;
    }
  }
  if (!match(TokenKind::RParen)) {
    errorAt(peek().loc, "expected ')' after parameter list");
    return false;
  }

  // Return type opsional: `-> Type` (absen = void).
  Type retType = Type::void_();
  if (match(TokenKind::Arrow)) {
    auto ty = parseType();
    if (!ty)
      return false;
    retType = *ty;
  }

  // Bentuk `main`: tanpa parameter, return void (aturan v0.1.0–v0.4.0
  // dipertahankan; selebihnya fungsi bebas).
  if (name == "main") {
    if (!params.empty()) {
      errorAt(nameLoc, "'fn main' must not take parameters",
              "'fn main' is the entry point: fn main() { ... }");
    }
    if (retType.kind != TypeKind::Void) {
      errorAt(nameLoc, "'fn main' must return 'void'",
              "remove '-> " + retType.name() + "' from 'fn main'");
    }
  }

  Block body;
  if (externNext_) {
    // Deklarasi eksternal C (Phase 32): signature saja, tanpa body.
    if (check(TokenKind::LBrace)) {
      errorAt(peek().loc, "extern functions have no bodies",
              "write the signature only, e.g. extern fn puts(message: string) -> i64;");
      return false;
    }
    consumeOptionalSemi();
  } else if (methodOut_ && methodOut_->noBody) {
    // Metode trait (Phase 22): signature saja; body dilarang.
    if (check(TokenKind::LBrace)) {
      errorAt(peek().loc, "trait methods have no bodies",
              "write the signature only, e.g. fn print(self);");
      return false;
    }
    consumeOptionalSemi();
  } else if (!parseBlock(body)) {
    return false;
  }

  FunctionDecl fn;
  fn.name = std::move(name);
  fn.loc = fnLoc;
  fn.isExtern = externNext_;
  fn.tparams = std::move(tparams);
  fn.tparamBounds = std::move(tparamBounds);
  fn.params = std::move(params);
  fn.retType = retType;
  fn.body = std::move(body);
  if (methodOut_ && methodOut_->vec)
    methodOut_->vec->push_back(std::move(fn));
  else
    result_.program.functions.push_back(std::move(fn));
  return result_.errors.empty();
}

bool Parser::parseTrait() {
  Location loc = peek().loc;
  match(TokenKind::Trait);
  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected trait name after 'trait'",
            "example: trait Printable { fn print(self); }");
    return false;
  }
  std::string name = peek().lexeme;
  match(TokenKind::Ident);
  if (!match(TokenKind::LBrace)) {
    errorAt(peek().loc, "expected '{' after trait name '" + name + "'",
            "example: trait " + name + " { fn print(self); }");
    return false;
  }
  TraitDecl td;
  td.name = std::move(name);
  td.loc = loc;
  MethodOut mo{&td.methods, true};
  MethodOut *saved = methodOut_;
  methodOut_ = &mo;
  while (!check(TokenKind::RBrace) && !atEnd()) {
    if (!check(TokenKind::Fn)) {
      errorAt(peek().loc, "expected method declaration",
              "example: fn print(self);");
      synchronizeTrait();
    } else if (!parseFuncDecl()) {
      // Gagal di tengah metode → lewati sisanya (hindari error ganda).
      // (Aman seusai sukses: posisi sudah di batas, synchronize diam.)
      synchronizeTrait();
    }
    if (check(TokenKind::RBrace) || atEnd())
      break;
  }
  methodOut_ = saved;
  if (!match(TokenKind::RBrace)) {
    errorAt(peek().loc, "expected '}' to close trait '" + td.name + "'");
    return false;
  }
  result_.program.traits.push_back(std::move(td));
  return result_.errors.empty();
}

bool Parser::parseImpl() {
  Location loc = peek().loc;
  match(TokenKind::Impl);
  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected trait name after 'impl'",
            "example: impl Printable for Person { ... }");
    return false;
  }
  std::string trait = peek().lexeme;
  Location traitLoc = peek().loc;
  match(TokenKind::Ident);
  if (!match(TokenKind::For)) {
    errorAt(peek().loc, "expected 'for' after trait name in impl",
            "example: impl " + trait + " for Person { ... }");
    return false;
  }
  auto target = parseType();
  if (!target)
    return false;
  if (!match(TokenKind::LBrace)) {
    errorAt(peek().loc, "expected '{' to begin impl block",
            "example: impl " + trait + " for Person { ... }");
    return false;
  }
  ImplBlock ib;
  ib.trait = std::move(trait);
  ib.traitLoc = traitLoc;
  ib.target = std::move(*target);
  ib.loc = loc;
  MethodOut mo{&ib.methods, false};
  MethodOut *saved = methodOut_;
  methodOut_ = &mo;
  while (!check(TokenKind::RBrace) && !atEnd()) {
    if (!check(TokenKind::Fn)) {
      errorAt(peek().loc, "expected method definition",
              "example: fn print(self) { ... }");
      synchronizeTrait();
    } else if (!parseFuncDecl()) {
      synchronizeTrait();
    }
    if (check(TokenKind::RBrace) || atEnd())
      break;
  }
  methodOut_ = saved;
  if (!match(TokenKind::RBrace)) {
    errorAt(peek().loc, "expected '}' to close impl block");
    return false;
  }
  result_.program.impls.push_back(std::move(ib));
  return result_.errors.empty();
}

bool Parser::parseBlock(Block &out) {
  DepthGuard depth(this, peek().loc, "block too deeply nested");
  if (depth.exceeded)
    return false;
  if (!match(TokenKind::LBrace)) {
    errorAt(peek().loc, "expected '{' to begin function body");
    return false;
  }
  while (!check(TokenKind::RBrace) && !atEnd()) {
    if (!parseStmt(out)) {
      synchronize();
      if (check(TokenKind::RBrace) || atEnd())
        break;
    }
  }
  if (!match(TokenKind::RBrace)) {
    errorAt(peek().loc, "expected '}' to close function body");
    return false;
  }
  return true;
}

bool Parser::parseStmt(Block &out) {
  if (check(TokenKind::Let))
    return parseLetStmt(out);
  if (check(TokenKind::Ident) && peek().lexeme == "print")
    return parsePrintStmt(out);
  if (check(TokenKind::Return))
    return parseReturnStmt(out);
  if (check(TokenKind::If))
    return parseIfStmt(out);
  if (check(TokenKind::While))
    return parseWhileStmt(out);
  if (check(TokenKind::Match))
    return parseMatchStmt(out);
  // Assignment: `name = expr;` (variabel harus sudah dideklarasikan —
  // dicek di semantic; `print` dikecualikan di atas).
  if (check(TokenKind::Ident) && peek().lexeme != "print" &&
      peekNext().kind == TokenKind::Assign) {
    Location loc = peek().loc;
    std::string name = peek().lexeme;
    match(TokenKind::Ident);
    match(TokenKind::Assign);
    auto value = parseExpr();
    if (!value)
      return false;
    consumeOptionalSemi();
    AssignStmt s;
    s.name = std::move(name);
    s.value = std::move(value);
    s.loc = loc;
    out.stmts.push_back(std::move(s));
    return true;
  }
  // Panggilan fungsi sebagai statement: `name(...)` (mis. fungsi void).
  // `print` dikecualikan (statement khusus). Selain call → bukan statement.
  // Panggilan method `p.m(...)` (Phase 22) juga statement ekspresi.
  if (check(TokenKind::Ident) && peek().lexeme != "print" &&
      (peekNext().kind == TokenKind::LParen || isMethodCallStart())) {
    Location loc = peek().loc;
    auto expr = parseExpr();
    if (!expr)
      return false;
    consumeOptionalSemi();
    ExprStmt s;
    s.expr = std::move(expr);
    s.loc = loc;
    out.stmts.push_back(std::move(s));
    return true;
  }
  errorAt(peek().loc, "expected statement",
          "example: let x = 1;  print(x);  return x;  if c { ... }  while c { ... }  match v { ... }  x = 1;  f(x);");
  return false;
}

bool Parser::isMethodCallStart() {
  // `Ident (. Ident)* . Ident (` — rantai field diakhiri panggilan.
  if (!check(TokenKind::Ident))
    return false;
  // Konstruktor enum eksplisit `E<T>.V(` (Phase 26) di posisi statement.
  if (peekAt(1).kind == TokenKind::Lt) {
    size_t j = 2;
    int depth = 1;
    while (true) {
      TokenKind k = peekAt(j).kind;
      if (k == TokenKind::Lt) {
        depth++;
      } else if (k == TokenKind::Gt) {
        depth--;
        if (depth == 0)
          break;
      } else if (k == TokenKind::Eof || k == TokenKind::Semi ||
                 k == TokenKind::LBrace || k == TokenKind::RBrace ||
                 k == TokenKind::Assign || k == TokenKind::Question ||
                 k == TokenKind::MatchArrow) {
        return false;
      }
      j++;
    }
    return peekAt(j + 1).kind == TokenKind::Dot &&
           peekAt(j + 2).kind == TokenKind::Ident &&
           peekAt(j + 3).kind == TokenKind::LParen;
  }
  size_t k = 1;
  while (peekAt(k).kind == TokenKind::Dot &&
         peekAt(k + 1).kind == TokenKind::Ident) {
    if (peekAt(k + 2).kind == TokenKind::LParen)
      return true;
    k += 2;
  }
  return false;
}

bool Parser::parseIfStmt(Block &out) {
  Location ifLoc = peek().loc;
  match(TokenKind::If);
  auto cond = parseExpr();
  if (!cond)
    return false;
  auto thenBlock = std::make_unique<Block>();
  if (!parseBlock(*thenBlock))
    return false;
  std::unique_ptr<Block> elseBlock;
  if (match(TokenKind::Else)) {
    // `else if ...` menempel sebagai Block satu-statement (dangling-else
    // terikat ke `if` terdekat secara alami via rekursi).
    if (check(TokenKind::If)) {
      auto chain = std::make_unique<Block>();
      if (!parseIfStmt(*chain))
        return false;
      elseBlock = std::move(chain);
    } else {
      auto eb = std::make_unique<Block>();
      if (!parseBlock(*eb))
        return false;
      elseBlock = std::move(eb);
    }
  }
  IfStmt s;
  s.cond = std::move(cond);
  s.thenBlock = std::move(thenBlock);
  s.elseBlock = std::move(elseBlock);
  s.loc = ifLoc;
  out.stmts.push_back(std::move(s));
  return true;
}

bool Parser::parseWhileStmt(Block &out) {
  Location whileLoc = peek().loc;
  match(TokenKind::While);
  auto cond = parseExpr();
  if (!cond)
    return false;
  auto body = std::make_unique<Block>();
  if (!parseBlock(*body))
    return false;
  WhileStmt s;
  s.cond = std::move(cond);
  s.body = std::move(body);
  s.loc = whileLoc;
  out.stmts.push_back(std::move(s));
  return true;
}

namespace {
// Lokasi pola untuk diagnostik (cermin Expr::loc).
const Location &patLoc(const Pat &p) {
  struct Visitor {
    const Location &operator()(const Pat::Wild &n) const { return n.loc; }
    const Location &operator()(const Pat::Bind &n) const { return n.loc; }
    const Location &operator()(const Pat::LitInt &n) const { return n.loc; }
    const Location &operator()(const Pat::LitBool &n) const { return n.loc; }
    const Location &operator()(const Pat::LitStr &n) const { return n.loc; }
    const Location &operator()(const Pat::Struct &n) const { return n.loc; }
    const Location &operator()(const Pat::Variant &n) const { return n.loc; }
  };
  return std::visit(Visitor{}, p.node);
}
} // namespace

std::unique_ptr<Pat> Parser::parsePattern() {
  auto out = std::make_unique<Pat>();
  // Wildcard `_` (juga untuk field bernama `_`).
  if (check(TokenKind::Ident) && peek().lexeme == "_") {
    Location loc = peek().loc;
    match(TokenKind::Ident);
    Pat::Wild w;
    w.loc = loc;
    out->node = std::move(w);
    return out;
  }
  // Literal negatif (`-1`).
  bool neg = false;
  if (check(TokenKind::Minus)) {
    neg = true;
    match(TokenKind::Minus);
  }
  if (check(TokenKind::IntLit)) {
    int64_t value = std::strtoll(peek().lexeme.c_str(), nullptr, 10);
    Location loc = peek().loc;
    match(TokenKind::IntLit);
    Pat::LitInt li;
    li.value = neg ? -value : value;
    li.loc = loc;
    out->node = std::move(li);
    return out;
  }
  if (neg) {
    errorAt(peek().loc, "expected integer literal after '-' in pattern",
            "example: -1");
    return nullptr;
  }
  if (check(TokenKind::StringLit)) {
    Pat::LitStr ls;
    ls.value = peek().lexeme;
    ls.loc = peek().loc;
    match(TokenKind::StringLit);
    out->node = std::move(ls);
    return out;
  }
  if (check(TokenKind::True) || check(TokenKind::False)) {
    Pat::LitBool lb;
    lb.value = check(TokenKind::True);
    lb.loc = peek().loc;
    match(peek().kind);
    out->node = std::move(lb);
    return out;
  }
  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected pattern",
            "example: 0, _, name, Point { x }, Some(v)");
    return nullptr;
  }
  std::string name = peek().lexeme;
  Location loc = peek().loc;
  match(TokenKind::Ident);
  // Varian nullary std (`None`) tanpa kurung — sebelum Bind, agar tak
  // disalahartikan sebagai binding. `Some`/`Ok`/`Err` telanjang juga
  // menjadi Variant (arity ditolak checker dengan pesan jelas).
  if ((name == "None" || name == "Some" || name == "Ok" || name == "Err") &&
      !check(TokenKind::LParen) && !check(TokenKind::LBrace) &&
      !check(TokenKind::Lt)) {
    Pat::Variant v;
    v.name = std::move(name);
    v.loc = loc;
    out->node = std::move(v);
    return out;
  }
  // Varian `Name(p, ...)` / `None` (Phase 25; resolusi type-directed
  // di checker — `Some`/`None`/`Ok`/`Err` untuk std Option/Result).
  if (check(TokenKind::LParen)) {
    match(TokenKind::LParen);
    Pat::Variant v;
    v.name = std::move(name);
    v.loc = loc;
    if (!check(TokenKind::RParen)) {
      while (true) {
        auto sub = parsePattern();
        if (!sub)
          return nullptr;
        v.args.push_back(std::move(sub));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
    }
    if (!match(TokenKind::RParen)) {
      errorAt(peek().loc, "expected ')' after variant pattern arguments",
              "example: Some(v)");
      return nullptr;
    }
    out->node = std::move(v);
    return out;
  }
  // Struct `Name { f, g: p }` (+ targs eksplisit opsional).
  if (check(TokenKind::Lt) || check(TokenKind::LBrace)) {
    Pat::Struct s;
    s.name = std::move(name);
    s.loc = loc;
    if (match(TokenKind::Lt)) {
      if (check(TokenKind::Gt)) {
        errorAt(peek().loc, "expected type argument after '<'",
                "example: Point<i64> { x }");
        return nullptr;
      }
      while (true) {
        auto ta = parseType();
        if (!ta)
          return nullptr;
        s.targs.push_back(std::move(*ta));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
      if (!match(TokenKind::Gt)) {
        errorAt(peek().loc, "expected '>' after type arguments",
                "example: Point<i64> { x }");
        return nullptr;
      }
    }
    if (!match(TokenKind::LBrace)) {
      errorAt(peek().loc, "expected '{' after struct pattern name",
              "example: Point { x, y }");
      return nullptr;
    }
    if (!check(TokenKind::RBrace)) {
      while (true) {
        if (!check(TokenKind::Ident)) {
          errorAt(peek().loc, "expected field name in struct pattern",
                  "example: Point { x, y: 0 }");
          return nullptr;
        }
        Pat::Struct::Field fi;
        fi.name = peek().lexeme;
        fi.loc = peek().loc;
        match(TokenKind::Ident);
        if (match(TokenKind::Colon)) {
          auto sub = parsePattern();
          if (!sub)
            return nullptr;
          fi.pat = std::move(sub);
        } else {
          // Shorthand `x` = bind; `_` = wildcard.
          auto sub = std::make_unique<Pat>();
          if (fi.name == "_") {
            Pat::Wild w;
            w.loc = fi.loc;
            sub->node = std::move(w);
          } else {
            Pat::Bind b;
            b.name = fi.name;
            b.loc = fi.loc;
            sub->node = std::move(b);
          }
          fi.pat = std::move(sub);
        }
        s.fields.push_back(std::move(fi));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
    }
    if (!match(TokenKind::RBrace)) {
      errorAt(peek().loc, "expected '}' to close struct pattern",
              "example: Point { x, y }");
      return nullptr;
    }
    out->node = std::move(s);
    return out;
  }
  // Binding telanjang (menangkap nilai apa pun).
  Pat::Bind b;
  b.name = std::move(name);
  b.loc = loc;
  out->node = std::move(b);
  return out;
}

bool Parser::parseMatchStmt(Block &out) {
  Location matchLoc = peek().loc;
  match(TokenKind::Match);
  auto subject = parseExpr();
  if (!subject)
    return false;
  if (!match(TokenKind::LBrace)) {
    errorAt(peek().loc, "expected '{' after match subject",
            "example: match x { 0 => print(\"zero\"), _ => print(\"other\") }");
    return false;
  }
  MatchStmt ms;
  ms.subject = std::move(subject);
  ms.loc = matchLoc;
  if (check(TokenKind::RBrace)) {
    errorAt(peek().loc, "match needs at least one arm",
            "example: match x { _ => print(x) }");
    return false;
  }
  while (!check(TokenKind::RBrace) && !atEnd()) {
    auto pat = parsePattern();
    if (!pat)
      return false;
    // Guard opsional: `<pola> if <kondisi> => ...`.
    std::unique_ptr<Expr> guard;
    if (check(TokenKind::If)) {
      match(TokenKind::If);
      guard = parseExpr();
      if (!guard)
        return false;
    }
    if (!match(TokenKind::MatchArrow)) {
      errorAt(peek().loc, "expected '=>' after match pattern",
              "example: 0 => print(\"zero\")");
      return false;
    }
    MatchArm arm;
    arm.pat = std::move(*pat);
    arm.guard = std::move(guard);
    arm.loc = patLoc(arm.pat);
    arm.body = std::make_unique<Block>();
    if (check(TokenKind::LBrace)) {
      if (!parseBlock(*arm.body))
        return false;
      if (!match(TokenKind::Comma) && !check(TokenKind::RBrace) &&
          !atEnd()) {
        errorAt(peek().loc, "expected ',' between match arms",
                "example: 0 => { print(\"zero\"); }, _ => { print(\"other\"); }");
        return false;
      }
    } else {
      // Single-statement dinormalisasi ke Block (scope arm seragam).
      Block single;
      if (!parseStmt(single))
        return false;
      if (!match(TokenKind::Comma) && !check(TokenKind::RBrace) &&
          !atEnd()) {
        errorAt(peek().loc, "expected ',' after match arm",
                "example: 0 => print(\"zero\"), _ => print(\"other\")");
        return false;
      }
      arm.body = std::make_unique<Block>(std::move(single));
    }
    ms.arms.push_back(std::move(arm));
  }
  if (!match(TokenKind::RBrace)) {
    errorAt(peek().loc, "expected '}' to close match");
    return false;
  }
  out.stmts.push_back(std::move(ms));
  return true;
}

bool Parser::parseLetStmt(Block &out) {
  Location letLoc = peek().loc;
  match(TokenKind::Let);

  if (!check(TokenKind::Ident)) {
    errorAt(peek().loc, "expected variable name after 'let'",
            "example: let x = 10 + 20 * 3;");
    return false;
  }
  std::string name = peek().lexeme;
  match(TokenKind::Ident);

  // Anotasi tipe opsional (v0.4.0): `let x: i64 = ...`.
  std::optional<Type> declaredType;
  if (match(TokenKind::Colon)) {
    auto ty = parseType();
    if (!ty)
      return false;
    declaredType = *ty;
  }

  if (!match(TokenKind::Assign)) {
    std::string found = atEnd() ? "end of file" : tokenKindName(peek().kind);
    errorAt(peek().loc,
            "expected '=' after variable name in 'let' (found " + found + ")",
            "example: let " + name + " = 10;");
    return false;
  }

  auto value = parseExpr();
  if (!value)
    return false;
  consumeOptionalSemi();

  LetStmt s;
  s.name = std::move(name);
  s.loc = letLoc;
  s.declaredType = std::move(declaredType);
  s.value = std::move(value);
  out.stmts.push_back(std::move(s));
  return true;
}

std::unique_ptr<Type> Parser::parseType() {
  DepthGuard depth(this, peek().loc, "type too deeply nested");
  if (depth.exceeded)
    return nullptr;
  // Tipe array `[T]` (v0.7.0).
  if (check(TokenKind::LBracket)) {
    match(TokenKind::LBracket);
    auto elem = parseType();
    if (!elem)
      return nullptr;
    if (!match(TokenKind::RBracket)) {
      errorAt(peek().loc, "expected ']' after array element type",
              "example: [i64]");
      return nullptr;
    }
    return std::make_unique<Type>(Type::array(*elem));
  }
  // Tipe fungsi `fn(T, ...) -> T` (Phase 21b, v1.2.0) — hanya di posisi
  // tipe (`fn` + `(` tak ambigu dengan deklarasi `fn nama(`).
  if (check(TokenKind::Fn)) {
    match(TokenKind::Fn);
    if (!match(TokenKind::LParen)) {
      errorAt(peek().loc, "expected '(' after 'fn' in function type",
              "example: fn(i64) -> i64");
      return nullptr;
    }
    std::vector<Type> params;
    if (!check(TokenKind::RParen)) {
      while (true) {
        auto pt = parseType();
        if (!pt)
          return nullptr;
        params.push_back(std::move(*pt));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
    }
    if (!match(TokenKind::RParen)) {
      errorAt(peek().loc, "expected ')' after function type parameters",
              "example: fn(i64, bool) -> i64");
      return nullptr;
    }
    Type ret = Type::void_();
    if (match(TokenKind::Arrow)) {
      auto rt = parseType();
      if (!rt)
        return nullptr;
      ret = std::move(*rt);
    }
    return std::make_unique<Type>(Type::func(std::move(params), ret));
  }
  Type ty = Type::error();
  if (check(TokenKind::I64)) ty = Type::i64();
  else if (check(TokenKind::Bool)) ty = Type::boolean();
  else if (check(TokenKind::String)) ty = Type::string();
  else if (check(TokenKind::Void)) ty = Type::void_();
  else if (check(TokenKind::Ident)) {
    // Tipe bernama (struct, v0.8.0) — keberadaannya dicek typechecker.
    // Aplikasi generik (Phase 21d): `Point<i64>` (rekursif, nested OK).
    std::string tname = peek().lexeme;
    match(TokenKind::Ident);
    if (check(TokenKind::Lt)) {
      match(TokenKind::Lt);
      std::vector<Type> targs;
      if (check(TokenKind::Gt)) {
        errorAt(peek().loc, "expected type argument after '<'",
                "example: " + tname + "<i64>");
        return nullptr;
      }
      while (true) {
        auto ta = parseType();
        if (!ta)
          return nullptr;
        targs.push_back(std::move(*ta));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
      if (!match(TokenKind::Gt)) {
        errorAt(peek().loc, "expected '>' after type arguments",
                "example: " + tname + "<i64>");
        return nullptr;
      }
      return std::make_unique<Type>(Type::applied(tname, std::move(targs)));
    }
    return std::make_unique<Type>(Type::named(tname));
  }
  else {
  errorAt(peek().loc, "expected type after ':'",
          "type: i64, bool, string, void, or [T]");
    return nullptr;
  }
  match(peek().kind);
  return std::make_unique<Type>(ty);
}

bool Parser::parseReturnStmt(Block &out) {
  Location retLoc = peek().loc;
  match(TokenKind::Return);
  ReturnStmt s;
  s.loc = retLoc;
  // `return;` / `return}` / EOF → bare return (fungsi void).
  if (!check(TokenKind::Semi) && !check(TokenKind::RBrace) && !atEnd()) {
    s.value = parseExpr();
    if (!s.value)
      return false;
  }
  consumeOptionalSemi();
  out.stmts.push_back(std::move(s));
  return true;
}

bool Parser::parsePrintStmt(Block &out) {
  Location printLoc = peek().loc;
  match(TokenKind::Ident); // `print`

  if (!match(TokenKind::LParen)) {
    errorAt(peek().loc, "expected '(' after 'print'");
    return false;
  }
  auto arg = parseExpr();
  if (!arg)
    return false;
  if (!match(TokenKind::RParen)) {
    errorAt(peek().loc, "expected ')' after print argument");
    return false;
  }
  consumeOptionalSemi();

  PrintStmt s;
  s.arg = std::move(arg);
  s.loc = printLoc;
  out.stmts.push_back(std::move(s));
  return true;
}

// ---- Expressions (precedence climbing) ----

std::unique_ptr<Expr> Parser::parseExpr() {
  DepthGuard depth(this, peek().loc, "expression too deeply nested");
  if (depth.exceeded)
    return nullptr;
  return parseEquality();
}

static std::unique_ptr<Expr> makeBinary(BinOp op, std::unique_ptr<Expr> lhs,
                                        std::unique_ptr<Expr> rhs,
                                        Location loc) {
  auto e = std::make_unique<Expr>();
  Expr::Binary b;
  b.op = op;
  b.lhs = std::move(lhs);
  b.rhs = std::move(rhs);
  b.loc = loc;
  e->node = std::move(b);
  return e;
}

std::unique_ptr<Expr> Parser::parseEquality() {
  auto lhs = parseComparison();
  if (!lhs)
    return nullptr;
  while (check(TokenKind::Eq) || check(TokenKind::NotEq)) {
    BinOp op = check(TokenKind::Eq) ? BinOp::Eq : BinOp::NotEq;
    Location loc = peek().loc;
    match(peek().kind);
    auto rhs = parseComparison();
    if (!rhs)
      return nullptr;
    lhs = makeBinary(op, std::move(lhs), std::move(rhs), loc);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::parseComparison() {
  auto lhs = parseTerm();
  if (!lhs)
    return nullptr;
  while (check(TokenKind::Lt) || check(TokenKind::Gt) ||
         check(TokenKind::LtEq) || check(TokenKind::GtEq)) {
    BinOp op = BinOp::Lt;
    if (check(TokenKind::Gt)) op = BinOp::Gt;
    else if (check(TokenKind::LtEq)) op = BinOp::LtEq;
    else if (check(TokenKind::GtEq)) op = BinOp::GtEq;
    Location loc = peek().loc;
    match(peek().kind);
    auto rhs = parseTerm();
    if (!rhs)
      return nullptr;
    lhs = makeBinary(op, std::move(lhs), std::move(rhs), loc);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::parseTerm() {
  auto lhs = parseFactor();
  if (!lhs)
    return nullptr;
  while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
    BinOp op = check(TokenKind::Plus) ? BinOp::Add : BinOp::Sub;
    Location loc = peek().loc;
    match(peek().kind);
    auto rhs = parseFactor();
    if (!rhs)
      return nullptr;
    lhs = makeBinary(op, std::move(lhs), std::move(rhs), loc);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::parseFactor() {
  auto lhs = parseUnary();
  if (!lhs)
    return nullptr;
  while (check(TokenKind::Star) || check(TokenKind::Slash)) {
    BinOp op = check(TokenKind::Star) ? BinOp::Mul : BinOp::Div;
    Location loc = peek().loc;
    match(peek().kind);
    auto rhs = parseUnary();
    if (!rhs)
      return nullptr;
    lhs = makeBinary(op, std::move(lhs), std::move(rhs), loc);
  }
  return lhs;
}

std::unique_ptr<Expr> Parser::parseUnary() {
  // Rekursi langsung (`---x`); tidak melalui parseExpr.
  DepthGuard depth(this, peek().loc, "expression too deeply nested");
  if (depth.exceeded)
    return nullptr;
  if (check(TokenKind::Minus) || check(TokenKind::Plus)) {
    UnOp op = check(TokenKind::Minus) ? UnOp::Neg : UnOp::Pos;
    Location loc = peek().loc;
    match(peek().kind);
    auto operand = parseUnary();
    if (!operand)
      return nullptr;
    auto e = std::make_unique<Expr>();
    Expr::Unary u;
    u.op = op;
    u.operand = std::move(operand);
    u.loc = loc;
    e->node = std::move(u);
    return e;
  }
  return parsePrimary();
}

std::unique_ptr<Expr> Parser::parsePrimary() {
  auto base = parseAtom();
  if (!base)
    return nullptr;
  // Postfix berantai: indexing `a[0]`, field `p.age` (v0.8.0),
  // method call `p.m()` (Phase 22), propagasi `r?` (Phase 24).
  // Kombinasi bebas: `a.b[0]`, `a[0].b`, `f()?.x`, `a.b()?.c`.
  while (true) {
    // Konstruktor enum eksplisit `E<T>.V(args)` (Phase 26): receiver
    // Ident + `<...>` seimbang + `.` + `Ident` + `(`. Tanpa cabang ini
    // `<` dibaca perbandingan; bentuk ini tak ambigu dengan kode valid
    // (`E < T > . V (` selalu gagal parse sebagai perbandingan).
    if (check(TokenKind::Lt) &&
        std::get_if<Expr::VarRef>(&base->node) != nullptr) {
      // Pindai dari sesudah `<` (depth=1 menghitungnya).
      size_t j = 1;
      int depth = 1;
      bool sawGt = false;
      while (true) {
        TokenKind k = peekAt(j).kind;
        if (k == TokenKind::Lt) {
          depth++;
        } else if (k == TokenKind::Gt) {
          depth--;
          if (depth == 0) {
            sawGt = true;
            break;
          }
        } else if (k == TokenKind::Eof || k == TokenKind::Semi ||
                   k == TokenKind::LBrace || k == TokenKind::RBrace ||
                   k == TokenKind::Assign || k == TokenKind::Question ||
                   k == TokenKind::MatchArrow) {
          break;
        }
        j++;
      }
      if (sawGt && peekAt(j + 1).kind == TokenKind::Dot &&
          peekAt(j + 2).kind == TokenKind::Ident) {
        match(TokenKind::Lt);
        std::vector<Type> targs;
        while (true) {
          auto ty = parseType();
          if (!ty)
            return nullptr;
          targs.push_back(*ty);
          if (match(TokenKind::Comma))
            continue;
          break;
        }
        if (!match(TokenKind::Gt)) {
          errorAt(peek().loc, "expected '>' after type arguments",
                  "example: Wrap<i64>.Val(1)");
          return nullptr;
        }
        match(TokenKind::Dot);
        std::string field = peek().lexeme;
        Location mloc = peek().loc;
        match(TokenKind::Ident);
        auto e = std::make_unique<Expr>();
        Expr::MethodCall mc;
        mc.loc = mloc;
        mc.method = std::move(field);
        mc.targs = std::move(targs);
        mc.receiver = std::move(base);
        // Tanpa `(` = varian unit (`E<T>.V`); arity divalidasi checker.
        if (match(TokenKind::LParen)) {
          if (!check(TokenKind::RParen)) {
            while (true) {
              auto arg = parseExpr();
              if (!arg)
                return nullptr;
              mc.args.push_back(std::move(arg));
              if (match(TokenKind::Comma))
                continue;
              break;
            }
          }
          if (!match(TokenKind::RParen)) {
            errorAt(peek().loc, "expected ')' after variant arguments",
                    "example: Wrap<i64>.Val(1)");
            return nullptr;
          }
        }
        e->node = std::move(mc);
        base = std::move(e);
        continue;
      }
    }
    if (match(TokenKind::Question)) {
      Location loc = previous().loc;
      auto e = std::make_unique<Expr>();
      Expr::Propagate p;
      p.expr = std::move(base);
      p.loc = loc;
      e->node = std::move(p);
      base = std::move(e);
      continue;
    }
    if (match(TokenKind::LBracket)) {
      Location loc = previous().loc;
      auto index = parseExpr();
      if (!index)
        return nullptr;
      if (!match(TokenKind::RBracket)) {
        errorAt(peek().loc, "expected ']' after index",
                "example: numbers[0]");
        return nullptr;
      }
      auto e = std::make_unique<Expr>();
      Expr::Index idx;
      idx.base = std::move(base);
      idx.index = std::move(index);
      idx.loc = loc;
      e->node = std::move(idx);
      base = std::move(e);
    } else if (match(TokenKind::Dot)) {
      Location loc = previous().loc;
      if (!check(TokenKind::Ident)) {
        errorAt(peek().loc, "expected field name after '.'",
                "example: person.age");
        return nullptr;
      }
      std::string field = peek().lexeme;
      match(TokenKind::Ident);
      // Panggilan method (Phase 22): `recv.method(args)`.
      if (check(TokenKind::LParen)) {
        match(TokenKind::LParen);
        auto e = std::make_unique<Expr>();
        Expr::MethodCall mc;
        mc.loc = loc;
        mc.method = std::move(field);
        if (!check(TokenKind::RParen)) {
          while (true) {
            auto arg = parseExpr();
            if (!arg)
              return nullptr;
            mc.args.push_back(std::move(arg));
            if (match(TokenKind::Comma))
              continue;
            break;
          }
        }
        if (!match(TokenKind::RParen)) {
          errorAt(peek().loc, "expected ')' after method arguments",
                  "example: person.describe()");
          return nullptr;
        }
        // Base dipindah terakhir (receiver sudah utuh).
        mc.receiver = std::move(base);
        e->node = std::move(mc);
        base = std::move(e);
        continue;
      }
      auto e = std::make_unique<Expr>();
      Expr::FieldAccess fa;
      fa.base = std::move(base);
      fa.field = std::move(field);
      fa.loc = loc;
      e->node = std::move(fa);
      base = std::move(e);
    } else {
      break;
    }
  }
  return base;
}

std::unique_ptr<Expr> Parser::parseAtom() {
  // Lambda `|x: i64| x + 1` (Phase 27): parameter opsional-anotasi,
  // body blok `{ ... }` atau ekspresi tunggal.
  if (check(TokenKind::Pipe)) {
    Location loc = peek().loc;
    match(TokenKind::Pipe);
    auto e = std::make_unique<Expr>();
    Expr::Lambda lam;
    lam.loc = loc;
    if (!check(TokenKind::Pipe)) {
      while (true) {
        if (!check(TokenKind::Ident)) {
          errorAt(peek().loc, "expected parameter name in lambda",
                  "example: |x: i64| x + 1");
          return nullptr;
        }
        Expr::Lambda::LParam p;
        p.name = peek().lexeme;
        p.loc = peek().loc;
        match(TokenKind::Ident);
        if (match(TokenKind::Colon)) {
          auto ty = parseType();
          if (!ty)
            return nullptr;
          p.annot = *ty;
        }
        lam.params.push_back(std::move(p));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
    }
    if (!match(TokenKind::Pipe)) {
      errorAt(peek().loc, "expected '|' after lambda parameters",
              "example: |x: i64| x + 1");
      return nullptr;
    }
    lam.body = std::make_unique<Block>();
    if (check(TokenKind::LBrace)) {
      if (!parseBlock(*lam.body))
        return nullptr;
    } else {
      // Ekspresi tunggal dinormalisasi ke Block `return <expr>;`
      // agar checker/codegen seragam dengan body fungsi.
      auto bexpr = parseExpr();
      if (!bexpr)
        return nullptr;
      ReturnStmt rs;
      rs.value = std::move(bexpr);
      rs.loc = rs.value->loc();
      lam.body->stmts.push_back(Stmt{std::move(rs)});
    }
    e->node = std::move(lam);
    return e;
  }
  // Receiver `self` (Phase 22): VarRef, atau Call bila diikuti `(`.
  if (check(TokenKind::Self)) {
    Location loc = peek().loc;
    match(TokenKind::Self);
    if (check(TokenKind::LParen)) {
      match(TokenKind::LParen);
      Expr::Call call;
      call.callee = "self";
      call.loc = loc;
      if (!check(TokenKind::RParen)) {
        while (true) {
          auto arg = parseExpr();
          if (!arg)
            return nullptr;
          call.args.push_back(std::move(arg));
          if (match(TokenKind::Comma))
            continue;
          break;
        }
      }
      if (!match(TokenKind::RParen)) {
        errorAt(peek().loc, "expected ')' after call arguments",
                "example: add(10, 20)");
        return nullptr;
      }
      auto e = std::make_unique<Expr>();
      e->node = std::move(call);
      return e;
    }
    auto e = std::make_unique<Expr>();
    e->node = Expr::VarRef{"self", loc};
    return e;
  }
  if (check(TokenKind::IntLit)) {
    Location loc = peek().loc;
    // Lexer sudah menjamin digit desimal dalam rentang i64.
    int64_t value = std::strtoll(peek().lexeme.c_str(), nullptr, 10);
    match(TokenKind::IntLit);
    auto e = std::make_unique<Expr>();
    e->node = Expr::LiteralInt{value, loc};
    return e;
  }
  if (check(TokenKind::StringLit)) {
    Location loc = peek().loc;
    std::string value = peek().lexeme;
    match(TokenKind::StringLit);
    auto e = std::make_unique<Expr>();
    e->node = Expr::LiteralString{std::move(value), loc};
    return e;
  }
  if (check(TokenKind::True) || check(TokenKind::False)) {
    bool value = check(TokenKind::True);
    Location loc = peek().loc;
    match(peek().kind);
    auto e = std::make_unique<Expr>();
    e->node = Expr::LiteralBool{value, loc};
    return e;
  }
  if (check(TokenKind::Ident)) {
    Location loc = peek().loc;
    std::string name = peek().lexeme;
    // `name(` → function call; `Name {` → struct literal (v0.8.0);
    // selain itu variable reference.
    //
    // Ambiguitas `if c {` vs literal: `{` milik block bila tidak diikuti
    // `}` atau `field :`. Putuskan via lookahead (tanpa backtracking).
    bool structLit = false;
    if (peekNext().kind == TokenKind::LBrace) {
      TokenKind afterBrace = peekAt(2).kind;
      structLit = (afterBrace == TokenKind::RBrace) ||
                  (afterBrace == TokenKind::Ident &&
                   peekAt(3).kind == TokenKind::Colon);
    }
    // Literal generik eksplisit (Phase 21d): `Point<i64> { ... }`.
    // Lookahead seimbang cari `>` + `{`; bila tak ada, jalur ekspresi
    // biasa (perbandingan) — mis. kondisi `if a<b {`.
    bool genericLit = false;
    if (!structLit && peekNext().kind == TokenKind::Lt) {
      int depth = 0;
      for (size_t k = 1;; k++) {
        TokenKind kk = peekAt(k).kind;
        if (kk == TokenKind::Eof || kk == TokenKind::Semi ||
            kk == TokenKind::RBrace)
          break;
        if (kk == TokenKind::Lt) {
          depth++;
        } else if (kk == TokenKind::Gt) {
          depth--;
          if (depth == 0) {
            genericLit = (peekAt(k + 1).kind == TokenKind::LBrace);
            break;
          }
        }
      }
    }
    if (peekNext().kind == TokenKind::LParen) {
      match(TokenKind::Ident);
      match(TokenKind::LParen);
      Expr::Call call;
      call.callee = std::move(name);
      call.loc = loc;
      if (!check(TokenKind::RParen)) {
        while (true) {
          auto arg = parseExpr();
          if (!arg)
            return nullptr;
          call.args.push_back(std::move(arg));
          if (match(TokenKind::Comma))
            continue;
          break;
        }
      }
      if (!match(TokenKind::RParen)) {
        errorAt(peek().loc, "expected ')' after call arguments",
                "example: add(10, 20)");
        return nullptr;
      }
      auto e = std::make_unique<Expr>();
      e->node = std::move(call);
      return e;
    }
    if (structLit || genericLit) {
      match(TokenKind::Ident);
      Expr::StructLit lit;
      if (genericLit) {
        match(TokenKind::Lt);
        while (true) {
          auto ta = parseType();
          if (!ta)
            return nullptr;
          lit.targs.push_back(std::move(*ta));
          if (match(TokenKind::Comma))
            continue;
          break;
        }
        if (!match(TokenKind::Gt)) {
          errorAt(peek().loc, "expected '>' after type arguments",
                  "example: Point<i64> { x: 1 }");
          return nullptr;
        }
      }
      match(TokenKind::LBrace);
      lit.name = std::move(name);
      lit.loc = loc;
      if (!check(TokenKind::RBrace)) {
        while (true) {
          if (!check(TokenKind::Ident)) {
            errorAt(peek().loc, "expected field name",
                    "example: Person { name: \"Nusa\", age: 1 }");
            return nullptr;
          }
          Expr::StructLit::FieldInit fi;
          fi.name = peek().lexeme;
          fi.loc = peek().loc;
          match(TokenKind::Ident);
          if (!match(TokenKind::Colon)) {
            errorAt(peek().loc,
                    "expected ':' after field '" + fi.name + "'",
                    "example: " + fi.name + ": 1");
            return nullptr;
          }
          fi.value = parseExpr();
          if (!fi.value)
            return nullptr;
          lit.fields.push_back(std::move(fi));
          if (match(TokenKind::Comma))
            continue;
          break;
        }
      }
      if (!match(TokenKind::RBrace)) {
        errorAt(peek().loc, "expected '}' after struct literal",
                "example: Person { name: \"Nusa\", age: 1 }");
        return nullptr;
      }
      auto e = std::make_unique<Expr>();
      e->node = std::move(lit);
      return e;
    }
    match(TokenKind::Ident);
    auto e = std::make_unique<Expr>();
    e->node = Expr::VarRef{std::move(name), loc};
    return e;
  }
  // Array literal `[1, 2, 3]` (v0.7.0; boleh kosong bila beranotasi).
  if (check(TokenKind::LBracket)) {
    Location loc = peek().loc;
    match(TokenKind::LBracket);
    Expr::ArrayLit lit;
    lit.loc = loc;
    if (!check(TokenKind::RBracket)) {
      while (true) {
        auto el = parseExpr();
        if (!el)
          return nullptr;
        lit.elems.push_back(std::move(el));
        if (match(TokenKind::Comma))
          continue;
        break;
      }
    }
    if (!match(TokenKind::RBracket)) {
      errorAt(peek().loc, "expected ']' after array literal",
              "example: [1, 2, 3, 4]");
      return nullptr;
    }
    auto e = std::make_unique<Expr>();
    e->node = std::move(lit);
    return e;
  }
  if (match(TokenKind::LParen)) {
    auto inner = parseExpr();
    if (!inner)
      return nullptr;
    if (!match(TokenKind::RParen)) {
      errorAt(peek().loc, "expected ')' after expression");
      return nullptr;
    }
    return inner;
  }
  errorAt(peek().loc, "expected expression",
          "expression: number, string, true/false, function call, [1, 2], "
          "variable name, or '(...)'");
  return nullptr;
}

} // namespace nusa
