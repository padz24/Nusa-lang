#pragma once
#include <string>

namespace nusa {

struct Location {
  std::string file;
  int line = 1;
  int column = 1;
};

enum class TokenKind {
  // Keywords (v0.2.0)
  Fn,     // fn
  Let,    // let
  Return, // return
  If,     // if
  Else,   // else
  While,  // while
  True,   // true
  False,  // false
  // Type keywords (v0.4.0, reserved)
  I64,    // i64
  Bool,   // bool
  String, // string
  Void,   // void
  // Keywords v0.8.0 (reserved)
  Struct, // struct
  Import, // import
  // Keywords v1.2.0 (Phase 21a: type aliases; reserved)
  TypeAlias, // type
  // Keywords v1.3.0 (Phase 22: traits; reserved)
  Trait, // trait
  Impl,  // impl
  For,   // for (impl Trait for Type)
  Self,  // self (receiver; tipe `Self` ditunda)
  // Keywords v1.6.0 (Phase 25: pattern matching; reserved)
  Match, // match
  // Keywords v1.7.0 (Phase 26: enums; reserved)
  Enum, // enum
  // Keywords v1.13.0 (Phase 32: C FFI; reserved)
  Extern, // extern
  // Keywords v2.0 (reserved untuk RFC-0004 async; tanpa sintaks).
  Async, // async
  Await, // await
  // Keywords v2.20.0 (Phase 69: macro system; reserved, tanpa sintaks).
  Macro, // macro
  // Keywords v2.21.0 (Phase 70: const items; RFC-0009 slice 1).
  Const, // const
  // Literals & identifiers
  Ident,     // main, print, x, ...
  IntLit,    // 10, 20, 3, ...
  StringLit, // "..."
  // Operators
  Plus,  // +
  Minus, // -
  Star,  // *
  Slash, // /
  Assign, // =
  Eq,     // ==
  NotEq,  // !=
  Lt,     // <
  Gt,     // >
  LtEq,   // <=
  GtEq,   // >=
  Arrow,  // -> (v0.5.0: return type)
  MatchArrow, // => (v1.6.0: match arm)
  // Symbols
  LParen,   // (
  RParen,   // )
  LBrace,   // {
  RBrace,   // }
  LBracket, // [
  RBracket, // ]
  Colon,    // :
  Comma,    // ,
  Semi,     // ;
  Dot,      // . (v0.8.0: field access)
  Question, // ? (v1.5.0: propagasi Result)
  Pipe,     // | (v1.8.0: lambda `|x| ...`)
  Eof,
  Unknown,
};

std::string tokenKindName(TokenKind k);

struct Token {
  TokenKind kind = TokenKind::Unknown;
  // Untuk StringLit: nilai sudah di-unescape.
  // Untuk IntLit: teks digit mentah (konversi + cek overflow di parser).
  std::string lexeme;
  Location loc;
};

} // namespace nusa
