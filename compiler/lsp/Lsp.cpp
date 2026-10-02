// Nusa Language Server (Phase 36). Lihat Lsp.h.
#include "compiler/lsp/Lsp.h"
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/doc/DocGen.h"
#include "compiler/fmt/Formatter.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"
#include "compiler/semantic/Semantic.h"
#include <cctype>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace nusa {
namespace lsp {
namespace {

// ================= JSON minimal =================
// Cukup untuk LSP: null/bool/int/string/array/object + escape \u.

struct J {
  enum class T { Null, Bool, Int, Str, Arr, Obj };
  T type = T::Null;
  bool boolean = false;
  long long num = 0;
  std::string str;
  std::vector<J> arr;
  std::vector<std::pair<std::string, J>> obj;

  static J jbool(bool b) {
    J j;
    j.type = T::Bool;
    j.boolean = b;
    return j;
  }
  static J integer(long long n) {
    J j;
    j.type = T::Int;
    j.num = n;
    return j;
  }
  static J string(const std::string &s) {
    J j;
    j.type = T::Str;
    j.str = s;
    return j;
  }
  static J array() {
    J j;
    j.type = T::Arr;
    return j;
  }
  static J object() {
    J j;
    j.type = T::Obj;
    return j;
  }
  const J *find(const std::string &key) const {
    if (type != T::Obj)
      return nullptr;
    for (const auto &kv : obj) {
      if (kv.first == key)
        return &kv.second;
    }
    return nullptr;
  }
  void set(const std::string &key, J value) {
    if (type != T::Obj)
      type = T::Obj;
    for (auto &kv : obj) {
      if (kv.first == key) {
        kv.second = std::move(value);
        return;
      }
    }
    obj.emplace_back(key, std::move(value));
  }
};

static void appendUtf8(std::string &o, unsigned c) {
  if (c < 0x80) {
    o.push_back(static_cast<char>(c));
  } else if (c < 0x800) {
    o.push_back(static_cast<char>(0xC0 | (c >> 6)));
    o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
  } else {
    o.push_back(static_cast<char>(0xE0 | (c >> 12)));
    o.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
    o.push_back(static_cast<char>(0x80 | (c & 0x3F)));
  }
}

struct JsonParser {
  const std::string &s;
  size_t i = 0;
  std::string err;

  bool parse(J &out) {
    skip();
    if (!parseValue(out))
      return false;
    skip();
    if (i != s.size()) {
      err = "trailing content";
      return false;
    }
    return true;
  }
  void skip() {
    while (i < s.size() &&
           (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
      i++;
  }
  bool parseValue(J &out) {
    if (i >= s.size()) {
      err = "unexpected end";
      return false;
    }
    char c = s[i];
    if (c == '{')
      return parseObject(out);
    if (c == '[')
      return parseArray(out);
    if (c == '"')
      return parseString(out);
    if (c == 't' || c == 'f')
      return parseBool(out);
    if (c == 'n')
      return parseNull(out);
    return parseNumber(out);
  }
  bool expect(char c, const char *what) {
    if (i >= s.size() || s[i] != c) {
      err = std::string("expected ") + what;
      return false;
    }
    i++;
    return true;
  }
  bool parseObject(J &out) {
    out = J::object();
    if (!expect('{', "'{'"))
      return false;
    skip();
    if (i < s.size() && s[i] == '}') {
      i++;
      return true;
    }
    while (true) {
      skip();
      if (i >= s.size() || s[i] != '"') {
        err = "expected string key";
        return false;
      }
      J key;
      if (!parseString(key))
        return false;
      skip();
      if (!expect(':', "':'"))
        return false;
      skip();
      J val;
      if (!parseValue(val))
        return false;
      out.obj.emplace_back(key.str, std::move(val));
      skip();
      if (i < s.size() && s[i] == ',') {
        i++;
        continue;
      }
      if (!expect('}', "'}'"))
        return false;
      return true;
    }
  }
  bool parseArray(J &out) {
    out = J::array();
    if (!expect('[', "'['"))
      return false;
    skip();
    if (i < s.size() && s[i] == ']') {
      i++;
      return true;
    }
    while (true) {
      skip();
      J val;
      if (!parseValue(val))
        return false;
      out.arr.push_back(std::move(val));
      skip();
      if (i < s.size() && s[i] == ',') {
        i++;
        continue;
      }
      if (!expect(']', "']'"))
        return false;
      return true;
    }
  }
  bool parseString(J &out) {
    out = J::string("");
    if (!expect('"', "'\"'"))
      return false;
    while (i < s.size() && s[i] != '"') {
      if (s[i] == '\\') {
        i++;
        if (i >= s.size()) {
          err = "bad escape";
          return false;
        }
        char e = s[i++];
        if (e == '"')
          out.str.push_back('"');
        else if (e == '\\')
          out.str.push_back('\\');
        else if (e == '/')
          out.str.push_back('/');
        else if (e == 'n')
          out.str.push_back('\n');
        else if (e == 'r')
          out.str.push_back('\r');
        else if (e == 't')
          out.str.push_back('\t');
        else if (e == 'b')
          out.str.push_back('\b');
        else if (e == 'f')
          out.str.push_back('\f');
        else if (e == 'u') {
          if (i + 4 > s.size()) {
            err = "bad \\u escape";
            return false;
          }
          unsigned cp = 0;
          for (int k = 0; k < 4; k++) {
            char h = s[i++];
            cp <<= 4;
            if (h >= '0' && h <= '9')
              cp += static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f')
              cp += static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
              cp += static_cast<unsigned>(h - 'A' + 10);
            else {
              err = "bad \\u escape";
              return false;
            }
          }
          // Surrogate pair dasar (cukup untuk teks editor).
          if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() &&
              s[i] == '\\' && s[i + 1] == 'u') {
            unsigned lo = 0;
            bool ok = true;
            for (int k = 0; k < 4; k++) {
              char h = s[i + 2 + k];
              lo <<= 4;
              if (h >= '0' && h <= '9')
                lo += static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f')
                lo += static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F')
                lo += static_cast<unsigned>(h - 'A' + 10);
              else
                ok = false;
            }
            if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
              i += 6;
            }
          }
          appendUtf8(out.str, cp);
        } else {
          err = "bad escape";
          return false;
        }
      } else {
        out.str.push_back(s[i++]);
      }
    }
    if (!expect('"', "closing '\"'"))
      return false;
    return true;
  }
  bool parseBool(J &out) {
    if (s.compare(i, 4, "true") == 0) {
      i += 4;
      out = J::jbool(true);
      return true;
    }
    if (s.compare(i, 5, "false") == 0) {
      i += 5;
      out = J::jbool(false);
      return true;
    }
    err = "invalid literal";
    return false;
  }
  bool parseNull(J &out) {
    if (s.compare(i, 4, "null") == 0) {
      i += 4;
      out = J();
      return true;
    }
    err = "invalid literal";
    return false;
  }
  bool parseNumber(J &out) {
    size_t start = i;
    bool neg = false;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
      neg = (s[i] == '-');
      i++;
    }
    long long val = 0;
    bool anyDigit = false;
    while (i < s.size() && std::isdigit((unsigned char)s[i])) {
      anyDigit = true;
      val = val * 10 + (s[i] - '0');
      i++;
    }
    if (!anyDigit) {
      err = "invalid value";
      return false;
    }
    // Fraksi/eksponen: LSP hanya butuh bilangan bulat (line/col);
    // lewati presisi (truncate).
    if (i < s.size() && s[i] == '.') {
      i++;
      while (i < s.size() && std::isdigit((unsigned char)s[i]))
        i++;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
      i++;
      if (i < s.size() && (s[i] == '-' || s[i] == '+'))
        i++;
      bool anyExp = false;
      while (i < s.size() && std::isdigit((unsigned char)s[i])) {
        anyExp = true;
        i++;
      }
      if (!anyExp) {
        err = "invalid number";
        return false;
      }
    }
    if (start == i) {
      err = "invalid value";
      return false;
    }
    out = J::integer(neg ? -val : val);
    (void)start;
    return true;
  }
};

static std::string jsonEscape(const std::string &s) {
  std::string o;
  for (unsigned char c : s) {
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else if (c == '\t')
      o += "\\t";
    else if (c < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\u%04x", c);
      o += buf;
    } else {
      o.push_back(static_cast<char>(c));
    }
  }
  return o;
}

static std::string jsonDump(const J &j) {
  switch (j.type) {
  case J::T::Null:
    return "null";
  case J::T::Bool:
    return j.boolean ? "true" : "false";
  case J::T::Int:
    return std::to_string(j.num);
  case J::T::Str:
    return "\"" + jsonEscape(j.str) + "\"";
  case J::T::Arr: {
    std::string o = "[";
    for (size_t i = 0; i < j.arr.size(); i++) {
      if (i)
        o += ",";
      o += jsonDump(j.arr[i]);
    }
    return o + "]";
  }
  case J::T::Obj: {
    std::string o = "{";
    for (size_t i = 0; i < j.obj.size(); i++) {
      if (i)
        o += ",";
      o += "\"" + jsonEscape(j.obj[i].first) + "\":" +
           jsonDump(j.obj[i].second);
    }
    return o + "}";
  }
  }
  return "null";
}

// Akses aman dengan default.
static long long jsonInt(const J *j, long long dflt) {
  if (!j)
    return dflt;
  if (j->type == J::T::Int)
    return j->num;
  return dflt;
}

static std::string jsonStr(const J *j, const std::string &dflt = "") {
  if (!j)
    return dflt;
  if (j->type == J::T::Str)
    return j->str;
  return dflt;
}



// ================= transport stdio =================

static bool readMessage(std::string &out) {
  out.clear();
  std::string line;
  long long length = -1;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      break;
    size_t colon = line.find(':');
    if (colon == std::string::npos)
      continue;
    std::string key = line.substr(0, colon);
    std::string val = line.substr(colon + 1);
    size_t s = 0;
    while (s < val.size() && (val[s] == ' ' || val[s] == '\t'))
      s++;
    val = val.substr(s);
    std::string low;
    for (char c : key)
      low.push_back(static_cast<char>(std::tolower((unsigned char)c)));
    if (low == "content-length") {
      long long n = 0;
      bool any = false;
      for (char c : val) {
        if (std::isdigit((unsigned char)c)) {
          any = true;
          n = n * 10 + (c - '0');
        }
      }
      if (any)
        length = n;
    }
  }
  if (std::cin.eof() && length < 0)
    return false;
  if (length < 0 || length > (1 << 26))
    return false;
  out.resize(static_cast<size_t>(length));
  size_t got = 0;
  while (got < out.size() && std::cin) {
    std::cin.read(&out[got], out.size() - got);
    got += static_cast<size_t>(std::cin.gcount());
  }
  return got == out.size();
}

static void sendMessage(const std::string &body) {
  std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
  std::cout.flush();
}

// ================= dokumen + analisis =================

struct Doc {
  std::string uri;
  std::string text;
  long long version = 0;
};

struct Analysis {
  std::string file;
  std::vector<std::string> lines;
  std::vector<Token> tokens;
  Program program;
  std::vector<std::pair<Location, std::string>> lexErrors; // loc + pesan
  std::vector<ParseError> parseErrors;
  std::vector<SemanticError> semanticErrors;
  bool lexOk = false;
  bool parseOk = false;
};

// "f:l:c: msg" lexer → loc; fallback line 1 col 1.
static bool splitLexError(const std::string &e, Location &loc,
                          std::string &msg) {
  size_t p1 = e.find(':');
  if (p1 == std::string::npos)
    return false;
  size_t p2 = e.find(':', p1 + 1);
  if (p2 == std::string::npos)
    return false;
  size_t p3 = e.find(':', p2 + 1);
  if (p3 == std::string::npos)
    return false;
  auto num = [](const std::string &s, int &v) {
    if (s.empty())
      return false;
    v = 0;
    for (char c : s) {
      if (!std::isdigit((unsigned char)c))
        return false;
      v = v * 10 + (c - '0');
    }
    return v > 0;
  };
  int line = 0, col = 0;
  if (!num(e.substr(p1 + 1, p2 - p1 - 1), line) ||
      !num(e.substr(p2 + 1, p3 - p2 - 1), col))
    return false;
  loc.file = e.substr(0, p1);
  loc.line = line;
  loc.column = col;
  msg = e.substr(p3 + 1);
  size_t s = 0;
  while (s < msg.size() && msg[s] == ' ')
    s++;
  msg = msg.substr(s);
  return true;
}

static Analysis analyzeDoc(const std::string &file, const std::string &text) {
  Analysis a;
  a.file = file;
  a.lines = DiagnosticEngine::splitLines(text);
  Lexer lx(file, text);
  a.tokens = lx.tokenize();
  for (const auto &e : lx.errors()) {
    Location loc{file, 1, 1};
    std::string msg = e;
    Location parsed{file, 1, 1};
    std::string pmsg;
    if (splitLexError(e, parsed, pmsg)) {
      loc = parsed;
      msg = pmsg;
    }
    a.lexErrors.emplace_back(loc, msg);
  }
  a.lexOk = a.lexErrors.empty();
  if (!a.lexOk)
    return a;
  Parser p(file, a.tokens, a.lines);
  ParseResult pres = p.parse();
  a.parseErrors = pres.errors;
  a.parseOk = pres.ok();
  if (!a.parseOk)
    return a;
  a.program = std::move(pres.program);
  SemanticAnalyzer sema;
  a.semanticErrors = sema.analyze(a.program);
  return a;
}

// ================= posisi =================

static int lineLen(const std::vector<std::string> &lines, int line0) {
  if (line0 < 0 || line0 >= (int)lines.size())
    return 0;
  return (int)lines[line0].size();
}

static int clampInt(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

static bool isIdentChar(char c) {
  return std::isalnum((unsigned char)c) || c == '_';
}

// Kata di posisi; "" bila tak ada. start/end = kolom 0-based [start,end).
static std::string wordAt(const std::vector<std::string> &lines, int line0,
                          int ch0, int &start, int &end) {
  start = end = ch0;
  if (line0 < 0 || line0 >= (int)lines.size())
    return "";
  const std::string &line = lines[line0];
  if (ch0 < 0 || ch0 > (int)line.size())
    return "";
  int s = ch0, e = ch0;
  while (s > 0 && isIdentChar(line[s - 1]))
    s--;
  while (e < (int)line.size() && isIdentChar(line[e]))
    e++;
  if (s == e)
    return "";
  start = s;
  end = e;
  return line.substr(s, e - s);
}

// Prefix identifier tepat sebelum kursor (untuk completion).
static std::string prefixBefore(const std::vector<std::string> &lines,
                                int line0, int ch0) {
  if (line0 < 0 || line0 >= (int)lines.size())
    return "";
  const std::string &line = lines[line0];
  int e = clampInt(ch0, 0, (int)line.size());
  int s = e;
  while (s > 0 && isIdentChar(line[s - 1]))
    s--;
  return line.substr(s, e - s);
}

static J posJson(int line0, int ch0) {
  J o = J::object();
  o.set("line", J::integer(line0));
  o.set("character", J::integer(ch0));
  return o;
}

static J rangeFromLoc(const Location &loc, int length,
                      const std::vector<std::string> &lines) {
  int l0 = loc.line - 1;
  int c0 = clampInt(loc.column - 1, 0, lineLen(lines, l0));
  int c1 = clampInt(c0 + length, 0, lineLen(lines, l0));
  J r = J::object();
  r.set("start", posJson(l0, c0));
  r.set("end", posJson(l0, c1));
  return r;
}

// ================= simbol top-level =================

struct TopSym {
  std::string name;
  std::string kind; // "fn", "struct", "enum", "trait", "alias"
  Location loc;
  std::string detail; // signature / ringkasan
};

static std::string fnSig(const FunctionDecl &fn) {
  std::string o = "fn " + fn.name + "(";
  for (size_t i = 0; i < fn.params.size(); i++) {
    if (i)
      o += ", ";
    o += fn.params[i].name + ": " + fn.params[i].type.name();
  }
  o += ") -> " + fn.retType.name();
  return o;
}

static std::vector<TopSym> topSymbols(const Program &prog) {
  std::vector<TopSym> out;
  for (const auto &fn : prog.functions) {
    if (fn.isExtern)
      continue;
    out.push_back(TopSym{fn.name, "fn", fn.loc, fnSig(fn)});
  }
  for (const auto &fn : prog.functions) {
    if (!fn.isExtern)
      continue;
    out.push_back(TopSym{fn.name, "extern", fn.loc, "extern " + fnSig(fn)});
  }
  for (const auto &sd : prog.structs)
    out.push_back(TopSym{sd.name, "struct", sd.loc,
                         "struct " + sd.name +
                             (sd.tparams.empty() ? "" : "<...>")});
  for (const auto &ed : prog.enums)
    out.push_back(TopSym{ed.name, "enum", ed.loc, "enum " + ed.name});
  for (const auto &td : prog.traits)
    out.push_back(TopSym{td.name, "trait", td.loc, "trait " + td.name});
  for (const auto &ad : prog.aliases)
    out.push_back(TopSym{ad.name, "alias", ad.loc,
                         "type " + ad.name + " = " + ad.target.name()});
  return out;
}

// ================= resolusi scope (locals) =================
// Walk tunggal: catat binding (def) + pemakaian (use) VarRef +
// callee Call. Method/field/pola-varian dilewati (bukan variabel).

struct Occ {
  std::string name;
  Location loc;    // lokasi nama ini
  Location defLoc; // lokasi definisi yang dirujuk ("" bila tak resolved)
  bool isDef = false;
};

static void patBinds(const Pat &pat, std::vector<std::pair<std::string,
                                                           Location>> &out) {
  struct V {
    std::vector<std::pair<std::string, Location>> &out;
    void operator()(const Pat::Bind &n) {
      out.emplace_back(n.name, n.loc);
    }
    void operator()(const Pat::Struct &n) {
      for (const auto &f : n.fields)
        patBinds(*f.pat, out);
    }
    void operator()(const Pat::Variant &n) {
      for (const auto &a : n.args)
        patBinds(*a, out);
    }
    void operator()(const Pat::Wild &) {}
    void operator()(const Pat::LitInt &) {}
    void operator()(const Pat::LitBool &) {}
    void operator()(const Pat::LitStr &) {}
  };
  std::visit(V{out}, pat.node);
}

struct ScopeWalk {
  // scope: nama -> loc definisi (terdalam menang).
  std::vector<std::vector<std::pair<std::string, Location>>> scopes;
  std::vector<Occ> occs;
  const std::vector<std::string> *lines_ = nullptr;

  // Lokasi NAMA dari loc keyword (`let x` → kolom `x`). AST Nusa
  // menyimpan loc keyword untuk Let/Function/Struct/Enum/Trait/Alias;
  // LSP butuh span nama yang tepat (def/goto/rename). Cari di baris
  // (tahan tab/spasi ganda); fallback aritmetika +1 spasi.
  static Location nameLoc(const std::vector<std::string> &lines,
                          const Location &kw, int kwLen,
                          const std::string &name) {
    int l0 = kw.line - 1;
    int start = kw.column - 1 + kwLen; // 0-based, sesudah keyword
    if (l0 >= 0 && l0 < (int)lines.size()) {
      const std::string &line = lines[l0];
      for (int i = start; i + (int)name.size() <= (int)line.size(); i++) {
        if (line.compare(i, name.size(), name) != 0)
          continue;
        bool leftOk = (i == 0) || !isIdentChar(line[i - 1]);
        bool rightOk = (i + (int)name.size() >= (int)line.size()) ||
                       !isIdentChar(line[i + name.size()]);
        if (leftOk && rightOk) {
          Location l = kw;
          l.column = i + 1;
          return l;
        }
      }
    }
    Location l = kw;
    l.column += kwLen + 1;
    return l;
  }

  void push() { scopes.emplace_back(); }
  void pop() { scopes.pop_back(); }
  void define(const std::string &name, const Location &loc) {
    scopes.back().emplace_back(name, loc);
    occs.push_back(Occ{name, loc, loc, true});
  }
  Location lookup(const std::string &name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      for (auto jt = it->rbegin(); jt != it->rend(); ++jt) {
        if (jt->first == name)
          return jt->second;
      }
    }
    return Location{};
  }
  void use(const std::string &name, const Location &loc) {
    Location d = lookup(name);
    if (d.file.empty() && d.line == 0)
      return; // unresolved (global tak dikenal di sini / error)
    occs.push_back(Occ{name, loc, d, false});
  }

  void walkExpr(const Expr &e) {
    struct V {
      ScopeWalk *self;
      void operator()(const Expr::VarRef &n) {
        self->use(n.name, n.loc);
      }
      void operator()(const Expr::Call &n) {
        self->use(n.callee, n.loc);
        for (const auto &a : n.args)
          self->walkExpr(*a);
      }
      void operator()(const Expr::MethodCall &n) {
        self->walkExpr(*n.receiver);
        for (const auto &a : n.args)
          self->walkExpr(*a);
      }
      void operator()(const Expr::Propagate &n) {
        self->walkExpr(*n.expr);
      }
      void operator()(const Expr::ArrayLit &n) {
        for (const auto &el : n.elems)
          self->walkExpr(*el);
      }
      void operator()(const Expr::Index &n) {
        self->walkExpr(*n.base);
        self->walkExpr(*n.index);
      }
      void operator()(const Expr::StructLit &n) {
        for (const auto &f : n.fields)
          self->walkExpr(*f.value);
      }
      void operator()(const Expr::FieldAccess &n) {
        self->walkExpr(*n.base);
      }
      void operator()(const Expr::Unary &n) {
        self->walkExpr(*n.operand);
      }
      void operator()(const Expr::Binary &n) {
        self->walkExpr(*n.lhs);
        self->walkExpr(*n.rhs);
      }
      void operator()(const Expr::Lambda &n) {
        self->push();
        for (const auto &p : n.params)
          self->define(p.name, p.loc);
        self->walkBlock(*n.body);
        self->pop();
      }
      void operator()(const Expr::Closure &n) {
        for (const auto &c : n.captures)
          self->walkExpr(*c);
      }
      void operator()(const Expr::LiteralInt &) {}
      void operator()(const Expr::LiteralBool &) {}
      void operator()(const Expr::LiteralString &) {}
    };
    std::visit(V{this}, e.node);
  }

  void walkBlock(const Block &b) {
    push();
    for (const auto &s : b.stmts)
      walkStmt(s);
    pop();
  }

  void walkStmt(const Stmt &s) {
    struct V {
      ScopeWalk *self;
      void operator()(const LetStmt &n) {
        self->walkExpr(*n.value);
        self->define(n.name, ScopeWalk::nameLoc(*self->lines_, n.loc, 3,
                                                n.name)); // `let`
      }
      void operator()(const PrintStmt &n) { self->walkExpr(*n.arg); }
      void operator()(const ReturnStmt &n) {
        if (n.value)
          self->walkExpr(*n.value);
      }
      void operator()(const ExprStmt &n) { self->walkExpr(*n.expr); }
      void operator()(const IfStmt &n) {
        self->walkExpr(*n.cond);
        self->walkBlock(*n.thenBlock);
        if (n.elseBlock)
          self->walkBlock(*n.elseBlock);
      }
      void operator()(const WhileStmt &n) {
        self->walkExpr(*n.cond);
        self->walkBlock(*n.body);
      }
      void operator()(const AssignStmt &n) {
        self->use(n.name, n.loc); // assign = pemakaian, bukan definisi
        self->walkExpr(*n.value);
      }
      void operator()(const MatchStmt &n) {
        self->walkExpr(*n.subject);
        for (const auto &arm : n.arms) {
          self->push();
          std::vector<std::pair<std::string, Location>> binds;
          patBinds(arm.pat, binds);
          for (const auto &bn : binds)
            self->define(bn.first, bn.second);
          if (arm.guard)
            self->walkExpr(*arm.guard);
          self->walkBlock(*arm.body);
          self->pop();
        }
      }
    };
    std::visit(V{this}, s);
  }

  // Program utuh: global (top-level symbols) + tiap body fungsi.
  void walkProgram(const Program &prog,
                   const std::vector<std::string> &lines) {
    lines_ = &lines;
    push(); // scope global
    for (const auto &fn : prog.functions)
      define(fn.name, nameLoc(lines, fn.loc, 2, fn.name)); // `fn`
    for (const auto &sd : prog.structs)
      define(sd.name, nameLoc(lines, sd.loc, 6, sd.name)); // `struct`
    for (const auto &ed : prog.enums)
      define(ed.name, nameLoc(lines, ed.loc, 4, ed.name)); // `enum`
    for (const auto &td : prog.traits)
      define(td.name, nameLoc(lines, td.loc, 5, td.name)); // `trait`
    for (const auto &ad : prog.aliases)
      define(ad.name, nameLoc(lines, ad.loc, 4, ad.name)); // `type`
    for (const auto &fn : prog.functions) {
      if (fn.isExtern)
        continue;
      push(); // scope fungsi
      for (const auto &p : fn.params)
        define(p.name, p.loc);
      walkBlock(fn.body);
      pop();
    }
    for (const auto &ib : prog.impls) {
      for (const auto &m : ib.methods) {
        push();
        for (const auto &p : m.params)
          define(p.name, p.loc);
        walkBlock(m.body);
        pop();
      }
    }
    // (scope global dibiarkan; lookup tetap valid.)
  }
};

// Definisi di posisi kursor (def site atau use site) → defLoc.
// Kosong (file=="") bila tak ketemu.
static Location defAt(const std::vector<Occ> &occs,
                      const std::vector<std::string> &lines, int line0,
                      int ch0) {
  (void)lines;
  for (const auto &o : occs) {
    int ol = o.loc.line - 1;
    if (ol != line0)
      continue;
    int os = o.loc.column - 1;
    int oe = os + (int)o.name.size();
    if (ch0 >= os && ch0 < oe)
      return o.defLoc;
  }
  return Location{};
}

// ================= diagnostics ==================

static J diagnosticJson(const Location &loc, int length,
                        const std::string &message,
                        const std::vector<std::string> &lines) {
  J d = J::object();
  d.set("range", rangeFromLoc(loc, length, lines));
  d.set("severity", J::integer(1)); // Error
  d.set("source", J::string("nusa"));
  d.set("message", J::string(message));
  return d;
}

static J publishParams(const std::string &uri, const Analysis &a) {
  J p = J::object();
  p.set("uri", J::string(uri));
  J diags = J::array();
  for (const auto &le : a.lexErrors)
    diags.arr.push_back(
        diagnosticJson(le.first, 1, le.second, a.lines));
  for (const auto &pe : a.parseErrors) {
    std::string msg = pe.message;
    if (!pe.hint.empty())
      msg += "\nhint: " + pe.hint;
    diags.arr.push_back(
        diagnosticJson(pe.loc, pe.length, msg, a.lines));
  }
  for (const auto &se : a.semanticErrors) {
    std::string msg = se.message;
    if (!se.hint.empty())
      msg += "\nhint: " + se.hint;
    diags.arr.push_back(
        diagnosticJson(se.loc, se.length, msg, a.lines));
  }
  p.set("diagnostics", std::move(diags));
  return p;
}

// ================= completion =================

static const char *kKeywords[] = {
    "fn",   "let",    "return", "if",   "else",  "while", "true",
    "false", "match",  "struct", "enum", "trait", "impl",  "for",
    "self", "import", "type",   "print", nullptr};

static const char *kKeywordHelp[] = {
    "function declaration", "variable binding", "return from function",
    "conditional", "else branch", "loop", "boolean literal",
    "boolean literal", "pattern match", "struct declaration",
    "enum declaration", "trait declaration", "trait implementation",
    "loop variable", "method receiver", "module import", "type alias",
    "print statement", nullptr};

static J completionItems(const Analysis &a, int line0, int ch0) {
  J items = J::array();
  std::string prefix = prefixBefore(a.lines, line0, ch0);
  auto startsWith = [&](const std::string &name) {
    return name.compare(0, prefix.size(), prefix) == 0;
  };
  // Kata kunci.
  for (int i = 0; kKeywords[i]; i++) {
    std::string kw = kKeywords[i];
    if (!startsWith(kw))
      continue;
    J it = J::object();
    it.set("label", J::string(kw));
    it.set("kind", J::integer(14)); // Keyword
    it.set("detail", J::string(kKeywordHelp[i]));
    items.arr.push_back(std::move(it));
  }
  // Builtin (fungsi global tanpa deklarasi).
  static const char *builtins[] = {
      "len", "input", "assert", "str_len", "str_concat", "str_eq",
      "parse_int", "parse_int_valid", "file_read", "file_write",
      "file_exists", "array_sum", "array_min", "array_max",
      "thread_spawn", "thread_join", "mutex_new", "mutex_lock",
      "mutex_unlock", "mutex_try_lock", "mutex_free", "chan_new",
      "chan_send", "chan_recv", "chan_free", "chan_new_bounded",
      "chan_try_send", "chan_new_bool",
      "chan_send_bool", "chan_recv_bool", "chan_new_str", "chan_send_str",
      "chan_recv_str", "atomic_new", "atomic_get",
      "atomic_set", "atomic_add", "atomic_free", "sleep_ms",
      "str_char_at", "str_slice", "time_now_ms", "time_now_s",
      "random_u64", "process_exit", "process_env", "process_cwd",
      "process_argc", "process_argv", "tcp_connect", "tcp_send",
      "tcp_recv", "tcp_close", "tcp_listen", "tcp_accept", nullptr};
  for (int i = 0; builtins[i]; i++) {
    std::string b = builtins[i];
    if (!startsWith(b))
      continue;
    J it = J::object();
    it.set("label", J::string(b));
    it.set("kind", J::integer(3)); // Function
    it.set("detail", J::string("builtin"));
    items.arr.push_back(std::move(it));
  }
  if (!a.parseOk)
    return items;
  // Simbol top-level.
  for (const auto &ts : topSymbols(a.program)) {
    if (!startsWith(ts.name))
      continue;
    J it = J::object();
    it.set("label", J::string(ts.name));
    int kind = 3; // Function
    if (ts.kind == "struct")
      kind = 22; // Struct
    else if (ts.kind == "enum")
      kind = 13; // Enum
    else if (ts.kind == "trait")
      kind = 11; // Interface
    else if (ts.kind == "alias")
      kind = 7; // Class (tipe)
    it.set("kind", J::integer(kind));
    it.set("detail", J::string(ts.detail));
    items.arr.push_back(std::move(it));
  }
  // Local terlihat di posisi (def sebelum kursor, terdalam dulu).
  if (a.parseOk) {
    ScopeWalk w;
    w.walkProgram(a.program, a.lines);
    std::vector<std::string> seen;
    for (auto it = w.occs.rbegin(); it != w.occs.rend(); ++it) {
      if (!it->isDef)
        continue;
      if (!startsWith(it->name))
        continue;
      bool dup = false;
      for (const auto &s : seen) {
        if (s == it->name) {
          dup = true;
          break;
        }
      }
      if (dup)
        continue;
      int dl = it->loc.line - 1;
      int dc = it->loc.column - 1;
      if (dl > line0 || (dl == line0 && dc >= ch0))
        continue; // didefinisikan sesudah kursor
      seen.push_back(it->name);
      J item = J::object();
      item.set("label", J::string(it->name));
      item.set("kind", J::integer(6)); // Variable
      items.arr.push_back(std::move(item));
    }
  }
  return items;
}

// ================= hover =================

static const char *hoverKeyword(const std::string &w) {
  for (int i = 0; kKeywords[i]; i++) {
    if (w == kKeywords[i])
      return kKeywordHelp[i];
  }
  return nullptr;
}

static J hoverAt(const Analysis &a, int line0, int ch0) {
  int s, e;
  std::string word = wordAt(a.lines, line0, ch0, s, e);
  if (word.empty() || !a.parseOk)
    return J();
  for (const auto &ts : topSymbols(a.program)) {
    if (ts.name == word) {
      J h = J::object();
      J contents = J::object();
      contents.set("kind", J::string("markdown"));
      contents.set("value", J::string("```nusa\n" + ts.detail + "\n```"));
      h.set("contents", std::move(contents));
      return h;
    }
  }
  if (const char *help = hoverKeyword(word)) {
    J h = J::object();
    h.set("contents", J::string(std::string("keyword `") + word + "`: " +
                                help));
    return h;
  }
  // Local: tampilkan peran (param/let) — tipe butuh checker (batas v1).
  ScopeWalk w;
  w.walkProgram(a.program, a.lines);
  for (const auto &o : w.occs) {
    if (!o.isDef || o.name != word)
      continue;
    int ol = o.loc.line - 1;
    int os = o.loc.column - 1;
    int oe = os + (int)o.name.size();
    if (ol == line0 && ch0 >= os && ch0 < oe) {
      J h = J::object();
      h.set("contents",
            J::string("local `" + word + "` (type info needs checker)"));
      return h;
    }
  }
  return J();
}

// ================= definition =================

static J definitionAt(const Analysis &a, int line0, int ch0,
                      const std::string &uri) {
  if (!a.parseOk)
    return J();
  ScopeWalk w;
  w.walkProgram(a.program, a.lines);
  Location d = defAt(w.occs, a.lines, line0, ch0);
  if (d.file.empty())
    return J();
  // Definisisi di file ini (impor lintas-file = batas v1).
  J loc = J::object();
  loc.set("uri", J::string(uri));
  loc.set("range", rangeFromLoc(d, 1, a.lines));
  return loc;
}

// ================= document symbols =================

// Akhir blok via pencocokan brace dari token (baris 0-based).
static int matchBraceEnd(const std::vector<Token> &tokens, int declLine0) {
  int depth = 0;
  bool started = false;
  int endLine = declLine0;
  for (const auto &t : tokens) {
    int tl = t.loc.line - 1;
    if (tl < declLine0)
      continue;
    if (t.kind == TokenKind::LBrace) {
      depth++;
      started = true;
    } else if (t.kind == TokenKind::RBrace && started) {
      depth--;
      endLine = tl;
      if (depth == 0)
        return endLine;
    }
  }
  return endLine;
}

static J documentSymbols(const Analysis &a) {
  J out = J::array();
  if (!a.parseOk)
    return out;
  auto sym = [&](const std::string &name, int kind, const Location &loc,
                 const std::string &detail) {
    J s = J::object();
    s.set("name", J::string(name));
    s.set("kind", J::integer(kind));
    s.set("detail", J::string(detail));
    int l0 = loc.line - 1;
    int c0 = clampInt(loc.column - 1, 0, lineLen(a.lines, l0));
    int el = matchBraceEnd(a.tokens, l0);
    J range = J::object();
    range.set("start", posJson(l0, c0));
    range.set("end", posJson(el, lineLen(a.lines, el)));
    s.set("range", range);
    s.set("selectionRange", rangeFromLoc(loc, (int)name.size(), a.lines));
    (void)c0;
    return s;
  };
  for (const auto &fn : a.program.functions) {
    J s = sym(fn.name, 12, fn.loc, fnSig(fn)); // Function
    out.arr.push_back(std::move(s));
  }
  for (const auto &sd : a.program.structs) {
    J s = sym(sd.name, 23, sd.loc, "struct"); // Struct
    out.arr.push_back(std::move(s));
  }
  for (const auto &ed : a.program.enums) {
    J s = sym(ed.name, 10, ed.loc, "enum"); // Enum
    J children = J::array();
    for (const auto &v : ed.variants) {
      J c = J::object();
      c.set("name", J::string(v.name));
      c.set("kind", J::integer(22)); // EnumMember
      J r = rangeFromLoc(v.loc, (int)v.name.size(), a.lines);
      c.set("range", r);
      c.set("selectionRange", std::move(r));
      children.arr.push_back(std::move(c));
    }
    s.set("children", std::move(children));
    out.arr.push_back(std::move(s));
  }
  for (const auto &td : a.program.traits) {
    J s = sym(td.name, 11, td.loc, "trait"); // Interface
    J children = J::array();
    for (const auto &m : td.methods) {
      J c = J::object();
      c.set("name", J::string(m.name));
      c.set("kind", J::integer(6)); // Method→Function-ish
      J r = rangeFromLoc(m.loc, (int)m.name.size(), a.lines);
      c.set("range", r);
      c.set("selectionRange", std::move(r));
      children.arr.push_back(std::move(c));
    }
    s.set("children", std::move(children));
    out.arr.push_back(std::move(s));
  }
  for (const auto &ad : a.program.aliases) {
    J s = sym(ad.name, 7, ad.loc, "type"); // Class (tipe)
    out.arr.push_back(std::move(s));
  }
  return out;
}

// ================= rename =================

static J renameAt(Analysis &a, int line0, int ch0,
                  const std::string &newName, const std::string &uri) {
  if (!a.parseOk)
    return J();
  if (newName.empty())
    return J();
  for (char c : newName) {
    if (!isIdentChar(c))
      return J();
  }
  ScopeWalk w;
  w.walkProgram(a.program, a.lines);
  Location d = defAt(w.occs, a.lines, line0, ch0);
  if (d.file.empty())
    return J();
  J edits = J::array();
  for (const auto &o : w.occs) {
    if (o.defLoc.file == d.file && o.defLoc.line == d.line &&
        o.defLoc.column == d.column && o.name.size() > 0) {
      // Hanya yang namanya sama dengan target (scope walk menjamin
      // resolusi; saring akhiran untuk keamanan).
      J e = J::object();
      e.set("range", rangeFromLoc(o.loc, (int)o.name.size(), a.lines));
      e.set("newText", J::string(newName));
      edits.arr.push_back(std::move(e));
    }
  }
  if (edits.arr.empty())
    return J();
  J changes = J::object();
  changes.set(uri, std::move(edits));
  J result = J::object();
  result.set("changes", std::move(changes));
  return result;
}

// ================= references (Phase 63, v2.14.0) =================
// Himpunan lokasi yang sama persis dengan rename (def + uses yang
// teresolusi ke definisi di posisi kursor); tanpa penggantian teks.
// includeDeclaration=false menyaring lokasi definisi. Batas sama
// dengan rename: satu file, scope-sintaksis, method/field dilewati.

static J referencesAt(Analysis &a, int line0, int ch0,
                      const std::string &uri, bool includeDecl) {
  J out = J::array();
  if (!a.parseOk)
    return out;
  ScopeWalk w;
  w.walkProgram(a.program, a.lines);
  Location d = defAt(w.occs, a.lines, line0, ch0);
  if (d.file.empty())
    return out;
  for (const auto &o : w.occs) {
    if (o.defLoc.file == d.file && o.defLoc.line == d.line &&
        o.defLoc.column == d.column && o.name.size() > 0) {
      if (o.isDef && !includeDecl)
        continue;
      J loc = J::object();
      loc.set("uri", J::string(uri));
      loc.set("range", rangeFromLoc(o.loc, (int)o.name.size(), a.lines));
      out.arr.push_back(std::move(loc));
    }
  }
  return out;
}

// ================= server =================

struct Server {
  std::map<std::string, Doc> docs;
  bool shutdownRequested = false;
  int exitCode = 0;

  Doc *findDoc(const std::string &uri) {
    auto it = docs.find(uri);
    if (it == docs.end())
      return nullptr;
    return &it->second;
  }

  void publish(const std::string &uri) {
    Doc *doc = findDoc(uri);
    if (!doc)
      return;
    Analysis a = analyzeDoc(uri, doc->text);
    J params = publishParams(uri, a);
    sendNotification("textDocument/publishDiagnostics", std::move(params));
  }

  void sendResponse(const J &id, J result) {
    J r = J::object();
    r.set("jsonrpc", J::string("2.0"));
    r.set("id", id);
    r.set("result", std::move(result));
    sendMessage(jsonDump(r));
  }

  void sendError(const J &id, long long code, const std::string &message) {
    J r = J::object();
    r.set("jsonrpc", J::string("2.0"));
    r.set("id", id);
    J e = J::object();
    e.set("code", J::integer(code));
    e.set("message", J::string(message));
    r.set("error", std::move(e));
    sendMessage(jsonDump(r));
  }

  void sendNotification(const std::string &method, J params) {
    J r = J::object();
    r.set("jsonrpc", J::string("2.0"));
    r.set("method", J::string(method));
    r.set("params", std::move(params));
    sendMessage(jsonDump(r));
  }

  static int posLine(const J &params) {
    const J *pos = nullptr;
    if (const J *pp = params.find("position"))
      pos = pp;
    return (int)jsonInt(pos ? pos->find("line") : nullptr, 0);
  }

  static int posChar(const J &params) {
    const J *pos = nullptr;
    if (const J *pp = params.find("position"))
      pos = pp;
    return (int)jsonInt(pos ? pos->find("character") : nullptr, 0);
  }

  static std::string docUri(const J &params) {
    const J *td = params.find("textDocument");
    return jsonStr(td ? td->find("uri") : nullptr);
  }

  void onInitialize(const J &id) {
    J caps = J::object();
    J sync = J::object();
    sync.set("openClose", J::jbool(true));
    sync.set("change", J::integer(1)); // Full
    caps.set("textDocumentSync", std::move(sync));
    caps.set("completionProvider", J::object());
    caps.set("hoverProvider", J::jbool(true));
    caps.set("definitionProvider", J::jbool(true));
    caps.set("documentSymbolProvider", J::jbool(true));
    caps.set("documentFormattingProvider", J::jbool(true));
    caps.set("renameProvider", J::jbool(true));
    caps.set("referencesProvider", J::jbool(true));
    J result = J::object();
    result.set("capabilities", std::move(caps));
    J info = J::object();
    info.set("name", J::string("nusa-lsp"));
    info.set("version", J::string(NUSA_VERSION));
    result.set("serverInfo", std::move(info));
    sendResponse(id, std::move(result));
  }

  void onCompletion(const J &id, const J &params) {
    std::string uri = docUri(params);
    Doc *doc = findDoc(uri);
    if (!doc) {
      sendResponse(id, J::array());
      return;
    }
    Analysis a = analyzeDoc(uri, doc->text);
    J items = completionItems(a, posLine(params), posChar(params));
    sendResponse(id, std::move(items));
  }

  void onHover(const J &id, const J &params) {
    std::string uri = docUri(params);
    Doc *doc = findDoc(uri);
    if (!doc) {
      sendResponse(id, J());
      return;
    }
    Analysis a = analyzeDoc(uri, doc->text);
    J h = hoverAt(a, posLine(params), posChar(params));
    sendResponse(id, std::move(h));
  }

  void onDefinition(const J &id, const J &params) {
    std::string uri = docUri(params);
    Doc *doc = findDoc(uri);
    if (!doc) {
      sendResponse(id, J());
      return;
    }
    Analysis a = analyzeDoc(uri, doc->text);
    J loc = definitionAt(a, posLine(params), posChar(params), uri);
    sendResponse(id, std::move(loc));
  }

  void onSymbols(const J &id, const J &params) {
    std::string uri = docUri(params);
    Doc *doc = findDoc(uri);
    if (!doc) {
      sendResponse(id, J::array());
      return;
    }
    Analysis a = analyzeDoc(uri, doc->text);
    sendResponse(id, documentSymbols(a));
  }

  void onFormatting(const J &id, const J &params) {
    std::string uri = docUri(params);
    Doc *doc = findDoc(uri);
    if (!doc) {
      sendResponse(id, J());
      return;
    }
    FormatResult r = formatSource(uri, doc->text);
    if (!r.ok) {
      sendResponse(id, J());
      return;
    }
    // Satu edit seluruh dokumen.
    int lastLine = (int)doc->text.empty()
                       ? 0
                       : (int)std::count(doc->text.begin(), doc->text.end(),
                                         '\n');
    std::vector<std::string> lines =
        DiagnosticEngine::splitLines(doc->text);
    int lastLen = lines.empty() ? 0 : (int)lines.back().size();
    (void)lastLine;
    J edit = J::object();
    J range = J::object();
    range.set("start", posJson(0, 0));
    range.set("end", posJson((int)lines.size() - 1, lastLen));
    edit.set("range", std::move(range));
    edit.set("newText", J::string(r.text));
    J arr = J::array();
    arr.arr.push_back(std::move(edit));
    sendResponse(id, std::move(arr));
  }

  void onRename(const J &id, const J &params) {
    std::string uri = docUri(params);
    std::string newName = jsonStr(params.find("newName"));
    Doc *doc = findDoc(uri);
    if (!doc) {
      sendResponse(id, J());
      return;
    }
    Analysis a = analyzeDoc(uri, doc->text);
    J r = renameAt(a, posLine(params), posChar(params), newName, uri);
    sendResponse(id, std::move(r));
  }

  void onReferences(const J &id, const J &params) {
    std::string uri = docUri(params);
    Doc *doc = findDoc(uri);
    if (!doc) {
      sendResponse(id, J::array());
      return;
    }
    // context.includeDeclaration (default true bila absen).
    bool includeDecl = true;
    const J *ctx = params.find("context");
    if (ctx) {
      const J *inc = ctx->find("includeDeclaration");
      if (inc && inc->type == J::T::Bool)
        includeDecl = inc->boolean;
    }
    Analysis a = analyzeDoc(uri, doc->text);
    J r = referencesAt(a, posLine(params), posChar(params), uri, includeDecl);
    sendResponse(id, std::move(r));
  }

  void onShutdown(const J &id) {
    shutdownRequested = true;
    sendResponse(id, J());
  }

  void handle(const J &msg) {
    std::string method = jsonStr(msg.find("method"));
    const J *id = msg.find("id");
    const J *params = msg.find("params");
    J noParams = J::object();
    const J &pp = params ? *params : noParams;
    if (method == "initialize") {
      onInitialize(id ? *id : J());
      return;
    }
    if (method == "initialized") {
      return; // notifikasi; tanpa aksi
    }
    if (method == "shutdown") {
      onShutdown(id ? *id : J());
      return;
    }
    if (method == "exit") {
      exitCode = shutdownRequested ? 0 : 1;
      std::exit(exitCode);
    }
    if (method == "textDocument/didOpen") {
      const J *td = pp.find("textDocument");
      std::string uri = jsonStr(td ? td->find("uri") : nullptr);
      std::string text = jsonStr(td ? td->find("text") : nullptr);
      long long ver = jsonInt(td ? td->find("version") : nullptr, 0);
      if (!uri.empty()) {
        docs[uri] = Doc{uri, text, ver};
        publish(uri);
      }
      return;
    }
    if (method == "textDocument/didChange") {
      const J *td = pp.find("textDocument");
      std::string uri = jsonStr(td ? td->find("uri") : nullptr);
      long long ver = jsonInt(td ? td->find("version") : nullptr, 0);
      const J *changes = pp.find("contentChanges");
      if (!uri.empty() && changes && changes->type == J::T::Arr &&
          !changes->arr.empty()) {
        // Sync penuh: pakai teks terakhir.
        const J *last = &changes->arr.back();
        std::string text = jsonStr(last->find("text"));
        docs[uri] = Doc{uri, text, ver};
        publish(uri);
      }
      return;
    }
    if (method == "textDocument/didClose") {
      std::string uri = docUri(pp);
      docs.erase(uri);
      // Bersihkan diagnostics.
      J p = J::object();
      p.set("uri", J::string(uri));
      p.set("diagnostics", J::array());
      sendNotification("textDocument/publishDiagnostics", std::move(p));
      return;
    }
    // Request butuh id; tanpa id = notifikasi tak dikenal (abaikan).
    if (!id) {
      if (method.rfind("$/", 0) != 0 && method != "$/cancelRequest") {
        // diam
      }
      return;
    }
    if (method == "textDocument/completion") {
      onCompletion(*id, pp);
      return;
    }
    if (method == "textDocument/hover") {
      onHover(*id, pp);
      return;
    }
    if (method == "textDocument/definition") {
      onDefinition(*id, pp);
      return;
    }
    if (method == "textDocument/documentSymbol") {
      onSymbols(*id, pp);
      return;
    }
    if (method == "textDocument/formatting") {
      onFormatting(*id, pp);
      return;
    }
    if (method == "textDocument/rename") {
      onRename(*id, pp);
      return;
    }
    if (method == "textDocument/references") {
      onReferences(*id, pp);
      return;
    }
    sendError(*id, -32601, "method not found: " + method);
  }

  void loop() {
    // Biner + teks aman: sinkronkan C++/C stdio.
    std::ios::sync_with_stdio(false);
    std::string body;
    while (readMessage(body)) {
      JsonParser parser{body, 0, ""};
      J msg;
      if (!parser.parse(msg))
        continue; // pesan rusak: abaikan (server tetap hidup)
      handle(msg);
    }
  }
};

} // namespace (anonim)

int run() {
  Server server;
  server.loop();
  return server.exitCode;
}

} // namespace lsp
} // namespace nusa
