#pragma once
#include "compiler/lexer/Token.h"
#include "compiler/parser/Parser.h"
#include <string>
#include <vector>

namespace nusa {

// Pencetak diagnostics dengan format (meniru contoh MASTER PROMPT):
//   file:line:col: error: pesan
//
//       <source snippet>
//       <caret beruntun selebar span>
//
//   help: saran
class DiagnosticEngine {
public:
  explicit DiagnosticEngine(std::vector<std::string> sourceLines);

  void reportLexerError(const std::string &msg);
  void reportParseError(const ParseError &err);
  bool hasErrors() const { return errorCount_ > 0; }
  int errorCount() const { return errorCount_; }

  // Helper statis untuk memecah source menjadi lines.
  static std::vector<std::string> splitLines(const std::string &src);

private:
  void printOne(const Location &loc, const std::string &msg,
                const std::string &hint, int length = 1);

  std::vector<std::string> lines_;
  int errorCount_ = 0;
};

} // namespace nusa
