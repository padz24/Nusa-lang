#pragma once
#include "compiler/lexer/Token.h"
#include <string>
#include <vector>

namespace nusa {

// Lexer v0.2.0 — front-end penuh tahap awal.
// Mendukung:
//   keywords: fn let return if else while true false
//   identifier, integer literal (desimal, i64), string literal
//   operator: + - * / = == != < > <= >=
//   simbol: ( ) { } [ ] : , ;
//   source location (file/line/column) di setiap token.
// Whitespace + komentar `//` dilewati.
// Komentar ditangkap untuk formatter (`fmt`) dan generator dokumen
// (`doc`) v0.9.0; parser mengabaikannya.
struct Comment {
  int line = 1;     // baris awal `//`
  std::string text; // isi setelah `//` (atau `///`), tanpa newline
  bool doc = false; // `///` (dokumentasi API)
};

class Lexer {
public:
  Lexer(std::string file, std::string source);

  std::vector<Token> tokenize();
  const std::vector<std::string> &errors() const { return errors_; }
  const std::vector<Comment> &comments() const { return comments_; }

  // Batas error anti-banjir (v1.1.0: file sampah mencetak ratusan error).
  // errorAt membuang error ke-(kMaxErrors+1) dst, lalu tepat satu ringkasan.
  static constexpr size_t kMaxErrors = 25;

private:
  char peek() const;
  char peekNext() const;
  char advance();
  bool atEnd() const;
  void skipWhitespaceAndComments();
  Token makeToken(TokenKind kind, const std::string &lexeme,
                  int line, int col);
  void errorAt(int line, int col, const std::string &msg);
  bool capped_ = false;
  Token lexString(int line, int col);
  Token lexIdentOrKeyword(int line, int col);
  Token lexNumber(int line, int col);
  static bool isAlpha(char c);
  static bool isAlnum(char c);
  static bool isDigit(char c);

  std::string file_;
  std::string src_;
  size_t pos_ = 0;
  int line_ = 1;
  int col_ = 1;
  std::vector<std::string> errors_;
  std::vector<Comment> comments_;
};

} // namespace nusa
