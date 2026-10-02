#include "compiler/lexer/Lexer.h"
#include <cstdint>
#include <limits>

namespace nusa {

std::string tokenKindName(TokenKind k) {
  switch (k) {
  case TokenKind::Fn: return "'fn'";
  case TokenKind::Let: return "'let'";
  case TokenKind::Return: return "'return'";
  case TokenKind::If: return "'if'";
  case TokenKind::Else: return "'else'";
  case TokenKind::While: return "'while'";
  case TokenKind::True: return "'true'";
  case TokenKind::False: return "'false'";
  case TokenKind::I64: return "'i64'";
  case TokenKind::Bool: return "'bool'";
  case TokenKind::String: return "'string'";
  case TokenKind::Void: return "'void'";
  case TokenKind::Struct: return "'struct'";
  case TokenKind::Import: return "'import'";
  case TokenKind::TypeAlias: return "'type'";
  case TokenKind::Trait: return "'trait'";
  case TokenKind::Impl: return "'impl'";
  case TokenKind::For: return "'for'";
  case TokenKind::Self: return "'self'";
  case TokenKind::Match: return "'match'";
  case TokenKind::Enum: return "'enum'";
  case TokenKind::Extern: return "'extern'";
  case TokenKind::Async: return "'async'";
  case TokenKind::Await: return "'await'";
  case TokenKind::Macro: return "'macro'";
  case TokenKind::Const: return "'const'";
  case TokenKind::Ident: return "identifier";
  case TokenKind::IntLit: return "integer literal";
  case TokenKind::StringLit: return "string literal";
  case TokenKind::Plus: return "'+'";
  case TokenKind::Minus: return "'-'";
  case TokenKind::Star: return "'*'";
  case TokenKind::Slash: return "'/'";
  case TokenKind::Assign: return "'='";
  case TokenKind::Eq: return "'=='";
  case TokenKind::NotEq: return "'!='";
  case TokenKind::Lt: return "'<'";
  case TokenKind::Gt: return "'>'";
  case TokenKind::LtEq: return "'<='";
  case TokenKind::GtEq: return "'>='";
  case TokenKind::Arrow: return "'->'";
  case TokenKind::MatchArrow: return "'=>'";
  case TokenKind::LParen: return "'('";
  case TokenKind::RParen: return "')'";
  case TokenKind::LBrace: return "'{'";
  case TokenKind::RBrace: return "'}'";
  case TokenKind::LBracket: return "'['";
  case TokenKind::RBracket: return "']'";
  case TokenKind::Colon: return "':'";
  case TokenKind::Comma: return "','";
  case TokenKind::Semi: return "';'";
  case TokenKind::Dot: return "'.'";
  case TokenKind::Question: return "'?'";
  case TokenKind::Pipe: return "'|'";
  case TokenKind::Eof: return "end of file";
  case TokenKind::Unknown: return "unknown";
  }
  return "unknown";
}

Lexer::Lexer(std::string file, std::string source)
    : file_(std::move(file)), src_(std::move(source)) {}

char Lexer::peek() const { return atEnd() ? '\0' : src_[pos_]; }
char Lexer::peekNext() const {
  return (pos_ + 1 < src_.size()) ? src_[pos_ + 1] : '\0';
}

char Lexer::advance() {
  char c = src_[pos_++];
  if (c == '\n') {
    line_++;
    col_ = 1;
  } else {
    col_++;
  }
  return c;
}

bool Lexer::atEnd() const { return pos_ >= src_.size(); }

bool Lexer::isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool Lexer::isAlnum(char c) { return isAlpha(c) || isDigit(c); }
bool Lexer::isDigit(char c) { return c >= '0' && c <= '9'; }

void Lexer::skipWhitespaceAndComments() {
  while (!atEnd()) {
    char c = peek();
    if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
      advance();
    } else if (c == '/' && peekNext() == '/') {
      // Tangkap komentar untuk fmt/doc (v0.9.0); parser tetap abaikan.
      int cline = line_;
      advance(); // '/'
      advance(); // '/'
      bool doc = false;
      if (peek() == '/') {
        doc = true;
        advance();
      }
      std::string text;
      while (!atEnd() && peek() != '\n')
        text.push_back(advance());
      comments_.push_back(Comment{cline, text, doc});
    } else {
      break;
    }
  }
}

Token Lexer::makeToken(TokenKind kind, const std::string &lexeme, int line,
                       int col) {
  Token t;
  t.kind = kind;
  t.lexeme = lexeme;
  t.loc = Location{file_, line, col};
  return t;
}

void Lexer::errorAt(int line, int col, const std::string &msg) {
  if (capped_)
    return;
  if (errors_.size() >= kMaxErrors) {
    capped_ = true;
    errors_.push_back("error: too many errors in '" + file_ + "', stopping");
    return;
  }
  errors_.push_back(file_ + ":" + std::to_string(line) + ":" +
                    std::to_string(col) + ": error: " + msg);
}

Token Lexer::lexString(int line, int col) {
  // peek() == '"' saat dipanggil; konsumsi quote pembuka
  advance();
  std::string value;
  while (!atEnd()) {
    char c = peek();
    if (c == '"') {
      advance(); // quote penutup
      return makeToken(TokenKind::StringLit, value, line, col);
    }
    if (c == '\n') {
      errorAt(line_, col_, "unterminated string literal");
      return makeToken(TokenKind::Unknown, value, line, col);
    }
    if (c == '\\') {
      advance(); // konsumsi backslash
      if (atEnd())
        break;
      char e = advance();
      switch (e) {
      case 'n': value.push_back('\n'); break;
      case 't': value.push_back('\t'); break;
      case 'r': value.push_back('\r'); break;
      case '"': value.push_back('"'); break;
      case '\\': value.push_back('\\'); break;
      case '0': value.push_back('\0'); break;
      default:
        // Escape tak dikenal: pertahankan char-nya apa adanya.
        value.push_back(e);
        break;
      }
    } else {
      value.push_back(advance());
    }
  }
  errorAt(line, col, "unterminated string literal");
  return makeToken(TokenKind::Unknown, value, line, col);
}

Token Lexer::lexIdentOrKeyword(int line, int col) {
  size_t start = pos_;
  while (!atEnd() && isAlnum(peek()))
    advance();
  std::string word = src_.substr(start, pos_ - start);
  TokenKind kind = TokenKind::Ident;
  if (word == "fn") kind = TokenKind::Fn;
  else if (word == "let") kind = TokenKind::Let;
  else if (word == "return") kind = TokenKind::Return;
  else if (word == "if") kind = TokenKind::If;
  else if (word == "else") kind = TokenKind::Else;
  else if (word == "while") kind = TokenKind::While;
  else if (word == "true") kind = TokenKind::True;
  else if (word == "false") kind = TokenKind::False;
  else if (word == "i64") kind = TokenKind::I64;
  else if (word == "bool") kind = TokenKind::Bool;
  else if (word == "string") kind = TokenKind::String;
  else if (word == "void") kind = TokenKind::Void;
  else if (word == "struct") kind = TokenKind::Struct;
  else if (word == "import") kind = TokenKind::Import;
  else if (word == "type") kind = TokenKind::TypeAlias;
  else if (word == "trait") kind = TokenKind::Trait;
  else if (word == "impl") kind = TokenKind::Impl;
  else if (word == "for") kind = TokenKind::For;
  else if (word == "self") kind = TokenKind::Self;
  else if (word == "match") kind = TokenKind::Match;
  else if (word == "enum") kind = TokenKind::Enum;
  else if (word == "extern") kind = TokenKind::Extern;
  else if (word == "async") kind = TokenKind::Async;
  else if (word == "await") kind = TokenKind::Await;
  else if (word == "macro") kind = TokenKind::Macro;
  else if (word == "const") kind = TokenKind::Const;
  return makeToken(kind, word, line, col);
}

Token Lexer::lexNumber(int line, int col) {
  size_t start = pos_;
  // Akumulasi dengan cek overflow i64 (literal non-negatif di level lexer;
  // tanda minus ditangani parser sebagai unary op).
  const uint64_t kMax = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  uint64_t value = 0;
  bool overflow = false;
  while (!atEnd() && isDigit(peek())) {
    unsigned digit = static_cast<unsigned>(advance() - '0');
    if (!overflow) {
      if (value > (kMax - digit) / 10) {
        overflow = true;
      } else {
        value = value * 10 + digit;
      }
    }
  }
  std::string text = src_.substr(start, pos_ - start);
  // `123abc` langsung menempel: tolak dengan pesan yang jelas.
  if (!atEnd() && (isAlpha(peek()) || peek() == '_')) {
    size_t tail = pos_;
    while (!atEnd() && isAlnum(peek()))
      advance();
    std::string bad = src_.substr(start, pos_ - start);
    errorAt(line, col, "invalid integer literal '" + bad + "'");
    return makeToken(TokenKind::Unknown, bad, line, col);
  }
  if (overflow) {
    errorAt(line, col,
            "integer literal '" + text + "' out of range (max 9223372036854775807)");
    return makeToken(TokenKind::Unknown, text, line, col);
  }
  return makeToken(TokenKind::IntLit, text, line, col);
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  while (true) {
    skipWhitespaceAndComments();
    if (atEnd())
      break;
    int line = line_;
    int col = col_;
    char c = peek();
    if (isAlpha(c)) {
      tokens.push_back(lexIdentOrKeyword(line, col));
    } else if (isDigit(c)) {
      tokens.push_back(lexNumber(line, col));
    } else if (c == '"') {
      tokens.push_back(lexString(line, col));
    } else {
      switch (c) {
      case '(': advance(); tokens.push_back(makeToken(TokenKind::LParen, "(", line, col)); break;
      case ')': advance(); tokens.push_back(makeToken(TokenKind::RParen, ")", line, col)); break;
      case '{': advance(); tokens.push_back(makeToken(TokenKind::LBrace, "{", line, col)); break;
      case '}': advance(); tokens.push_back(makeToken(TokenKind::RBrace, "}", line, col)); break;
      case '[': advance(); tokens.push_back(makeToken(TokenKind::LBracket, "[", line, col)); break;
      case ']': advance(); tokens.push_back(makeToken(TokenKind::RBracket, "]", line, col)); break;
      case ':': advance(); tokens.push_back(makeToken(TokenKind::Colon, ":", line, col)); break;
      case ';': advance(); tokens.push_back(makeToken(TokenKind::Semi, ";", line, col)); break;
      case ',': advance(); tokens.push_back(makeToken(TokenKind::Comma, ",", line, col)); break;
      case '.': advance(); tokens.push_back(makeToken(TokenKind::Dot, ".", line, col)); break;
      case '+': advance(); tokens.push_back(makeToken(TokenKind::Plus, "+", line, col)); break;
      case '-':
        advance();
        if (peek() == '>') {
          // `->` selalu arrow (Minus+G-inverse tak pernah valid berdampingan).
          advance();
          tokens.push_back(makeToken(TokenKind::Arrow, "->", line, col));
        } else {
          tokens.push_back(makeToken(TokenKind::Minus, "-", line, col));
        }
        break;
      case '*': advance(); tokens.push_back(makeToken(TokenKind::Star, "*", line, col)); break;
      case '/': advance(); tokens.push_back(makeToken(TokenKind::Slash, "/", line, col)); break;
      case '=':
        advance();
        if (peek() == '=') {
          advance();
          tokens.push_back(makeToken(TokenKind::Eq, "==", line, col));
        } else if (peek() == '>') {
          // `=>` lengan match (Phase 25). Aman: `= >` terpisah tak
          // pernah valid (ekspresi tak mengenal `>` setelah `=`).
          advance();
          tokens.push_back(makeToken(TokenKind::MatchArrow, "=>", line, col));
        } else {
          tokens.push_back(makeToken(TokenKind::Assign, "=", line, col));
        }
        break;
      case '!':
        advance();
        if (peek() == '=') {
          advance();
          tokens.push_back(makeToken(TokenKind::NotEq, "!=", line, col));
        } else {
          errorAt(line, col, "unexpected character '!' (did you mean '!='?)");
          tokens.push_back(makeToken(TokenKind::Unknown, "!", line, col));
        }
        break;
      case '<':
        advance();
        if (peek() == '=') {
          advance();
          tokens.push_back(makeToken(TokenKind::LtEq, "<=", line, col));
        } else {
          tokens.push_back(makeToken(TokenKind::Lt, "<", line, col));
        }
        break;
      case '>':
        advance();
        if (peek() == '=') {
          advance();
          tokens.push_back(makeToken(TokenKind::GtEq, ">=", line, col));
        } else {
          tokens.push_back(makeToken(TokenKind::Gt, ">", line, col));
        }
        break;
      case '?':
        advance();
        tokens.push_back(makeToken(TokenKind::Question, "?", line, col));
        break;
      case '|':
        advance();
        tokens.push_back(makeToken(TokenKind::Pipe, "|", line, col));
        break;
      default: {
        std::string bad(1, advance());
        errorAt(line, col, "unexpected character '" + bad + "'");
        tokens.push_back(makeToken(TokenKind::Unknown, bad, line, col));
        break;
      }
      }
    }
  }
  Token eof;
  eof.kind = TokenKind::Eof;
  eof.lexeme = "";
  eof.loc = Location{file_, line_, col_};
  tokens.push_back(eof);
  return tokens;
}

} // namespace nusa
