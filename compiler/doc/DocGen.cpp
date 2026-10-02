#include "compiler/doc/DocGen.h"
#include "compiler/ast/AST.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"
#include "compiler/diagnostics/Diagnostic.h"

#include <cstdio>

namespace nusa {
namespace {

// Ambil blok `///` yang menempel tepat di atas baris `kwLine`
// (berurutan tanpa jeda; urutan atas→bawah). "" bila tak ada.
std::string docAbove(const std::vector<Comment> &comments, int kwLine) {
  std::vector<std::string> picked;
  int want = kwLine - 1;
  // comments terurut baris menanjak — pindai mundur.
  for (size_t i = comments.size(); i-- > 0;) {
    const Comment &c = comments[i];
    if (!c.doc || c.line != want)
      break;
    std::string line = c.text;
    if (!line.empty() && line[0] == ' ')
      line.erase(0, 1);
    picked.push_back(line);
    want--;
  }
  std::string out;
  for (size_t i = picked.size(); i-- > 0;) {
    if (!out.empty())
      out += "\n";
    out += picked[i];
  }
  return out;
}

std::string signatureOf(const FunctionDecl &fn) {
  std::string s = fn.name;
  if (!fn.tparams.empty()) {
    s += "<";
    for (size_t i = 0; i < fn.tparams.size(); i++) {
      if (i)
        s += ", ";
      s += fn.tparams[i];
    }
    s += ">";
  }
  s += "(";
  for (size_t i = 0; i < fn.params.size(); i++) {
    if (i)
      s += ", ";
    // Receiver `self` (Phase 22): marker tipe tak ditampilkan.
    const Param &p = fn.params[i];
    if (p.name == "self" && p.type.kind == TypeKind::Struct &&
        p.type.sname && *p.type.sname == "self" && p.type.sargs.empty())
      s += "self";
    else
      s += p.name + ": " + p.type.name();
  }
  s += ") -> " + fn.retType.name();
  return s;
}

} // namespace

// Prolog bersama Markdown + JSON: lex + parse (atau error pertama,
// dengan format error yang sama seperti semula).
static bool parseDocSource(const std::string &file, const std::string &source,
                           std::vector<Comment> &comments, ParseResult &pres,
                           std::string &error) {
  Lexer lexer(file, source);
  std::vector<Token> tokens = lexer.tokenize();
  if (!lexer.errors().empty()) {
    error = lexer.errors().front();
    return false;
  }
  comments = lexer.comments();
  Parser parser(file, std::move(tokens),
                DiagnosticEngine::splitLines(source));
  pres = parser.parse();
  if (!pres.ok()) {
    const ParseError &e = pres.errors.front();
    error = e.loc.file + ":" + std::to_string(e.loc.line) + ":" +
            std::to_string(e.loc.column) + ": error: " + e.message;
    return false;
  }
  return true;
}

DocResult generateDoc(const std::string &file, const std::string &source) {
  DocResult r;
  std::vector<Comment> comments;
  ParseResult pres;
  if (!parseDocSource(file, source, comments, pres, r.error)) {
    r.ok = false;
    return r;
  }

  std::string md = "# API — " + file + "\n";
  // Module intro: blok `///` di awal file yang dipisah baris kosong
  // dari item pertama (bila menempel item, ia milik item itu).
  int firstItemLine = 1 << 30;
  for (const auto &fn : pres.program.functions) {
    if (fn.loc.line < firstItemLine)
      firstItemLine = fn.loc.line;
  }
  for (const auto &sd : pres.program.structs) {
    if (sd.loc.line < firstItemLine)
      firstItemLine = sd.loc.line;
  }
  for (const auto &ed : pres.program.enums) {
    if (ed.loc.line < firstItemLine)
      firstItemLine = ed.loc.line;
  }
  {
    // Blok `///` di awal file (mulai baris 1, berurutan).
    std::vector<std::string> intro;
    int expect = 1;
    for (const auto &c : comments) {
      if (!c.doc || c.line != expect)
        break;
      std::string line = c.text;
      if (!line.empty() && line[0] == ' ')
        line.erase(0, 1);
      intro.push_back(line);
      expect++;
    }
    // Intro bila tanpa item, atau dipisah baris kosong dari item pertama
    // (bila menempel, blok milik item itu via docAbove).
    bool separated =
        !intro.empty() && (firstItemLine == (1 << 30) || expect < firstItemLine);
    if (separated) {
      md += "\n";
      for (const auto &line : intro)
        md += line + "\n";
    }
  }
  bool hasFn = false;
  for (const auto &fn : pres.program.functions) {
    if (!fn.isExtern) {
      hasFn = true;
      break;
    }
  }
  if (hasFn) {
    md += "\n## Functions\n";
    for (const auto &fn : pres.program.functions) {
      if (fn.isExtern)
        continue; // ----> § Extern Functions di bawah.
      md += "\n### `" + signatureOf(fn) + "`\n\n";
      std::string doc = docAbove(comments, fn.loc.line);
      md += doc.empty() ? "*No documentation.*\n" : doc + "\n";
    }
  }
  if (!pres.program.structs.empty()) {
    md += "\n## Structs\n";
    for (const auto &sd : pres.program.structs) {
      std::string head = "struct " + sd.name;
      if (!sd.tparams.empty()) {
        head += "<";
        for (size_t i = 0; i < sd.tparams.size(); i++) {
          if (i)
            head += ", ";
          head += sd.tparams[i];
        }
        head += ">";
      }
      md += "\n### `" + head + "`\n\n";
      std::string doc = docAbove(comments, sd.loc.line);
      md += doc.empty() ? "*No documentation.*\n" : doc + "\n";
      if (!sd.fields.empty()) {
        md += "\n| Field | Type |\n|---|---|\n";
        for (const auto &f : sd.fields)
          md += "| " + f.name + " | " + f.type.name() + " |\n";
      }
    }
  }
  if (!pres.program.aliases.empty()) {
    md += "\n## Type Aliases\n";
    for (const auto &ad : pres.program.aliases) {
      md += "\n### `type " + ad.name + " = " + ad.target.name() + "`\n\n";
      std::string doc = docAbove(comments, ad.loc.line);
      md += doc.empty() ? "*No documentation.*\n" : doc + "\n";
    }
  }
  if (!pres.program.consts.empty()) {
    md += "\n## Constants\n";
    for (const auto &cd : pres.program.consts) {
      std::string head = "const " + cd.name;
      if (cd.declaredType)
        head += ": " + cd.declaredType->name();
      md += "\n### `" + head + "`\n\n";
      std::string doc = docAbove(comments, cd.loc.line);
      md += doc.empty() ? "*No documentation.*\n" : doc + "\n";
    }
  }
  if (!pres.program.enums.empty()) {
    md += "\n## Enums\n";
    for (const auto &ed : pres.program.enums) {
      std::string head = "enum " + ed.name;
      if (!ed.tparams.empty()) {
        head += "<";
        for (size_t i = 0; i < ed.tparams.size(); i++) {
          if (i)
            head += ", ";
          head += ed.tparams[i];
        }
        head += ">";
      }
      md += "\n### `" + head + "`\n\n";
      std::string doc = docAbove(comments, ed.loc.line);
      md += doc.empty() ? "*No documentation.*\n" : doc + "\n";
      md += "\n| Variant | Payload |\n|---|---|\n";
      for (const auto &v : ed.variants) {
        std::string pay;
        for (size_t i = 0; i < v.payload.size(); i++) {
          if (i)
            pay += ", ";
          pay += v.payload[i].name();
        }
        md += "| " + v.name + " | " + (pay.empty() ? "—" : pay) + " |\n";
      }
    }
  }
  if (!pres.program.traits.empty()) {    md += "\n## Traits\n";
    for (const auto &td : pres.program.traits) {
      md += "\n### `trait " + td.name + "`\n\n";
      std::string doc = docAbove(comments, td.loc.line);
      md += doc.empty() ? "*No documentation.*\n" : doc + "\n";
      for (const auto &m : td.methods)
        md += "\n- `" + signatureOf(m) + "`\n";
    }
  }
  bool hasExtern = false;
  for (const auto &fn : pres.program.functions) {
    if (fn.isExtern) {
      hasExtern = true;
      break;
    }
  }
  if (hasExtern) {
    md += "\n## Extern Functions\n";
    for (const auto &fn : pres.program.functions) {
      if (!fn.isExtern)
        continue;
      md += "\n### `extern " + signatureOf(fn) + "`\n\n";
      std::string doc = docAbove(comments, fn.loc.line);
      md += doc.empty() ? "*No documentation.*\n" : doc + "\n";
    }
  }
  if (!hasFn && !hasExtern && pres.program.structs.empty() &&
      pres.program.aliases.empty() && pres.program.traits.empty() &&
      pres.program.enums.empty() && pres.program.consts.empty())
    md += "\n_No functions, structs, type aliases, traits, or enums found._\n";
  r.markdown = md;
  return r;
}

namespace {
// JSON minimal deterministik (urutan = urutan sumber).
void jStr(std::string &o, const std::string &s) {
  o.push_back('"');
  for (unsigned char c : s) {
    switch (c) {
    case '"': o += "\\\""; break;
    case '\\': o += "\\\\"; break;
    case '\n': o += "\\n"; break;
    case '\t': o += "\\t"; break;
    case '\r': o += "\\r"; break;
    default:
      if (c < 0x20) {
        char b[7];
        std::snprintf(b, sizeof(b), "\\u%04x", c);
        o += b;
      } else {
        o.push_back(static_cast<char>(c));
      }
    }
  }
  o.push_back('"');
}
void jFn(std::string &o, const FunctionDecl &fn,
         const std::vector<Comment> &comments, bool isExtern) {
  o += "{\"name\":";
  jStr(o, fn.name);
  o += ",\"tparams\":[";
  for (size_t i = 0; i < fn.tparams.size(); i++) {
    if (i)
      o.push_back(',');
    jStr(o, fn.tparams[i]);
  }
  o += "],\"params\":[";
  for (size_t i = 0; i < fn.params.size(); i++) {
    if (i)
      o.push_back(',');
    o += "{\"name\":";
    jStr(o, fn.params[i].name);
    o += ",\"type\":";
    jStr(o, fn.params[i].type.name());
    o += "}";
  }
  o += "],\"ret\":";
  jStr(o, fn.retType.name());
  o += ",\"extern\":";
  o += isExtern ? "true" : "false";
  o += ",\"doc\":";
  jStr(o, docAbove(comments, fn.loc.line));
  o += "}";
}
} // namespace

std::string generateDocJson(const std::string &file, const std::string &source,
                            bool &ok, std::string &error) {
  std::vector<Comment> comments;
  ParseResult pres;
  if (!parseDocSource(file, source, comments, pres, error)) {
    ok = false;
    return "";
  }
  ok = true;
  std::string o = "{\"file\":";
  jStr(o, file);
  o += ",\"functions\":[";
  bool first = true;
  for (const auto &fn : pres.program.functions) {
    if (fn.isExtern)
      continue;
    if (!first)
      o.push_back(',');
    first = false;
    jFn(o, fn, comments, false);
  }
  o += "],\"structs\":[";
  first = true;
  for (const auto &sd : pres.program.structs) {
    if (!first)
      o.push_back(',');
    first = false;
    o += "{\"name\":";
    jStr(o, sd.name);
    o += ",\"tparams\":[";
    for (size_t i = 0; i < sd.tparams.size(); i++) {
      if (i)
        o.push_back(',');
      jStr(o, sd.tparams[i]);
    }
    o += "],\"fields\":[";
    for (size_t i = 0; i < sd.fields.size(); i++) {
      if (i)
        o.push_back(',');
      o += "{\"name\":";
      jStr(o, sd.fields[i].name);
      o += ",\"type\":";
      jStr(o, sd.fields[i].type.name());
      o += "}";
    }
    o += "],\"doc\":";
    jStr(o, docAbove(comments, sd.loc.line));
    o += "}";
  }
  o += "],\"aliases\":[";
  first = true;
  for (const auto &ad : pres.program.aliases) {
    if (!first)
      o.push_back(',');
    first = false;
    o += "{\"name\":";
    jStr(o, ad.name);
    o += ",\"target\":";
    jStr(o, ad.target.name());
    o += ",\"doc\":";
    jStr(o, docAbove(comments, ad.loc.line));
    o += "}";
  }
  o += "],\"consts\":[";
  first = true;
  for (const auto &cd : pres.program.consts) {
    if (!first)
      o.push_back(',');
    first = false;
    o += "{\"name\":";
    jStr(o, cd.name);
    o += ",\"type\":";
    if (cd.declaredType)
      jStr(o, cd.declaredType->name());
    else
      o += "null";
    o += ",\"doc\":";
    jStr(o, docAbove(comments, cd.loc.line));
    o += "}";
  }
  o += "],\"enums\":[";
  first = true;
  for (const auto &ed : pres.program.enums) {
    if (!first)
      o.push_back(',');
    first = false;
    o += "{\"name\":";
    jStr(o, ed.name);
    o += ",\"tparams\":[";
    for (size_t i = 0; i < ed.tparams.size(); i++) {
      if (i)
        o.push_back(',');
      jStr(o, ed.tparams[i]);
    }
    o += "],\"variants\":[";
    for (size_t i = 0; i < ed.variants.size(); i++) {
      if (i)
        o.push_back(',');
      o += "{\"name\":";
      jStr(o, ed.variants[i].name);
      o += ",\"payload\":[";
      for (size_t k = 0; k < ed.variants[i].payload.size(); k++) {
        if (k)
          o.push_back(',');
        jStr(o, ed.variants[i].payload[k].name());
      }
      o += "]}";
    }
    o += "],\"doc\":";
    jStr(o, docAbove(comments, ed.loc.line));
    o += "}";
  }
  o += "],\"traits\":[";
  first = true;
  for (const auto &td : pres.program.traits) {
    if (!first)
      o.push_back(',');
    first = false;
    o += "{\"name\":";
    jStr(o, td.name);
    o += ",\"methods\":[";
    for (size_t i = 0; i < td.methods.size(); i++) {
      if (i)
        o.push_back(',');
      jFn(o, td.methods[i], comments, false);
    }
    o += "],\"doc\":";
    jStr(o, docAbove(comments, td.loc.line));
    o += "}";
  }
  o += "],\"externs\":[";
  first = true;
  for (const auto &fn : pres.program.functions) {
    if (!fn.isExtern)
      continue;
    if (!first)
      o.push_back(',');
    first = false;
    jFn(o, fn, comments, true);
  }
  o += "]}";
  return o;
}

} // namespace nusa
