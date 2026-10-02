#include "compiler/diagnostics/Diagnostic.h"
#include <cstdio>
#include <string>

namespace nusa {

DiagnosticEngine::DiagnosticEngine(std::vector<std::string> sourceLines)
    : lines_(std::move(sourceLines)) {}

std::vector<std::string> DiagnosticEngine::splitLines(const std::string &src) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : src) {
    if (c == '\n') {
      out.push_back(cur);
      cur.clear();
    } else if (c != '\r') {
      cur.push_back(c);
    }
  }
  out.push_back(cur);
  return out;
}

void DiagnosticEngine::reportLexerError(const std::string &msg) {
  // Lexer sudah memformat "file:line:col: error: ..." — teruskan apa adanya.
  std::fprintf(stderr, "%s\n", msg.c_str());
  errorCount_++;
}

void DiagnosticEngine::reportParseError(const ParseError &err) {
  printOne(err.loc, err.message, err.hint, err.length);
}

void DiagnosticEngine::printOne(const Location &loc, const std::string &msg,
                                const std::string &hint, int length) {
  std::fprintf(stderr, "%s:%d:%d: error: %s\n", loc.file.c_str(), loc.line,
               loc.column, msg.c_str());
  if (loc.line >= 1 && loc.line <= (int)lines_.size()) {
    const std::string &snippet = lines_[loc.line - 1];
    std::fprintf(stderr, "\n    %s\n", snippet.c_str());
    // Span caret dari kolom awal, dijepit ke sisa baris (min 1).
    int start = loc.column < 1 ? 1 : loc.column;
    int avail = (int)snippet.size() - (start - 1);
    if (avail < 1)
      avail = 1;
    int span = length < 1 ? 1 : length;
    if (span > avail)
      span = avail;
    std::string caret = "    ";
    for (int i = 1; i < start; i++)
      caret.push_back(' ');
    caret.append(static_cast<size_t>(span), '^');
    std::fprintf(stderr, "%s\n", caret.c_str());
  }
  if (!hint.empty()) {
    std::fprintf(stderr, "\nhelp: %s\n", hint.c_str());
  }
  errorCount_++;
}

} // namespace nusa
