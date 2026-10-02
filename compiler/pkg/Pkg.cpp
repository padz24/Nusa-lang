// Package manager Nusa (Phase 30, v1.11.0). Lihat Pkg.h.
#include "compiler/pkg/Pkg.h"
#include "compiler/driver/Driver.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <sstream>

namespace nusa {
namespace pkg {
namespace fs = std::filesystem;

std::string compilerVersion() { return std::string(NUSA_VERSION); }

// ---------- util ----------

static std::string trim(const std::string &s) {
  size_t a = 0;
  while (a < s.size() && std::isspace((unsigned char)s[a]))
    a++;
  size_t b = s.size();
  while (b > a && std::isspace((unsigned char)s[b - 1]))
    b--;
  return s.substr(a, b - a);
}

static bool readFile(const std::string &path, std::string &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

static bool writeFile(const std::string &path, const std::string &data,
                      std::string &err) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    err = "cannot write '" + path + "'";
    return false;
  }
  f << data;
  if (!f) {
    err = "cannot write '" + path + "'";
    return false;
  }
  return true;
}

static bool validName(const std::string &n) {
  if (n.empty() || !(std::isalpha((unsigned char)n[0]) || n[0] == '_'))
    return false;
  for (char c : n) {
    if (!(std::isalnum((unsigned char)c) || c == '_'))
      return false;
  }
  return true;
}

static std::string shellQ(const std::string &s) {
  std::string o = "'";
  for (char c : s) {
    if (c == '\'')
      o += "'\\''";
    else
      o.push_back(c);
  }
  return o + "'";
}

// ---------- TOML subset ----------
// Mendukung: # komentar, [section], [[array-section]], key = value;
// value: "string" (\" \\ \n \t), integer, true/false,
// ["a", "b"], { k = v, ... } (satu level).

struct TomlValue {
  enum class Kind { Str, Int, Bool, Arr, Table };
  Kind kind = Kind::Str;
  std::string str;
  int64_t num = 0;
  bool boolean = false;
  std::vector<std::string> arr;
  std::vector<std::pair<std::string, TomlValue>> table;
};

struct TomlDoc {
  // section -> (key -> value); array-section -> daftar map.
  std::vector<std::pair<std::string,
                        std::vector<std::pair<std::string, TomlValue>>>>
      sections;
  std::vector<std::pair<
      std::string, std::vector<std::pair<std::string, TomlValue>>>>
      arrays;
};

static bool parseTomlValue(const std::string &s, size_t &i, TomlValue &out,
                           std::string &err);

static void skipWs(const std::string &s, size_t &i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    i++;
}

static bool parseTomlString(const std::string &s, size_t &i, std::string &out,
                            std::string &err) {
  // s[i] == '"'.
  i++;
  std::string o;
  while (i < s.size() && s[i] != '"') {
    if (s[i] == '\\' && i + 1 < s.size()) {
      char e = s[i + 1];
      if (e == 'n')
        o.push_back('\n');
      else if (e == 't')
        o.push_back('\t');
      else
        o.push_back(e);
      i += 2;
    } else {
      o.push_back(s[i++]);
    }
  }
  if (i >= s.size()) {
    err = "unterminated string";
    return false;
  }
  i++; // '"'
  out = o;
  return true;
}

static bool parseTomlValue(const std::string &s, size_t &i, TomlValue &out,
                           std::string &err) {
  skipWs(s, i);
  if (i >= s.size()) {
    err = "expected value";
    return false;
  }
  if (s[i] == '"') {
    out.kind = TomlValue::Kind::Str;
    return parseTomlString(s, i, out.str, err);
  }
  if (s[i] == '[') {
    out.kind = TomlValue::Kind::Arr;
    i++;
    while (true) {
      skipWs(s, i);
      if (i < s.size() && s[i] == ']') {
        i++;
        return true;
      }
      if (i >= s.size() || s[i] != '"') {
        err = "arrays support string elements only";
        return false;
      }
      std::string e;
      if (!parseTomlString(s, i, e, err))
        return false;
      out.arr.push_back(e);
      skipWs(s, i);
      if (i < s.size() && s[i] == ',') {
        i++;
        continue;
      }
      skipWs(s, i);
      if (i < s.size() && s[i] == ']') {
        i++;
        return true;
      }
      err = "expected ',' or ']' in array";
      return false;
    }
  }
  if (s[i] == '{') {
    out.kind = TomlValue::Kind::Table;
    i++;
    while (true) {
      skipWs(s, i);
      if (i < s.size() && s[i] == '}') {
        i++;
        return true;
      }
      size_t ks = i;
      while (i < s.size() &&
             (std::isalnum((unsigned char)s[i]) || s[i] == '_' ||
              s[i] == '-'))
        i++;
      if (ks == i) {
        err = "expected key in inline table";
        return false;
      }
      std::string key = s.substr(ks, i - ks);
      skipWs(s, i);
      if (i >= s.size() || s[i] != '=') {
        err = "expected '=' in inline table";
        return false;
      }
      i++;
      TomlValue v;
      if (!parseTomlValue(s, i, v, err))
        return false;
      out.table.emplace_back(key, v);
      skipWs(s, i);
      if (i < s.size() && s[i] == ',') {
        i++;
        continue;
      }
      skipWs(s, i);
      if (i < s.size() && s[i] == '}') {
        i++;
        return true;
      }
      err = "expected ',' or '}' in inline table";
      return false;
    }
  }
  size_t ks = i;
  while (i < s.size() && s[i] != '#' && s[i] != '\n')
    i++;
  std::string tok = trim(s.substr(ks, i - ks));
  if (tok == "true" || tok == "false") {
    out.kind = TomlValue::Kind::Bool;
    out.boolean = (tok == "true");
    return true;
  }
  // Integer desimal (manual: tanpa exceptions di build ini).
  if (!tok.empty()) {
    size_t d = 0;
    bool neg = false;
    if (tok[0] == '-' || tok[0] == '+') {
      neg = (tok[0] == '-');
      d = 1;
    }
    int64_t v = 0;
    bool any = false;
    bool bad = false;
    for (; d < tok.size(); d++) {
      if (!std::isdigit((unsigned char)tok[d])) {
        bad = true;
        break;
      }
      any = true;
      v = v * 10 + (tok[d] - '0');
    }
    if (any && !bad) {
      out.kind = TomlValue::Kind::Int;
      out.num = neg ? -v : v;
      return true;
    }
  }
  err = "invalid value '" + tok + "'";
  return false;
}

static bool parseTomlDoc(const std::string &text, TomlDoc &doc,
                         std::string &err) {
  std::istringstream in(text);
  std::string line;
  int lineno = 0;
  std::string cur;
  bool curIsArray = false;
  auto section = [&](const std::string &name, bool isArray)
      -> std::vector<std::pair<std::string, TomlValue>> * {
    if (isArray) {
      doc.arrays.emplace_back(
          name, std::vector<std::pair<std::string, TomlValue>>{});
      return &doc.arrays.back().second;
    }
    for (auto &s : doc.sections) {
      if (s.first == name)
        return &s.second;
    }
    doc.sections.emplace_back(
        name, std::vector<std::pair<std::string, TomlValue>>{});
    return &doc.sections.back().second;
  };
  std::vector<std::pair<std::string, TomlValue>> *tgt = nullptr;
  while (std::getline(in, line)) {
    lineno++;
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    // Potong komentar (di luar string).
    std::string code;
    bool inStr = false;
    for (size_t i = 0; i < line.size(); i++) {
      if (inStr) {
        code.push_back(line[i]);
        if (line[i] == '\\' && i + 1 < line.size()) {
          code.push_back(line[i + 1]);
          i++;
        } else if (line[i] == '"') {
          inStr = false;
        }
      } else if (line[i] == '"') {
        inStr = true;
        code.push_back(line[i]);
      } else if (line[i] == '#') {
        break;
      } else {
        code.push_back(line[i]);
      }
    }
    std::string t = trim(code);
    if (t.empty())
      continue;
    if (t[0] == '[') {
      bool isArr = t.size() > 1 && t[1] == '[';
      size_t close = t.rfind(isArr ? "]]" : "]");
      if (close == std::string::npos) {
        err = "line " + std::to_string(lineno) + ": bad section header";
        return false;
      }
      cur = trim(t.substr(isArr ? 2 : 1, close - (isArr ? 2 : 1)));
      curIsArray = isArr;
      tgt = section(cur, isArr);
      continue;
    }
    size_t eq = t.find('=');
    if (eq == std::string::npos) {
      err = "line " + std::to_string(lineno) + ": expected key = value";
      return false;
    }
    std::string key = trim(t.substr(0, eq));
    if (key.empty()) {
      err = "line " + std::to_string(lineno) + ": empty key";
      return false;
    }
    if (!tgt) {
      err = "line " + std::to_string(lineno) +
            ": key outside any section (need [package])";
      return false;
    }
    size_t i = eq + 1;
    TomlValue v;
    if (!parseTomlValue(t, i, v, err)) {
      err = "line " + std::to_string(lineno) + ": " + err;
      return false;
    }
    tgt->emplace_back(key, v);
  }
  (void)curIsArray;
  (void)cur;
  return true;
}

static const TomlValue *
findKey(const std::vector<std::pair<std::string, TomlValue>> &m,
        const std::string &key) {
  for (const auto &kv : m) {
    if (kv.first == key)
      return &kv.second;
  }
  return nullptr;
}

static const std::vector<std::pair<std::string, TomlValue>> *
findSection(const TomlDoc &doc, const std::string &name) {
  for (const auto &s : doc.sections) {
    if (s.first == name)
      return &s.second;
  }
  return nullptr;
}

// ---------- manifest ----------

static std::string escStr(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\t')
      o += "\\t";
    else
      o.push_back(c);
  }
  return o;
}

bool parseManifest(const std::string &path, Manifest &out, std::string &err) {
  std::string text;
  if (!readFile(path, text)) {
    err = "cannot read '" + path + "'";
    return false;
  }
  TomlDoc doc;
  if (!parseTomlDoc(text, doc, err)) {
    err = path + ": " + err;
    return false;
  }
  const auto *pkg = findSection(doc, "package");
  if (!pkg) {
    err = path + ": missing [package] section";
    return false;
  }
  auto getStr = [&](const char *key, std::string &dst,
                    bool required) -> bool {
    const TomlValue *v = findKey(*pkg, key);
    if (!v) {
      if (required) {
        err = std::string(path) + ": [package] missing '" + key + "'";
        return false;
      }
      return true;
    }
    if (v->kind != TomlValue::Kind::Str) {
      err = std::string(path) + ": [package] '" + key + "' must be a string";
      return false;
    }
    dst = v->str;
    return true;
  };
  if (!getStr("name", out.name, true) ||
      !getStr("version", out.version, true) ||
      !getStr("compiler", out.compiler, false) ||
      !getStr("target", out.target, false) ||
      !getStr("description", out.description, false) ||
      !getStr("entry", out.entry, false))
    return false;
  if (!validName(out.name)) {
    err = path + ": invalid package name '" + out.name + "'";
    return false;
  }
  int a, b, c;
  std::string pre;
  if (!parseSemver(out.version, a, b, c, pre)) {
    err = path + ": invalid version '" + out.version + "' (want M.m.p)";
    return false;
  }
  const TomlValue *fv = findKey(*pkg, "features");
  if (fv) {
    if (fv->kind != TomlValue::Kind::Arr) {
      err = path + ": [package] 'features' must be an array of strings";
      return false;
    }
    out.features = fv->arr;
  }
  const auto *deps = findSection(doc, "dependencies");
  // [capabilities] opsional (Phase 59): bool per izin, default false;
  // key asing ditolak (typo fail-closed).
  const auto *caps = findSection(doc, "capabilities");
  if (caps) {
    auto getCap = [&](const char *key, bool &dst) -> bool {
      const TomlValue *v = findKey(*caps, key);
      if (!v)
        return true;
      if (v->kind != TomlValue::Kind::Bool) {
        err = path + ": [capabilities] '" + key + "' must be true/false";
        return false;
      }
      dst = v->boolean;
      return true;
    };
    if (!getCap("filesystem", out.caps.filesystem) ||
        !getCap("network", out.caps.network) ||
        !getCap("process", out.caps.process) ||
        !getCap("environment", out.caps.environment) ||
        !getCap("native", out.caps.native))
      return false;
    for (const auto &kv : *caps) {
      if (kv.first != "filesystem" && kv.first != "network" &&
          kv.first != "process" && kv.first != "environment" &&
          kv.first != "native") {
        err = path + ": unknown capability '" + kv.first + "'";
        return false;
      }
    }
  }
  // [link] opsional (Phase 32): libraries/library-dirs/files (array).
  const auto *link = findSection(doc, "link");
  if (link) {
    auto getArr = [&](const char *key,
                      std::vector<std::string> &dst) -> bool {
      const TomlValue *v = findKey(*link, key);
      if (!v)
        return true;
      if (v->kind != TomlValue::Kind::Arr) {
        err = path + ": [link] '" + key + "' must be an array of strings";
        return false;
      }
      dst = v->arr;
      return true;
    };
    if (!getArr("libraries", out.linkLibs) ||
        !getArr("library-dirs", out.linkLibDirs) ||
        !getArr("files", out.linkFiles))
      return false;
  }
  if (deps) {
    for (const auto &kv : *deps) {
      if (!validName(kv.first)) {
        err = path + ": invalid dependency name '" + kv.first + "'";
        return false;
      }
      Dep d;
      d.name = kv.first;
      const TomlValue &v = kv.second;
      if (v.kind == TomlValue::Kind::Str) {
        // Gula: `foo = "1.2.3"` = versi registry.
        d.version = v.str;
      } else if (v.kind == TomlValue::Kind::Table) {
        bool hasPath = false, hasVer = false;
        for (const auto &tk : v.table) {
          if (tk.first == "path" &&
              tk.second.kind == TomlValue::Kind::Str) {
            d.path = tk.second.str;
            hasPath = true;
          } else if (tk.first == "version" &&
                     tk.second.kind == TomlValue::Kind::Str) {
            d.version = tk.second.str;
            hasVer = true;
          } else {
            err = path + ": dependency '" + d.name +
                  "': want { path = ... } or { version = ... }";
            return false;
          }
        }
        if (hasPath == hasVer) {
          err = path + ": dependency '" + d.name +
                "': exactly one of 'path'/'version' required";
          return false;
        }
      } else {
        err = path + ": dependency '" + d.name +
              "': want \"version\" or { path/version }";
        return false;
      }
      out.deps.push_back(d);
    }
  }
  size_t slash = path.find_last_of("/\\");
  out.dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
  if (out.dir.empty())
    out.dir = ".";
  return true;
}

bool writeManifest(const std::string &path, const Manifest &m,
                   std::string &err, bool emitCaps) {
  std::string o = "[package]\n";
  o += "name = \"" + escStr(m.name) + "\"\n";
  o += "version = \"" + escStr(m.version) + "\"\n";
  if (!m.compiler.empty())
    o += "compiler = \"" + escStr(m.compiler) + "\"\n";
  if (!m.target.empty())
    o += "target = \"" + escStr(m.target) + "\"\n";
  if (!m.description.empty())
    o += "description = \"" + escStr(m.description) + "\"\n";
  if (!m.features.empty()) {
    o += "features = [";
    for (size_t i = 0; i < m.features.size(); i++) {
      if (i)
        o += ", ";
      o += "\"" + escStr(m.features[i]) + "\"";
    }
    o += "]\n";
  }
  if (!m.entry.empty())
    o += "entry = \"" + escStr(m.entry) + "\"\n";
  if (!m.linkLibs.empty() || !m.linkLibDirs.empty() || !m.linkFiles.empty()) {
    o += "\n[link]\n";
    auto emitArr = [&](const char *key, const std::vector<std::string> &v) {
      if (v.empty())
        return;
      o += key;
      o += std::string(" = [");
      for (size_t i = 0; i < v.size(); i++) {
        if (i)
          o += ", ";
        o += "\"" + escStr(v[i]) + "\"";
      }
      o += "]\n";
    };
    emitArr("libraries", m.linkLibs);
    emitArr("library-dirs", m.linkLibDirs);
    emitArr("files", m.linkFiles);
  }
  // [capabilities] (Phase 59): bila ada yang true, atau bila
  // pemanggil meminta eksplisit (init: deklarasi terlihat). Hindari
  // churn manifest lama pada update biasa.
  if (emitCaps || m.caps.filesystem || m.caps.network || m.caps.process ||
      m.caps.environment || m.caps.native) {
    auto emitCap = [&](const char *key, bool v) {
      o += key;
      o += v ? std::string(" = true\n") : std::string(" = false\n");
    };
    o += "\n[capabilities]\n";
    emitCap("filesystem", m.caps.filesystem);
    emitCap("network", m.caps.network);
    emitCap("process", m.caps.process);
    emitCap("environment", m.caps.environment);
    emitCap("native", m.caps.native);
  }
  o += "\n[dependencies]\n";
  for (const auto &d : m.deps) {
    if (d.isPath())
      o += d.name + " = { path = \"" + escStr(d.path) + "\" }\n";
    else
      o += d.name + " = \"" + escStr(d.version) + "\"\n";
  }
  return writeFile(path, o, err);
}

std::string capabilitiesSummary(const Manifest &m) {
  std::string s;
  auto add = [&](const char *key, bool v) {
    if (v) {
      if (!s.empty())
        s += " ";
      s += key;
    }
  };
  add("filesystem", m.caps.filesystem);
  add("network", m.caps.network);
  add("process", m.caps.process);
  add("environment", m.caps.environment);
  add("native", m.caps.native);
  return s.empty() ? "(none)" : s;
}

bool parseLock(const std::string &path, std::vector<LockPkg> &out,
               std::string &err) {
  return parseLockFull(path, out, nullptr, err);
}

bool parseLockFull(const std::string &path, std::vector<LockPkg> &out,
                   std::string *lockCompiler, std::string &err);

// [meta] compiler pin (Phase 61): versi compiler eksak saat lock
// ditulis. Lock pra-2.12 tanpa [meta] → pin kosong (basi).
static bool parseLockMetaImpl(const TomlDoc &doc, std::string &compiler) {
  const auto *meta = findSection(doc, "meta");
  if (!meta)
    return true;
  const TomlValue *v = nullptr;
  for (const auto &kv : *meta) {
    if (kv.first == "compiler") {
      v = &kv.second;
      break;
    }
  }
  if (!v || v->kind != TomlValue::Kind::Str)
    return false;
  compiler = v->str;
  return true;
}

bool parseLockFull(const std::string &path, std::vector<LockPkg> &out,
                   std::string *lockCompiler, std::string &err) {
  std::string text;
  if (!readFile(path, text)) {
    err = "cannot read '" + path + "'";
    return false;
  }
  TomlDoc doc;
  if (!parseTomlDoc(text, doc, err)) {
    err = path + ": " + err;
    return false;
  }
  for (const auto &arr : doc.arrays) {
    if (arr.first != "package")
      continue;
    LockPkg p;
    auto get = [&](const char *k, std::string &dst,
                   bool required) -> bool {
      const TomlValue *v = findKey(arr.second, k);
      if (!v) {
        if (required) {
          err = std::string(path) + ": lock [[package]] missing '" + k +
                "'";
          return false;
        }
        dst.clear();
        return true;
      }
      if (v->kind != TomlValue::Kind::Str) {
        err = std::string(path) + ": lock [[package]] '" + k +
              "' must be a string";
        return false;
      }
      dst = v->str;
      return true;
    };
    if (!get("name", p.name, true) || !get("version", p.version, true) ||
        !get("path", p.path, true) || !get("registry", p.registry, false) ||
        !get("checksum", p.checksum, true))
      return false;
    out.push_back(p);
  }
  if (lockCompiler) {
    lockCompiler->clear();
    if (!parseLockMetaImpl(doc, *lockCompiler)) {
      err = path + ": lock [meta] 'compiler' must be a string";
      return false;
    }
  }
  return true;
}

bool writeLock(const std::string &path, const std::vector<LockPkg> &pkgs,
               std::string &err) {
  std::vector<LockPkg> sorted = pkgs;
  std::sort(sorted.begin(), sorted.end(),
            [](const LockPkg &a, const LockPkg &b) {
              return a.name < b.name;
            });
  std::string o = "# GENERATED by `nusa pkg` — do not edit.\n";
  o += "\n[meta]\n";
  o += "compiler = \"" + escStr(compilerVersion()) + "\"\n";
  for (const auto &p : sorted) {
    o += "\n[[package]]\n";
    o += "name = \"" + escStr(p.name) + "\"\n";
    o += "version = \"" + escStr(p.version) + "\"\n";
    o += "path = \"" + escStr(p.path) + "\"\n";
    if (!p.registry.empty())
      o += "registry = \"" + escStr(p.registry) + "\"\n";
    o += "checksum = \"" + escStr(p.checksum) + "\"\n";
  }
  return writeFile(path, o, err);
}

// ---------- semver ----------

bool parseSemver(const std::string &s, int &maj, int &min, int &patch,
                 std::string &pre) {
  pre.clear();
  std::string core = s;
  size_t dash = s.find('-');
  if (dash != std::string::npos) {
    core = s.substr(0, dash);
    pre = s.substr(dash + 1);
    if (pre.empty())
      return false;
    for (char ch : pre) {
      if (!(std::isalnum((unsigned char)ch) || ch == '.' || ch == '-'))
        return false;
    }
  }
  std::vector<int> parts;
  std::string cur;
  for (size_t i = 0; i <= core.size(); i++) {
    if (i == core.size() || core[i] == '.') {
      if (cur.empty())
        return false;
      int v = 0;
      for (char ch : cur) {
        if (!std::isdigit((unsigned char)ch))
          return false;
        v = v * 10 + (ch - '0');
      }
      parts.push_back(v);
      cur.clear();
    } else {
      cur.push_back(core[i]);
    }
  }
  if (parts.size() != 3)
    return false;
  maj = parts[0];
  min = parts[1];
  patch = parts[2];
  return true;
}

int cmpSemver(const std::string &a, const std::string &b) {
  int a1, a2, a3, b1, b2, b3;
  std::string ap, bp;
  bool aok = parseSemver(a, a1, a2, a3, ap);
  bool bok = parseSemver(b, b1, b2, b3, bp);
  if (!aok || !bok)
    return aok ? 1 : (bok ? -1 : 0);
  if (a1 != b1)
    return a1 < b1 ? -1 : 1;
  if (a2 != b2)
    return a2 < b2 ? -1 : 1;
  if (a3 != b3)
    return a3 < b3 ? -1 : 1;
  if (ap == bp)
    return 0;
  if (ap.empty())
    return 1; // rilis > pre-rilis
  if (bp.empty())
    return -1;
  return ap < bp ? -1 : 1;
}

bool satisfiesMin(const std::string &have, const std::string &minimum) {
  return cmpSemver(have, minimum) >= 0;
}

// ---------- checksum ----------

std::string sha256Hex(const std::string &data) {
  std::array<uint8_t, 32> raw = llvm::SHA256::hash(llvm::ArrayRef<uint8_t>(
      (const uint8_t *)data.data(), data.size()));
  static const char *hex = "0123456789abcdef";
  std::string o;
  for (uint8_t byte : raw) {
    o.push_back(hex[byte >> 4]);
    o.push_back(hex[byte & 0xF]);
  }
  return o;
}

// Validator isi paket pasca-unpack (Phase 59): direktori paket yang
// aman = HANYA file reguler top-level bernama waras. Tolak
// subdirektori, symlink/tipe-khusus, nama kosong/`.`/`..`/berawalan
// `.`/mengandung `/` atau `\`. False + nama pelanggar di `badName`.
bool packageDirSafe(const std::string &dir, std::string &badName) {
  std::error_code ec;
  for (const auto &ent : fs::directory_iterator(dir, ec)) {
    if (ec)
      return false;
    std::string nm = ent.path().filename().string();
    auto st = fs::symlink_status(ent.path(), ec);
    bool regular = !ec && fs::is_regular_file(st);
    bool sane = !nm.empty() && nm != "." && nm != ".." && nm[0] != '.' &&
                nm.find('/') == std::string::npos &&
                nm.find('\\') == std::string::npos;
    if (!regular || !sane) {
      badName = nm;
      return false;
    }
  }
  return !ec;
}

std::string dirChecksum(const std::string &dir) {
  // File *.ns + nusa.toml top-level, terurut nama (reproducible).
  std::error_code ec;
  std::vector<std::string> names;
  for (auto it = fs::directory_iterator(dir, ec);
       !ec && it != fs::directory_iterator(); ++it) {
    if (!it->is_regular_file(ec) || ec)
      continue;
    std::string fn = it->path().filename().string();
    if (fn == "nusa.toml" ||
        (fn.size() > 3 && fn.compare(fn.size() - 3, 3, ".ns") == 0))
      names.push_back(fn);
  }
  std::sort(names.begin(), names.end());
  llvm::SHA256 h;
  for (const auto &fn : names) {
    std::string data;
    if (!readFile(dir + "/" + fn, data))
      continue;
    // Pemisah NUL eksplisit (hindari jebakan literal "\0" C).
    std::string head = fn;
    head.push_back('\0');
    head += std::to_string(data.size());
    head.push_back('\0');
    h.update(llvm::StringRef(head));
    h.update(llvm::StringRef(data));
  }
  std::array<uint8_t, 32> raw = h.result();
  static const char *hex = "0123456789abcdef";
  std::string o;
  for (uint8_t byte : raw) {
    o.push_back(hex[byte >> 4]);
    o.push_back(hex[byte & 0xF]);
  }
  return "sha256:" + o;
}

std::string findManifestUp() {
  std::error_code ec;
  fs::path dir = fs::current_path(ec);
  if (ec)
    return "";
  while (true) {
    fs::path cand = dir / "nusa.toml";
    if (fs::is_regular_file(cand, ec) && !ec)
      return cand.string();
    if (!dir.has_parent_path() || dir.parent_path() == dir)
      return "";
    dir = dir.parent_path();
  }
}

// ---------- registry file-backed (Phase 31, v1.12.0) ----------
// Layout: `<reg>/index/<name>.toml` (§[[version]]: version, checksum,
// deps = ["name req"]) + `<reg>/packages/<name>-<version>.tar.gz`.
// Tanpa server/network: registry = direktori (lokal/shared).

bool satisfiesReq(const std::string &version, const std::string &req) {
  if (req.empty())
    return true;
  if (req[0] == '^') {
    std::string base = req.substr(1);
    int a, b, c, x, y, z;
    std::string ap, bp;
    if (!parseSemver(version, a, b, c, ap) ||
        !parseSemver(base, x, y, z, bp))
      return false;
    if (cmpSemver(version, base) < 0)
      return false;
    // Batas atas caret: ^1.2.3 → <2.0.0; ^0.2.3 → <0.3.0;
    // ^0.0.3 → <0.0.4.
    if (x > 0)
      return a == x;
    if (y > 0)
      return a == 0 && b == y;
    return a == 0 && b == 0 && c == z;
  }
  return cmpSemver(version, req) == 0;
}

// Daftar registry: --registry + $NUSA_REGISTRY (':'-separated).
static std::vector<std::string> registryList(const std::string &flag) {
  std::vector<std::string> out;
  auto push = [&](const std::string &r) {
    if (!r.empty())
      out.push_back(r);
  };
  push(flag);
  if (const char *env = std::getenv("NUSA_REGISTRY")) {
    std::string paths(env);
    size_t start = 0;
    while (start <= paths.size()) {
      size_t sep = paths.find(':', start);
      push(paths.substr(start, sep == std::string::npos ? sep : sep - start));
      if (sep == std::string::npos)
        break;
      start = sep + 1;
    }
  }
  return out;
}

static std::string indexPathFor(const std::string &reg,
                                const std::string &name) {
  return reg + "/index/" + name + ".toml";
}

static std::string tarballPathFor(const std::string &reg,
                                  const std::string &name,
                                  const std::string &version) {
  return reg + "/packages/" + name + "-" + version + ".tar.gz";
}

static bool readIndex(const std::string &reg, const std::string &name,
                      std::vector<IndexEntry> &out, std::string &err) {
  std::string text;
  if (!readFile(indexPathFor(reg, name), text)) {
    err = "package '" + name + "' not found in registry '" + reg + "'";
    return false;
  }
  TomlDoc doc;
  if (!parseTomlDoc(text, doc, err)) {
    err = "bad index for '" + name + "': " + err;
    return false;
  }
  for (const auto &arr : doc.arrays) {
    if (arr.first != "version")
      continue;
    IndexEntry e;
    auto get = [&](const char *k, std::string &dst,
                   bool required) -> bool {
      const TomlValue *v = findKey(arr.second, k);
      if (!v) {
        if (!required) {
          dst.clear();
          return true;
        }
        err = "index for '" + name + "': entry missing '" + k + "'";
        return false;
      }
      if (v->kind != TomlValue::Kind::Str) {
        err = "index for '" + name + "': '" + k + "' must be a string";
        return false;
      }
      dst = v->str;
      return true;
    };
    std::string deps_ignored;
    if (!get("version", e.version, true) ||
        !get("checksum", e.checksum, true) ||
        !get("tarball", e.tarball, false))
      return false;
    const TomlValue *dv = findKey(arr.second, "deps");
    if (dv) {
      if (dv->kind != TomlValue::Kind::Arr) {
        err = "index for '" + name + "': 'deps' must be an array";
        return false;
      }
      e.deps = dv->arr;
    }
    out.push_back(e);
  }
  return true;
}

// Pilih versi terbaik: req persis/caret; req kosong = maksimum.
static bool selectVersion(const std::vector<IndexEntry> &entries,
                          const std::string &req, IndexEntry &sel,
                          std::string &err, const std::string &name) {
  bool found = false;
  for (const auto &e : entries) {
    if (!satisfiesReq(e.version, req))
      continue;
    if (!found || cmpSemver(e.version, sel.version) > 0) {
      sel = e;
      found = true;
    }
  }
  if (!found) {
    err = "no version of '" + name + "' satisfies '" +
          (req.empty() ? "any" : req) + "'";
    return false;
  }
  return true;
}

static std::string cacheRoot() {
  const char *home = std::getenv("HOME");
  if (!home || !*home)
    return "";
  return std::string(home) + "/.cache/nusa/registry";
}

static std::string regId(const std::string &reg) {
  return sha256Hex(fs::path(reg).lexically_normal().string()).substr(0, 16);
}

// Pastikan tarball terunduh + terverifikasi + ter-unpack ke cache.
// Mengembalikan dir cache. Selalu verifikasi checksum (keamanan).
static bool ensureCached(const std::string &reg, const std::string &name,
                         const IndexEntry &sel, std::string &cacheDir,
                         std::string &err) {
  std::string root = cacheRoot();
  if (root.empty()) {
    err = "cannot locate cache (HOME unset)";
    return false;
  }
  std::string base =
      root + "/" + regId(reg) + "/" + name + "/" + sel.version;
  std::string tb = base + ".tar.gz";
  std::string dir = base + "/pkg";
  std::error_code ec;
  bool fresh = false;
  if (fs::is_regular_file(tb, ec) && !ec) {
    std::string blob;
    // Cache valid bila tarball cocok (bila tercatat) DAN isi cocok.
    bool tbOk =
        sel.tarball.empty() || (readFile(tb, blob) &&
                                "sha256:" + sha256Hex(blob) == sel.tarball);
    if (tbOk && fs::is_directory(dir, ec) && !ec &&
        dirChecksum(dir) == sel.checksum)
      fresh = true;
  }
  if (!fresh) {
    std::string src = tarballPathFor(reg, name, sel.version);
    std::string blob;
    if (!readFile(src, blob)) {
      err = "package file missing in registry: '" + src + "'";
      return false;
    }
    // Verifikasi transport (bila index mencatatnya) lalu salin.
    if (!sel.tarball.empty() &&
        "sha256:" + sha256Hex(blob) != sel.tarball) {
      err = "tarball checksum mismatch for '" + name + " " + sel.version +
            "' (registry may be tampered)";
      return false;
    }
    fs::create_directories(base, ec);
    if (!writeFile(tb, blob, err))
      return false;
  }
  // Unpack ulang bila dir hilang atau checksum isi beda.
  bool unpackedOk = false;
  if (fs::is_directory(dir, ec) && !ec)
    unpackedOk = (dirChecksum(dir) == sel.checksum);
  if (!unpackedOk) {
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    // Pre-list (Phase 59): tolak entri absolut atau memuat `..`
    // SEBELUM ekstrak (file `../x` mendarat di luar dir sehingga
    // walk pasca-ekstrak tak melihatnya). Daftar nama `tar -tzf`
    // stabil lintas dialek (satu nama per baris).
    {
      std::string cmd =
          "tar -tzf " + shellQ(tb) + " 2>&1";
      FILE *pipe = popen(cmd.c_str(), "r");
      if (!pipe) {
        err = "failed to invoke tar";
        return false;
      }
      char buf[1024];
      bool bad = false;
      std::string badName;
      std::string acc;
      auto flushEntry = [&](std::string e) {
        // Normalisasi ringan: buang `./` depan + `/` akhir (dir).
        while (e.rfind("./", 0) == 0)
          e.erase(0, 2);
        while (!e.empty() && e.back() == '/')
          e.pop_back();
        if (e.empty() || e == ".")
          return;
        if (e[0] == '/')
          bad = true;
        else {
          size_t s = 0;
          while (!bad && s <= e.size()) {
            size_t p = e.find('/', s);
            std::string comp = e.substr(
                s, p == std::string::npos ? p : p - s);
            if (comp == "..")
              bad = true;
            if (p == std::string::npos)
              break;
            s = p + 1;
          }
        }
        if (bad && badName.empty())
          badName = e;
      };
      while (std::fgets(buf, sizeof(buf), pipe)) {
        acc += buf;
        size_t pos;
        while ((pos = acc.find('\n')) != std::string::npos) {
          flushEntry(acc.substr(0, pos));
          acc.erase(0, pos + 1);
          if (bad)
            break;
        }
        if (bad)
          break;
      }
      if (!acc.empty() && !bad)
        flushEntry(acc);
      int listRc = pclose(pipe);
      if (listRc != 0) {
        err = "tar failed to list '" + name + " " + sel.version + "'";
        return false;
      }
      if (bad) {
        fs::remove_all(dir, ec);
        err = "unsafe tarball entries in '" + name + " " + sel.version +
              "' ('" + badName + "') (registry may be tampered)";
        return false;
      }
    }
    std::string cmd = "tar -xzf " + shellQ(tb) + " -C " + shellQ(dir) +
                      " 2>&1";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
      err = "failed to invoke tar";
      return false;
    }
    char buf[1024];
    std::string tarOut;
    while (std::fgets(buf, sizeof(buf), pipe))
      tarOut += buf;
    int tarRc = pclose(pipe);
    if (tarRc != 0) {
      err = "tar failed:\n" + tarOut;
      return false;
    }
    // Guard traversal (Phase 59): paket = file reguler top-level saja.
    // Tolak subdirektori, symlink/tipe-khusus, dan nama tak-waras
    // SEBELUM checksum (file luar dir lolos checksum!). Portabel
    // std::filesystem (tanpa dialek tar).
    {
      std::string badName;
      if (!packageDirSafe(dir, badName)) {
        fs::remove_all(dir, ec);
        err = "unsafe tarball entries in '" + name + " " + sel.version +
              "'" + (badName.empty() ? "" : " ('" + badName + "')") +
              " (registry may be tampered)";
        return false;
      }
    }
    if (dirChecksum(dir) != sel.checksum) {
      err = "checksum mismatch after unpacking '" + name + " " +
            sel.version + "'";
      return false;
    }
  }
  cacheDir = dir;
  return true;
}

// ---------- resolver ----------
// Closure path-deps: nama -> (versi, dir-abs). Konflik versi ganda
// dan registry-dep langsung error (registry = Phase 31).

struct Resolved {
  std::string name;
  std::string version;
  std::string dirAbs;
  std::string reg;      // registry asal ("" = path-dep)
  std::string checksum; // checksum terverifikasi
};

static std::string abspath(const std::string &base, const std::string &rel) {
  fs::path p(rel);
  if (p.is_absolute())
    return p.lexically_normal().string();
  return (fs::path(base) / p).lexically_normal().string();
}

static bool resolveInto(const Manifest &m,
                        const std::vector<std::string> &regs,
                        const std::map<std::string, LockPkg> &pins,
                        std::vector<Resolved> &out,
                        std::vector<std::string> &stack, std::string &err) {
  for (const auto &d : m.deps) {
    if (!d.isPath()) {
      // Registry (Phase 31). Pin (build: dari lock) atau seleksi segar
      // (update: maksimum memenuhi req di registry pertama yang cocok).
      IndexEntry sel;
      std::string selReg;
      bool haveSel = false;
      auto pinIt = pins.find(d.name);
      if (pinIt != pins.end()) {
        const LockPkg &pin = pinIt->second;
        if (!satisfiesReq(pin.version, d.version)) {
          err = "lock out of date for '" + d.name + "' (requirement '" +
                d.version + "' no longer satisfied; run `nusa pkg update`)";
          return false;
        }
        sel.version = pin.version;
        sel.checksum = pin.checksum;
        selReg = pin.registry;
        haveSel = true;
        // Hash transport dibaca dari index bila registry terjangkau
        // (verifikasi dini; konten selalu diverifikasi pasca-unpack).
        if (!selReg.empty()) {
          std::vector<IndexEntry> entries;
          std::string rerr;
          if (readIndex(selReg, d.name, entries, rerr)) {
            for (const auto &e : entries) {
              if (e.version == sel.version) {
                sel.tarball = e.tarball;
                break;
              }
            }
          }
        }
      } else {
        if (regs.empty()) {
          err = "dependency '" + d.name + "' wants version '" + d.version +
                "': set $NUSA_REGISTRY or --registry (registry, Phase 31)";
          return false;
        }
        bool got = false;
        std::string lastErr;
        for (const auto &reg : regs) {
          std::vector<IndexEntry> entries;
          if (!readIndex(reg, d.name, entries, lastErr))
            continue;
          IndexEntry cand;
          if (selectVersion(entries, d.version, cand, lastErr, d.name)) {
            sel = cand;
            selReg = reg;
            got = true;
            break; // registry pertama yang cocok menang
          }
        }
        // Anti-confusion (Phase 114): preseden "registry pertama
        // menang" terlihat diam-diam bila nama ada di >1 registry.
        // Tak digagalkan (mirror sah memakai nama sama) — tapi
        // DIBERITAHU agar shadowing tak pernah senyap.
        if (got) {
          for (const auto &reg : regs) {
            if (reg == selReg)
              continue;
            std::vector<IndexEntry> others;
            std::string oerr;
            if (!readIndex(reg, d.name, others, oerr))
              continue;
            IndexEntry dummy;
            if (selectVersion(others, d.version, dummy, oerr, d.name)) {
              std::fprintf(stderr,
                           "warning: '%s' also in registry '%s' (using "
                           "first match from '%s')\n",
                           d.name.c_str(), reg.c_str(), selReg.c_str());
              break;
            }
          }
        }
        if (!got) {
          err = lastErr.empty()
                    ? "dependency '" + d.name + "' not found in any registry"
                    : lastErr;
          return false;
        }
      }
      if (selReg.empty()) {
        err = "package '" + d.name + " " + sel.version +
              "' has no registry provenance (run `nusa pkg update`)";
        return false;
      }
      std::string cacheDir;
      if (!ensureCached(selReg, d.name, sel, cacheDir, err))
        return false;
      std::string depMan = cacheDir + "/nusa.toml";
      Manifest dm;
      if (!parseManifest(depMan, dm, err)) {
        err = "dependency '" + d.name + "': " + err;
        return false;
      }
      if (dm.name != d.name || dm.version != sel.version) {
        err = "dependency '" + d.name + "': cache mismatch";
        return false;
      }
      bool present = false;
      for (const auto &r : out) {
        if (r.name == dm.name) {
          present = true;
          if (r.version != dm.version) {
            err = "version conflict for '" + dm.name + "': '" + r.version +
                  "' vs '" + dm.version + "'";
            return false;
          }
        }
      }
      if (!present) {
        out.push_back(Resolved{dm.name, dm.version, cacheDir, selReg,
                               sel.checksum});
        stack.push_back(cacheDir);
        if (!resolveInto(dm, regs, pins, out, stack, err))
          return false;
        stack.pop_back();
      }
      continue;
    }
    std::string depDir = abspath(m.dir, d.path);
    std::error_code ec;
    if (!fs::is_directory(depDir, ec) || ec) {
      err = "dependency '" + d.name + "': directory '" + d.path +
            "' not found (from '" + m.dir + "')";
      return false;
    }
    for (const auto &s : stack) {
      if (s == depDir) {
        err = "dependency cycle involving '" + d.name + "'";
        return false;
      }
    }
    std::string depMan = depDir + "/nusa.toml";
    Manifest dm;
    if (!parseManifest(depMan, dm, err)) {
      err = "dependency '" + d.name + "': " + err;
      return false;
    }
    if (dm.name != d.name) {
      err = "dependency '" + d.name + "': manifest at '" + d.path +
            "' declares name '" + dm.name + "'";
      return false;
    }
    bool present = false;
    for (const auto &r : out) {
      if (r.name == dm.name) {
        present = true;
        if (r.version != dm.version) {
          err = "version conflict for '" + dm.name + "': '" + r.version +
                "' vs '" + dm.version + "'";
          return false;
        }
      }
    }
    if (!present) {
      out.push_back(Resolved{dm.name, dm.version, depDir, "",
                             dirChecksum(depDir)});
      stack.push_back(depDir);
      if (!resolveInto(dm, regs, pins, out, stack, err))
        return false;
      stack.pop_back();
    }
  }
  return true;
}

static std::string toRel(const std::string &rootDir,
                         const std::string &abs) {
  // Relatif (`../..` bila perlu) agar lock portabel; absolut bila
  // tak berhubungan (lexically_proximate murni leksikal).
  fs::path rel = fs::path(abs).lexically_proximate(fs::path(rootDir));
  if (!rel.empty())
    return rel.string();
  return fs::path(abs).lexically_normal().string();
}

// ---------- perintah ----------

static int cmdInit(const std::vector<std::string> &args) {
  std::string dir = ".";
  std::string name;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--dir" && i + 1 < args.size()) {
      dir = args[++i];
    } else if (!args[i].empty() && args[i][0] != '-') {
      if (!name.empty()) {
        std::fprintf(stderr, "error: unexpected argument '%s'\n",
                     args[i].c_str());
        return 1;
      }
      name = args[i];
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    }
  }
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) {
    std::fprintf(stderr, "error: cannot create '%s'\n", dir.c_str());
    return 1;
  }
  if (name.empty()) {
    name = fs::path(fs::absolute(dir, ec)).filename().string();
    if (name.empty() || ec)
      name = "myapp";
  }
  if (!validName(name)) {
    std::fprintf(stderr,
                 "error: invalid package name '%s' (want [A-Za-z_][A-Za-z0-9_]*)\n",
                 name.c_str());
    return 1;
  }
  std::string manPath = (fs::path(dir) / "nusa.toml").string();
  if (fs::is_regular_file(manPath, ec) && !ec) {
    std::fprintf(stderr, "error: '%s' already exists\n", manPath.c_str());
    return 1;
  }
  Manifest m;
  m.name = name;
  m.version = "0.1.0";
  m.compiler = compilerVersion();
  m.dir = dir;
  std::string err;
  if (!writeManifest(manPath, m, err, true)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  // Stub main.ns bila belum ada sumber .ns.
  bool hasNs = false;
  for (auto it = fs::directory_iterator(dir, ec);
       !ec && it != fs::directory_iterator(); ++it) {
    std::string fn = it->path().filename().string();
    if (fn.size() > 3 && fn.compare(fn.size() - 3, 3, ".ns") == 0) {
      hasNs = true;
      break;
    }
  }
  if (!hasNs) {
    std::string stub = "fn main() {\n    print(\"Hello, " + name + "\");\n}\n";
    if (!writeFile((fs::path(dir) / "main.ns").string(), stub, err)) {
      std::fprintf(stderr, "error: %s\n", err.c_str());
      return 1;
    }
  }
  std::printf("Initialized package '%s' in '%s'\n", name.c_str(),
              manPath.c_str());
  return 0;
}

static int cmdAdd(const std::vector<std::string> &args) {
  std::string name, path, version;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--path" && i + 1 < args.size()) {
      path = args[++i];
    } else if (!args[i].empty() && args[i][0] != '-') {
      if (name.empty()) {
        name = args[i];
      } else if (version.empty()) {
        // Posisi kedua non-flag = versi (`pkg add foo 1.2.3`).
        version = args[i];
      } else {
        std::fprintf(stderr, "error: unexpected argument '%s'\n",
                     args[i].c_str());
        return 1;
      }
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    }
  }
  if (name.empty() || (path.empty() == version.empty())) {
    std::fprintf(stderr,
                 "error: usage: nusa pkg add <name> (--path <dir> | <version>)\n");
    return 1;
  }
  if (!validName(name)) {
    std::fprintf(stderr, "error: invalid package name '%s'\n", name.c_str());
    return 1;
  }
  if (!version.empty()) {
    std::string vchk = version;
    if (!vchk.empty() && vchk[0] == '^')
      vchk = vchk.substr(1);
    int a, b, c;
    std::string pre;
    if (!parseSemver(vchk, a, b, c, pre)) {
      std::fprintf(stderr, "error: invalid version '%s'\n", version.c_str());
      return 1;
    }
  }
  std::string manPath = findManifestUp();
  if (manPath.empty()) {
    std::fprintf(stderr, "error: no nusa.toml found (run `nusa pkg init`)\n");
    return 1;
  }
  Manifest m;
  std::string err;
  if (!parseManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  for (const auto &d : m.deps) {
    if (d.name == name) {
      std::fprintf(stderr, "error: dependency '%s' already present\n",
                   name.c_str());
      return 1;
    }
  }
  if (!path.empty()) {
    std::error_code ec;
    if (!fs::is_directory(abspath(m.dir, path), ec) || ec) {
      std::fprintf(stderr, "error: directory '%s' not found\n", path.c_str());
      return 1;
    }
    std::string depMan = abspath(m.dir, path) + "/nusa.toml";
    Manifest dm;
    if (!parseManifest(depMan, dm, err)) {
      std::fprintf(stderr, "error: target is not a package: %s\n",
                   err.c_str());
      return 1;
    }
    if (dm.name != name) {
      std::fprintf(stderr,
                   "error: target declares name '%s', want '%s'\n",
                   dm.name.c_str(), name.c_str());
      return 1;
    }
  }
  Dep d;
  d.name = name;
  d.path = path;
  d.version = version;
  m.deps.push_back(d);
  if (!writeManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::printf("Added dependency '%s' (run `nusa pkg update`)\n", name.c_str());
  return 0;
}

static int cmdRemove(const std::vector<std::string> &args) {
  if (args.size() != 1 || args[0].empty() || args[0][0] == '-') {
    std::fprintf(stderr, "error: usage: nusa pkg remove <name>\n");
    return 1;
  }
  std::string manPath = findManifestUp();
  if (manPath.empty()) {
    std::fprintf(stderr, "error: no nusa.toml found\n");
    return 1;
  }
  Manifest m;
  std::string err;
  if (!parseManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  size_t before = m.deps.size();
  m.deps.erase(std::remove_if(m.deps.begin(), m.deps.end(),
                              [&](const Dep &d) {
                                return d.name == args[0];
                              }),
               m.deps.end());
  if (m.deps.size() == before) {
    std::fprintf(stderr, "error: no dependency '%s'\n", args[0].c_str());
    return 1;
  }
  if (!writeManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::printf("Removed dependency '%s' (run `nusa pkg update`)\n",
              args[0].c_str());
  return 0;
}

static int cmdUpdateAt(const std::string &rootDir,
                       const std::vector<std::string> &regs) {
  std::string manPath =
      (rootDir.empty() ? std::string(".") : rootDir) + "/nusa.toml";
  Manifest m;
  std::string err;
  if (!parseManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::string rd = rootDir.empty() ? "." : rootDir;
  std::map<std::string, LockPkg> pins; // kosong = seleksi segar
  std::vector<Resolved> res;
  std::vector<std::string> stack;
  if (!resolveInto(m, regs, pins, res, stack, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::vector<LockPkg> pkgs;
  for (const auto &r : res) {
    LockPkg p;
    p.name = r.name;
    p.version = r.version;
    // Registry: cache machine-local → absolut. Path-dep: relatif.
    p.path = r.reg.empty()
                 ? toRel(rd, r.dirAbs)
                 : fs::path(r.dirAbs).lexically_normal().string();
    p.registry = r.reg;
    p.checksum = r.checksum;
    pkgs.push_back(p);
  }
  std::string lockPath = rd + "/nusa.lock";
  if (!writeLock(lockPath, pkgs, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::printf("Locked %zu dependencies in '%s'\n", pkgs.size(),
              lockPath.c_str());
  for (const auto &p : pkgs)
    std::printf("  %s %s (%s)%s\n", p.name.c_str(), p.version.c_str(),
                p.checksum.c_str(),
                p.registry.empty() ? "" : " [registry]");
  return 0;
}

static int cmdUpdate(const std::vector<std::string> &args) {
  std::string regFlag;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--registry" && i + 1 < args.size()) {
      regFlag = args[++i];
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    }
  }
  std::string manPath = findManifestUp();
  if (manPath.empty()) {
    std::fprintf(stderr, "error: no nusa.toml found\n");
    return 1;
  }
  std::string rootDir = fs::path(manPath).parent_path().string();
  if (rootDir.empty())
    rootDir = ".";
  std::vector<std::string> regs = registryList(regFlag);
  return cmdUpdateAt(rootDir, regs);
}

// Verifikasi lock segar (ada + cocok). False + pesan.
static bool lockFresh(const Manifest &m, const std::string &rootDir,
                      const std::vector<std::string> &regs,
                      std::vector<LockPkg> &pkgs, std::string &err) {
  std::string lockPath = rootDir + "/nusa.lock";
  pkgs.clear();
  std::string lockCompiler;
  if (!parseLockFull(lockPath, pkgs, &lockCompiler, err)) {
    err = "no fresh lock (run `nusa pkg update`)";
    return false;
  }
  // Pin compiler (Phase 61): lock milik compiler eksak; lock lama
  // tanpa [meta] maupun beda versi = basi (self-heal via update).
  if (lockCompiler.empty()) {
    err = "lock has no compiler pin (run `nusa pkg update`)";
    return false;
  }
  if (lockCompiler != compilerVersion()) {
    err = "lock pinned to compiler " + lockCompiler + ", have " +
          compilerVersion() + " (run `nusa pkg update`)";
    return false;
  }
  std::map<std::string, LockPkg> pins;
  for (const auto &p : pkgs)
    pins[p.name] = p;
  std::vector<Resolved> res;
  std::vector<std::string> stack;
  if (!resolveInto(m, regs, pins, res, stack, err))
    return false;
  if (res.size() != pkgs.size()) {
    err = "lock out of date (run `nusa pkg update`)";
    return false;
  }
  for (const auto &r : res) {
    bool found = false;
    for (const auto &p : pkgs) {
      if (p.name == r.name) {
        found = true;
        // r.checksum = isi segar terverifikasi (path: hitung-ulang;
        // registry: index == cache via ensureCached).
        if (p.version != r.version || p.registry != r.reg ||
            p.checksum != r.checksum) {
          err = "lock out of date for '" + r.name +
                "' (run `nusa pkg update`)";
          return false;
        }
      }
    }
    if (!found) {
      err = "lock out of date (run `nusa pkg update`)";
      return false;
    }
  }
  return true;
}

static int cmdBuild(Driver &driver, const std::vector<std::string> &args) {
  std::string output;
  std::string regFlag;
  std::string targetFlag;
  std::string sysroot;
  std::string emit;
  std::string opt;
  bool fresh = false;
  bool debug = false;
  bool stats = false;
  bool frozen = false;
  bool pgoGenerate = false;
  std::string pgoUse;
  std::string profOpt;
  bool profDebug = false;
  bool haveProfile = false;
  bool optExplicit = false;
  bool debugExplicit = false;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "-o" && i + 1 < args.size()) {
      output = args[++i];
    } else if (args[i] == "--profile" && i + 1 < args.size()) {
      std::string perr;
      if (!Driver::resolveProfile(args[++i], profOpt, profDebug, perr)) {
        std::fprintf(stderr, "error: %s\n", perr.c_str());
        return 1;
      }
      haveProfile = true;
    } else if (args[i] == "--registry" && i + 1 < args.size()) {
      regFlag = args[++i];
    } else if (args[i] == "--target" && i + 1 < args.size()) {
      targetFlag = args[++i];
    } else if (args[i] == "--sysroot" && i + 1 < args.size()) {
      sysroot = args[++i];
    } else if (args[i] == "--emit-ir") {
      emit = "ir";
    } else if (args[i] == "--emit-asm") {
      emit = "asm";
    } else if (args[i] == "-c" || args[i] == "--emit-obj") {
      emit = "obj";
    } else if (args[i] == "--fresh") {
      fresh = true;
    } else if (args[i] == "--frozen") {
      // CI/rilis: jangan pernah mengambil dependensi saat build.
      frozen = true;
    } else if (args[i] == "--stats") {
      stats = true;
    } else if (args[i] == "-g" || args[i] == "--debug") {
      debug = true;
      debugExplicit = true;
    } else if (args[i] == "--pgo-generate") {
      pgoGenerate = true;
    } else if (args[i] == "--pgo-use" && i + 1 < args.size()) {
      pgoUse = args[++i];
    } else if (args[i].rfind("--pgo-use=", 0) == 0) {
      pgoUse = args[i].substr(10);
    } else if (args[i].size() == 3 && args[i][0] == '-' &&
               args[i][1] == 'O' &&
               (args[i][2] == '0' || args[i][2] == '1' ||
                args[i][2] == '2' || args[i][2] == '3' ||
                args[i][2] == 's' || args[i][2] == 'z')) {
      opt = args[i].substr(2, 1);
      optExplicit = true;
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    }
  }
  if (haveProfile) {
    if (!optExplicit)
      opt = profOpt;
    if (!debugExplicit)
      debug = profDebug;
  }
  std::string manPath = findManifestUp();
  if (manPath.empty()) {
    std::fprintf(stderr, "error: no nusa.toml found\n");
    return 1;
  }
  Manifest m;
  std::string err;
  if (!parseManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  if (!m.compiler.empty() &&
      !satisfiesMin(compilerVersion(), m.compiler)) {
    std::fprintf(stderr,
                 "error: package needs compiler >= %s (have %s)\n",
                 m.compiler.c_str(), compilerVersion().c_str());
    return 1;
  }
  std::string rootDir =
      fs::path(manPath).parent_path().string();
  if (rootDir.empty())
    rootDir = ".";
  // Lock: buat bila absen, verifikasi bila ada (reproducible).
  // --frozen (Phase 114): JANGAN pernah update/fetch saat build —
  // gagal bila lock absen/basi (batas supply-chain untuk CI/rilis).
  std::vector<std::string> regs = registryList(regFlag);
  std::vector<LockPkg> pkgs;
  {
    std::error_code ec;
    bool hasLock = fs::is_regular_file(rootDir + "/nusa.lock", ec) && !ec;
    if (!hasLock) {
      if (frozen) {
        std::fprintf(stderr, "error: no nusa.lock (--frozen: refusing to "
                             "fetch dependencies during build; run `nusa pkg "
                             "update` explicitly)\n");
        return 1;
      }
      std::printf("note: no nusa.lock; running update first\n");
      if (cmdUpdateAt(rootDir, regs) != 0)
        return 1;
    }
    if (!lockFresh(m, rootDir, regs, pkgs, err)) {
      std::fprintf(stderr, "error: %s\n", err.c_str());
      return 1;
    }
  }
  // Entry + output.
  std::string entry = m.entry;
  if (entry.empty()) {
    std::error_code ec;
    if (fs::is_regular_file(rootDir + "/main.ns", ec) && !ec)
      entry = rootDir + "/main.ns";
    else if (fs::is_regular_file(rootDir + "/src/main.ns", ec) && !ec)
      entry = rootDir + "/src/main.ns";
    else {
      std::fprintf(stderr, "error: no entry (main.ns or src/main.ns)\n");
      return 1;
    }
  } else if (entry[0] != '/') {
    entry = rootDir + "/" + entry;
  }
  if (output.empty()) {
    std::error_code ec;
    fs::create_directories(rootDir + "/target", ec);
    output = rootDir + "/target/" + m.name;
  }
  // NUSA_PATH = dir dep (absolut) + warisan.
  std::string nusaPath;
  for (const auto &p : pkgs) {
    std::string d = p.path;
    if (!d.empty() && d[0] != '/')
      d = rootDir + "/" + d;
    if (!nusaPath.empty())
      nusaPath += ":";
    nusaPath += d;
  }
  const char *oldNp = std::getenv("NUSA_PATH");
  std::string saved = oldNp ? oldNp : "";
  bool hadNp = oldNp != nullptr;
  if (!nusaPath.empty()) {
    if (hadNp)
      nusaPath += ":" + saved;
#if defined(_WIN32)
    _putenv_s("NUSA_PATH", nusaPath.c_str());
#else
    setenv("NUSA_PATH", nusaPath.c_str(), 1);
#endif
  }
  int rc = 0;
  {
    // Capability (Phase 59): ringkas + enforce `native` (extern/[link]).
    std::printf("capabilities: %s\n",
                capabilitiesSummary(m).c_str());
    bool wantsLink = !m.linkLibs.empty() || !m.linkLibDirs.empty() ||
                     !m.linkFiles.empty();
    // Gate native: pindai program gabungan (checkStages murah —
    // frontend; runBuild mengulanginya). Hanya bila cap absen.
    if (!m.caps.native) {
      // Pindai `extern fn` pada program gabungan (checkStages tak
      // mencetak apa pun; diagnostic asli tetap dari runBuild bila
      // program rusak — gate hanya jalan saat check lolos).
      Program merged;
      if (driver.checkStages(entry, merged) == 0) {
        bool hasExtern = false;
        for (const auto &fn : merged.functions) {
          if (fn.isExtern) {
            hasExtern = true;
            break;
          }
        }
        if (hasExtern || wantsLink) {
          std::fprintf(stderr,
                       "error: package '%s' uses native code (extern fn / "
                       "[link]) but [capabilities] native is not true\n"
                       "help: declare `native = true` under [capabilities] "
                       "in nusa.toml to acknowledge FFI trust\n",
                       m.name.c_str());
          rc = 1;
        }
      }
    }
    if (rc != 0) {
      // Gate menolak: lewati runBuild; NUSA_PATH dikembalikan di bawah.
    } else {
    // [link] manifest (Phase 32): relatif ke rootDir.
    LinkSpec links;
    links.libs = m.linkLibs;
    for (const auto &d : m.linkLibDirs) {
      if (!d.empty() && d[0] != '/')
        links.libDirs.push_back(rootDir + "/" + d);
      else
        links.libDirs.push_back(d);
    }
    for (const auto &f : m.linkFiles) {
      if (!f.empty() && f[0] != '/')
        links.files.push_back(rootDir + "/" + f);
      else
        links.files.push_back(f);
    }
    // Target: CLI menang atas manifest (Phase 33).
    std::string target = targetFlag.empty() ? m.target : targetFlag;
    rc = driver.runBuild(entry, output, links, target, sysroot, emit, opt,
                         fresh, debug, stats, pgoGenerate, pgoUse);
    }
  }
  // Kembalikan NUSA_PATH (hanya bila kita yang mengubah).
  if (!nusaPath.empty()) {
#if defined(_WIN32)
    if (hadNp)
      _putenv_s("NUSA_PATH", saved.c_str());
#else
    if (hadNp)
      setenv("NUSA_PATH", saved.c_str(), 1);
    else
      unsetenv("NUSA_PATH");
#endif
  }
  if (rc != 0)
    return rc;
  std::printf("Built '%s' -> '%s'\n", m.name.c_str(), output.c_str());
  return 0;
}

static int cmdPublish(Driver &driver, const std::vector<std::string> &args) {
  bool dryRun = false;
  std::string regFlag;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--dry-run") {
      dryRun = true;
    } else if (args[i] == "--registry" && i + 1 < args.size()) {
      regFlag = args[++i];
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    }
  }
  std::string manPath = findManifestUp();
  if (manPath.empty()) {
    std::fprintf(stderr, "error: no nusa.toml found\n");
    return 1;
  }
  Manifest m;
  std::string err;
  if (!parseManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::string rootDir =
      fs::path(manPath).parent_path().string();
  if (rootDir.empty())
    rootDir = ".";
  std::vector<std::string> regs = registryList(regFlag);
  std::vector<LockPkg> pkgs;
  {
    std::error_code ec;
    bool hasLock = fs::is_regular_file(rootDir + "/nusa.lock", ec) && !ec;
    if (!hasLock) {
      std::printf("note: no nusa.lock; running update first\n");
      if (cmdUpdateAt(rootDir, regs) != 0)
        return 1;
    }
  }
  if (!lockFresh(m, rootDir, regs, pkgs, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  // Pastikan bisa build sebelum dikemas (lib tanpa entry: lewati).
  {
    std::string entryProbe = m.entry;
    std::error_code ec;
    bool hasEntry = !entryProbe.empty();
    if (!hasEntry) {
      hasEntry =
          (fs::is_regular_file(rootDir + "/main.ns", ec) && !ec) ||
          (fs::is_regular_file(rootDir + "/src/main.ns", ec) && !ec);
    }
    if (hasEntry) {
      std::vector<std::string> buildArgs;
      int rc = cmdBuild(driver, buildArgs);
      if (rc != 0)
        return rc;
    } else {
      std::printf("note: no entry; library package, skipping build\n");
    }
  }
  std::string distDir = rootDir + "/dist";
  std::error_code ec;
  fs::create_directories(distDir, ec);
  std::string tarball =
      distDir + "/" + m.name + "-" + m.version + ".tar.gz";
  // Daftar file reproducible (terurut): manifest + lock + *.ns.
  std::string files = "nusa.toml nusa.lock";
  {
    std::vector<std::string> names;
    for (auto it = fs::directory_iterator(rootDir, ec);
         !ec && it != fs::directory_iterator(); ++it) {
      if (!it->is_regular_file(ec) || ec)
        continue;
      std::string fn = it->path().filename().string();
      if (fn.size() > 3 && fn.compare(fn.size() - 3, 3, ".ns") == 0)
        names.push_back(fn);
    }
    std::sort(names.begin(), names.end());
    for (const auto &fn : names)
      files += " " + shellQ(fn);
  }
  // Tarball byte-deterministik (Phase 61): entri terurut, mtime/
  // owner dinormalisasi, gzip tanpa nama/timestamp. Dua tahap agar
  // error tar tidak tertelan pipe (gzip exit 0 atas input kosong).
  std::string tmpTar = distDir + "/." + m.name + "-" + m.version + ".tar";
  std::string cmd = "tar --sort=name --mtime='@0' --owner=0 --group=0"
                    " --numeric-owner -cf " +
                    shellQ(tmpTar) + " -C " + shellQ(rootDir) + " " + files +
                    " 2>&1";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    std::fprintf(stderr, "error: failed to invoke tar\n");
    return 1;
  }
  char buf[1024];
  std::string tarOut;
  while (std::fgets(buf, sizeof(buf), pipe))
    tarOut += buf;
  int tarRc = pclose(pipe);
  if (tarRc != 0) {
    std::fprintf(stderr, "error: tar failed:\n%s\n", tarOut.c_str());
    fs::remove(tmpTar, ec);
    return 1;
  }
  std::string cmd2 = "gzip -n -c " + shellQ(tmpTar) + " > " +
                     shellQ(tarball) + " 2>&1";
  FILE *pipe2 = popen(cmd2.c_str(), "r");
  if (!pipe2) {
    std::fprintf(stderr, "error: failed to invoke gzip\n");
    fs::remove(tmpTar, ec);
    return 1;
  }
  std::string gzOut;
  while (std::fgets(buf, sizeof(buf), pipe2))
    gzOut += buf;
  int gzRc = pclose(pipe2);
  fs::remove(tmpTar, ec);
  if (gzRc != 0) {
    std::fprintf(stderr, "error: gzip failed:\n%s\n", gzOut.c_str());
    return 1;
  }
  std::string blob;
  if (!readFile(tarball, blob)) {
    std::fprintf(stderr, "error: cannot read back '%s'\n", tarball.c_str());
    return 1;
  }
  std::string sum = sha256Hex(blob);
  if (dryRun)
    std::printf("(dry-run) ");
  std::printf("Packaged '%s' (sha256:%s)\n", tarball.c_str(), sum.c_str());
  // Push registry (Phase 31): path-deps tak relokatable → tolak;
  // tiap registry: tarball + index (idempoten bila identik).
  std::vector<std::string> pushRegs = registryList(regFlag);
  if (!pushRegs.empty()) {
    for (const auto &d : m.deps) {
      if (d.isPath()) {
        std::fprintf(stderr,
                     "error: cannot publish '%s': path dependency '%s' "
                     "is not relocatable\n",
                     m.name.c_str(), d.name.c_str());
        std::fprintf(stderr, "help: publish '%s' first, then depend on "
                             "its version\n",
                     d.name.c_str());
        return 1;
      }
    }
    for (const auto &reg : pushRegs) {
      std::vector<IndexEntry> entries;
      std::string rerr;
      readIndex(reg, m.name, entries, rerr); // absen = paket baru
      bool haveSame = false;
      for (const auto &e : entries) {
        if (e.version == m.version) {
          if (e.checksum != dirChecksum(rootDir)) {
            std::fprintf(stderr,
                         "error: '%s %s' already published in '%s' "
                         "with different content\n",
                         m.name.c_str(), m.version.c_str(), reg.c_str());
            return 1;
          }
          haveSame = true;
        }
      }
      std::error_code ec;
      fs::create_directories(reg + "/packages", ec);
      fs::create_directories(reg + "/index", ec);
      if (!writeFile(tarballPathFor(reg, m.name, m.version), blob, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
      }
      if (!haveSame) {
        IndexEntry ne;
        ne.version = m.version;
        ne.checksum = dirChecksum(rootDir);
        ne.tarball = "sha256:" + sum;
        for (const auto &d : m.deps)
          ne.deps.push_back(d.name + " " + d.version);
        entries.push_back(ne);
      } else {
        // Konten identik: segarkan hash transport (gzip tak
        // deterministik antar-kemas) agar unduhan terverifikasi.
        for (auto &e : entries) {
          if (e.version == m.version)
            e.tarball = "sha256:" + sum;
        }
      }
      std::string idx = "# Registry index for '" + m.name + "'.\n";
      for (const auto &e : entries) {
        idx += "\n[[version]]\n";
        idx += "version = \"" + escStr(e.version) + "\"\n";
        idx += "checksum = \"" + escStr(e.checksum) + "\"\n";
        if (!e.tarball.empty())
          idx += "tarball = \"" + escStr(e.tarball) + "\"\n";
        if (!e.deps.empty()) {
          idx += "deps = [";
          for (size_t i = 0; i < e.deps.size(); i++) {
            if (i)
              idx += ", ";
            idx += "\"" + escStr(e.deps[i]) + "\"";
          }
          idx += "]\n";
        }
      }
      if (!writeFile(indexPathFor(reg, m.name), idx, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
      }
      std::printf("Published '%s %s' to '%s'\n", m.name.c_str(),
                  m.version.c_str(), reg.c_str());
    }
  } else {
    std::printf("note: pass --registry <dir> (or $NUSA_REGISTRY) to publish\n");
  }
  return 0;
}

static int cmdSearch(const std::vector<std::string> &args) {
  std::string query;
  std::string regFlag;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--registry" && i + 1 < args.size()) {
      regFlag = args[++i];
    } else if (!args[i].empty() && args[i][0] != '-') {
      if (!query.empty()) {
        std::fprintf(stderr, "error: unexpected argument '%s'\n",
                     args[i].c_str());
        return 1;
      }
      query = args[i];
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    }
  }
  if (query.empty()) {
    std::fprintf(stderr, "error: usage: nusa search <query> [--registry DIR]\n");
    return 1;
  }
  std::vector<std::string> regs = registryList(regFlag);
  if (regs.empty()) {
    std::fprintf(stderr,
                 "error: no registry (set $NUSA_REGISTRY or --registry)\n");
    return 1;
  }
  bool any = false;
  for (const auto &reg : regs) {
    std::error_code ec;
    std::string idxDir = reg + "/index";
    if (!fs::is_directory(idxDir, ec) || ec)
      continue;
    // Iterasi terurut agar output stabil.
    std::vector<std::string> names;
    for (auto it = fs::directory_iterator(idxDir, ec);
         !ec && it != fs::directory_iterator(); ++it) {
      std::string fn = it->path().filename().string();
      if (fn.size() > 5 && fn.compare(fn.size() - 5, 5, ".toml") == 0)
        names.push_back(fn.substr(0, fn.size() - 5));
    }
    std::sort(names.begin(), names.end());
    for (const auto &nm : names) {
      if (nm.find(query) == std::string::npos)
        continue;
      std::vector<IndexEntry> entries;
      std::string rerr;
      if (!readIndex(reg, nm, entries, rerr))
        continue;
      std::string vers;
      for (size_t i = 0; i < entries.size(); i++) {
        if (i)
          vers += ", ";
        vers += entries[i].version;
      }
      std::printf("%s (%s) [%s]\n", nm.c_str(), vers.c_str(), reg.c_str());
      any = true;
    }
  }
  if (!any)
    std::printf("no packages matching '%s'\n", query.c_str());
  return 0;
}

// ---------- read-only inspection (Phase 60, v2.11.0) ----------
// tree/verify/outdated/audit: tak mengubah disk (tanpa network;
// registry = direktori lokal). Top-level + `pkg` (cermin
// search/install/publish).

struct InspRoot {
  Manifest m;
  std::string rootDir;
  std::vector<LockPkg> pins;
  bool haveLock = false;
};

static const LockPkg *findPin(const std::vector<LockPkg> &pins,
                              const std::string &name) {
  for (const auto &p : pins) {
    if (p.name == name)
      return &p;
  }
  return nullptr;
}

static bool loadInspect(InspRoot &out, std::string &err) {
  std::string manPath = findManifestUp();
  if (manPath.empty()) {
    err = "no nusa.toml (run inside a package)";
    return false;
  }
  if (!parseManifest(manPath, out.m, err))
    return false;
  size_t slash = manPath.find_last_of("/\\");
  out.rootDir = (slash == std::string::npos) ? "." : manPath.substr(0, slash);
  if (out.rootDir.empty())
    out.rootDir = ".";
  std::error_code ec;
  std::string lockPath = out.rootDir + "/nusa.lock";
  if (fs::is_regular_file(lockPath, ec) && !ec) {
    if (!parseLock(lockPath, out.pins, err))
      return false;
    out.haveLock = true;
  }
  return true;
}

static void printTreeDep(const Manifest &m, const std::string &baseDir,
                         const std::vector<LockPkg> &pins,
                         std::set<std::string> &seen, int depth) {
  std::string pad(static_cast<size_t>(depth) * 2, ' ');
  for (const auto &d : m.deps) {
    if (d.isPath()) {
      std::string dirAbs = abspath(baseDir, d.path);
      std::error_code ec;
      std::string manPath = dirAbs + "/nusa.toml";
      std::string ver = "?";
      Manifest dm;
      std::string derr;
      if (fs::is_regular_file(manPath, ec) && !ec &&
          parseManifest(manPath, dm, derr))
        ver = dm.version;
      std::printf("%s%s %s (path)\n", pad.c_str(), d.name.c_str(),
                  ver.c_str());
      if (seen.insert(dirAbs).second && ver != "?") {
        printTreeDep(dm, dirAbs, pins, seen, depth + 1);
      }
    } else {
      const LockPkg *pin = findPin(pins, d.name);
      if (pin)
        std::printf("%s%s %s -> %s (registry)\n", pad.c_str(), d.name.c_str(),
                    d.version.c_str(), pin->version.c_str());
      else
        std::printf("%s%s %s (registry, unlocked)\n", pad.c_str(),
                    d.name.c_str(), d.version.c_str());
    }
  }
}

static int cmdTree(const std::vector<std::string> &args) {
  for (const auto &a : args) {
    std::fprintf(stderr, "error: unexpected argument '%s'\n", a.c_str());
    return 1;
  }
  InspRoot ins;
  std::string err;
  if (!loadInspect(ins, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::printf("%s %s\n", ins.m.name.c_str(), ins.m.version.c_str());
  std::set<std::string> seen;
  seen.insert(abspath(".", ins.rootDir));
  printTreeDep(ins.m, ins.rootDir, ins.pins, seen, 1);
  if (!ins.haveLock)
    std::printf("note: no nusa.lock (registry versions unresolved)\n");
  return 0;
}

static int cmdVerify(const std::vector<std::string> &args) {
  std::string regFlag;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--registry" && i + 1 < args.size()) {
      regFlag = args[++i];
    } else {
      std::fprintf(stderr, "error: unexpected argument '%s'\n",
                   args[i].c_str());
      return 1;
    }
  }
  InspRoot ins;
  std::string err;
  if (!loadInspect(ins, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  (void)regFlag;
  int fails = 0;
  int skips = 0;
  std::printf("OK %s %s (manifest, capabilities: %s)\n", ins.m.name.c_str(),
              ins.m.version.c_str(), capabilitiesSummary(ins.m).c_str());
  if (!ins.haveLock) {
    std::printf("note: no nusa.lock (only root manifest verified)\n");
    return 0;
  }
  for (const auto &pin : ins.pins) {
    if (pin.registry.empty()) {
      // Path-dep: direktori + checksum + isi aman.
      std::string dirAbs = abspath(ins.rootDir, pin.path);
      std::error_code ec;
      bool ok = fs::is_directory(dirAbs, ec) && !ec;
      std::string why;
      if (ok) {
        std::string manPath = dirAbs + "/nusa.toml";
        Manifest dm;
        if (!parseManifest(manPath, dm, err)) {
          ok = false;
          why = "manifest: " + err;
        } else if (dm.name != pin.name || dm.version != pin.version) {
          ok = false;
          why = "name/version mismatch";
        } else if (dirChecksum(dirAbs) != pin.checksum) {
          ok = false;
          why = "checksum mismatch";
        } else if (!packageDirSafe(dirAbs, why)) {
          ok = false;
          why = std::string("unsafe entries ('") + why + "')";
        }
      } else {
        why = "directory missing";
      }
      if (ok) {
        std::printf("OK %s %s (path)\n", pin.name.c_str(),
                    pin.version.c_str());
      } else {
        std::printf("FAIL %s %s (path): %s\n", pin.name.c_str(),
                    pin.version.c_str(), why.c_str());
        fails++;
      }
    } else {
      // Registry-dep: tarball cache vs index + isi vs pin.
      std::string root = cacheRoot();
      std::string base =
          root + "/" + regId(pin.registry) + "/" + pin.name + "/" +
          pin.version;
      std::string tb = base + ".tar.gz";
      std::string dir = base + "/pkg";
      std::error_code ec;
      if (!fs::is_regular_file(tb, ec) || ec) {
        std::printf("SKIP %s %s (registry, not cached)\n", pin.name.c_str(),
                    pin.version.c_str());
        skips++;
        continue;
      }
      bool ok = true;
      std::string why;
      std::vector<IndexEntry> entries;
      std::string rerr;
      if (readIndex(pin.registry, pin.name, entries, rerr)) {
        for (const auto &e : entries) {
          if (e.version == pin.version && !e.tarball.empty()) {
            std::string blob;
            if (!readFile(tb, blob) ||
                "sha256:" + sha256Hex(blob) != e.tarball) {
              ok = false;
              why = "tarball checksum mismatch";
            }
            break;
          }
        }
      }
      if (ok && fs::is_directory(dir, ec) && !ec) {
        if (dirChecksum(dir) != pin.checksum) {
          ok = false;
          why = "content checksum mismatch";
        } else if (!packageDirSafe(dir, why)) {
          ok = false;
          why = std::string("unsafe entries ('") + why + "')";
        }
      }
      if (ok) {
        std::printf("OK %s %s (registry)\n", pin.name.c_str(),
                    pin.version.c_str());
      } else {
        std::printf("FAIL %s %s (registry): %s\n", pin.name.c_str(),
                    pin.version.c_str(), why.c_str());
        fails++;
      }
    }
  }
  std::printf("verify: %d failed, %d skipped\n", fails, skips);
  return fails ? 1 : 0;
}

static int cmdOutdated(const std::vector<std::string> &args) {
  std::string regFlag;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--registry" && i + 1 < args.size()) {
      regFlag = args[++i];
    } else {
      std::fprintf(stderr, "error: unexpected argument '%s'\n",
                   args[i].c_str());
      return 1;
    }
  }
  InspRoot ins;
  std::string err;
  if (!loadInspect(ins, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  if (!ins.haveLock) {
    std::fprintf(stderr, "error: no nusa.lock (run `nusa pkg update` first)\n");
    return 1;
  }
  std::vector<std::string> regs = registryList(regFlag);
  bool any = false;
  for (const auto &d : ins.m.deps) {
    if (d.isPath())
      continue;
    const LockPkg *pin = findPin(ins.pins, d.name);
    if (!pin) {
      std::printf("%s %s: unlocked\n", d.name.c_str(), d.version.c_str());
      any = true;
      continue;
    }
    std::string latest;
    for (const auto &reg : regs) {
      std::vector<IndexEntry> entries;
      std::string rerr;
      if (!readIndex(reg, d.name, entries, rerr))
        continue;
      for (const auto &e : entries) {
        int a, b, c;
        std::string pre;
        if (!parseSemver(e.version, a, b, c, pre))
          continue;
        if (latest.empty() || cmpSemver(e.version, latest) > 0)
          latest = e.version;
      }
    }
    if (latest.empty()) {
      std::printf("%s %s: locked %s (no registry data)\n", d.name.c_str(),
                  d.version.c_str(), pin->version.c_str());
      any = true;
    } else if (cmpSemver(latest, pin->version) > 0) {
      std::printf("%s %s: locked %s -> latest %s%s\n", d.name.c_str(),
                  d.version.c_str(), pin->version.c_str(), latest.c_str(),
                  satisfiesReq(latest, d.version) ? "" : " (beyond req)");
      any = true;
    }
  }
  if (!any)
    std::printf("all dependencies up to date\n");
  return 0;
}

static int cmdAudit(const std::vector<std::string> &args) {
  for (const auto &a : args) {
    std::fprintf(stderr, "error: unexpected argument '%s'\n", a.c_str());
    return 1;
  }
  InspRoot ins;
  std::string err;
  if (!loadInspect(ins, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  // Closure manifest: root + path-dep rekursif (aman-siklus).
  std::printf("package %s %s [%s]\n", ins.m.name.c_str(),
              ins.m.version.c_str(), capabilitiesSummary(ins.m).c_str());
  std::set<std::string> seen;
  std::vector<std::pair<std::string, Manifest>> stack;
  stack.push_back({ins.rootDir, ins.m});
  seen.insert(abspath(".", ins.rootDir));
  while (!stack.empty()) {
    auto cur = stack.back();
    stack.pop_back();
    for (const auto &d : cur.second.deps) {
      if (!d.isPath())
        continue;
      std::string dirAbs = abspath(cur.first, d.path);
      if (!seen.insert(dirAbs).second)
        continue;
      std::error_code ec;
      std::string manPath = dirAbs + "/nusa.toml";
      Manifest dm;
      std::string derr;
      if (!fs::is_regular_file(manPath, ec) || ec ||
          !parseManifest(manPath, dm, derr)) {
        std::printf("dep %s (path): unreadable\n", d.name.c_str());
        continue;
      }
      const LockPkg *pin = findPin(ins.pins, dm.name);
      std::printf("dep %s %s [%s] (%s)\n", dm.name.c_str(),
                  dm.version.c_str(), capabilitiesSummary(dm).c_str(),
                  pin ? pin->checksum.c_str() : "unlocked");
      stack.push_back({dirAbs, dm});
    }
  }
  // Pin registry: capability dari manifest cache bila ada.
  for (const auto &pin : ins.pins) {
    if (pin.registry.empty())
      continue;
    std::string dir = cacheRoot() + "/" + regId(pin.registry) + "/" +
                      pin.name + "/" + pin.version + "/pkg";
    std::error_code ec;
    std::string manPath = dir + "/nusa.toml";
    Manifest dm;
    std::string derr;
    if (fs::is_regular_file(manPath, ec) && !ec &&
        parseManifest(manPath, dm, derr)) {
      std::printf("dep %s %s [%s] (%s)\n", pin.name.c_str(),
                  pin.version.c_str(), capabilitiesSummary(dm).c_str(),
                  pin.checksum.c_str());
    } else {
      std::printf("dep %s %s [?] (%s, not cached)\n", pin.name.c_str(),
                  pin.version.c_str(), pin.checksum.c_str());
    }
  }
  std::printf("signatures: unavailable (checksums only)\n");
  return 0;
}

// SBOM v1 (Phase 113): inventaris JSON deterministik dari manifest +
// lock (tanpa kriptografi baru — checksum sha256 yang ada). Skema:
// docs/supply-3.14.md. Urutan: root, path-deps (traversal audit),
// registry-pins (urutan lock).
static void sbomStr(std::string &o, const std::string &s) {
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
        snprintf(b, sizeof(b), "\\u%04x", c);
        o += b;
      } else {
        o.push_back(static_cast<char>(c));
      }
    }
  }
  o.push_back('"');
}

static void sbomCaps(std::string &o, const Manifest &m) {
  o += "[";
  bool first = true;
  const char *names[5] = {"filesystem", "network", "process", "environment",
                          "native"};
  bool vals[5] = {m.caps.filesystem, m.caps.network, m.caps.process,
                  m.caps.environment, m.caps.native};
  for (int i = 0; i < 5; i++) {
    if (!vals[i])
      continue;
    if (!first)
      o.push_back(',');
    first = false;
    sbomStr(o, names[i]);
  }
  o += "]";
}

static int cmdSbom(const std::vector<std::string> &args) {
  for (const auto &a : args) {
    std::fprintf(stderr, "error: unexpected argument '%s'\n", a.c_str());
    return 1;
  }
  InspRoot ins;
  std::string err;
  if (!loadInspect(ins, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  std::string o = "{\"sbomFormat\":\"nusa-sbom/1\",";
  o += "\"tool\":";
  sbomStr(o, std::string("nusa ") + compilerVersion());
  o += ",\"integrity\":{\"scheme\":\"sha256-checksum\",\"signatures\":false},";
  o += "\"root\":{\"name\":";
  sbomStr(o, ins.m.name);
  o += ",\"version\":";
  sbomStr(o, ins.m.version);
  o += ",\"capabilities\":";
  sbomCaps(o, ins.m);
  o += "},\"components\":[";
  bool first = true;
  auto comp = [&](const std::string &name, const std::string &version,
                  const std::string &source, const std::string &checksum,
                  const Manifest *dm, bool cached) {
    if (!first)
      o.push_back(',');
    first = false;
    o += "{\"name\":";
    sbomStr(o, name);
    o += ",\"version\":";
    sbomStr(o, version);
    o += ",\"source\":";
    sbomStr(o, source);
    o += ",\"checksum\":";
    sbomStr(o, checksum);
    o += ",\"capabilities\":";
    if (dm)
      sbomCaps(o, *dm);
    else
      o += "null";
    o += ",\"cached\":";
    o += cached ? "true" : "false";
    o += "}";
  };
  // Path-deps (traversal cermin audit, aman-siklus).
  std::set<std::string> seen;
  std::vector<std::pair<std::string, Manifest>> stack;
  stack.push_back({ins.rootDir, ins.m});
  seen.insert(abspath(".", ins.rootDir));
  while (!stack.empty()) {
    auto cur = stack.back();
    stack.pop_back();
    for (const auto &d : cur.second.deps) {
      if (!d.isPath())
        continue;
      std::string dirAbs = abspath(cur.first, d.path);
      if (!seen.insert(dirAbs).second)
        continue;
      std::error_code ec;
      std::string manPath = dirAbs + "/nusa.toml";
      Manifest dm;
      std::string derr;
      if (!fs::is_regular_file(manPath, ec) || ec ||
          !parseManifest(manPath, dm, derr)) {
        comp(d.name, "", std::string("path:") + d.path, "unreadable",
             nullptr, true);
        continue;
      }
      const LockPkg *pin = findPin(ins.pins, dm.name);
      comp(dm.name, dm.version, std::string("path:") + d.path,
           pin ? pin->checksum : "unlocked", &dm, true);
      stack.push_back({dirAbs, dm});
    }
  }
  // Registry-pins (urutan lock).
  for (const auto &pin : ins.pins) {
    if (pin.registry.empty())
      continue;
    std::string dir = cacheRoot() + "/" + regId(pin.registry) + "/" +
                      pin.name + "/" + pin.version + "/pkg";
    std::error_code ec;
    std::string manPath = dir + "/nusa.toml";
    Manifest dm;
    std::string derr;
    bool cached = fs::is_regular_file(manPath, ec) && !ec &&
                  parseManifest(manPath, dm, derr);
    comp(pin.name, pin.version, std::string("registry:") + pin.registry,
         pin.checksum, cached ? &dm : nullptr, cached);
  }
  o += "]}";
  std::printf("%s\n", o.c_str());
  return 0;
}

static int cmdInstall(Driver &driver, const std::vector<std::string> &args) {
  std::string spec;
  std::string regFlag;
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == "--registry" && i + 1 < args.size()) {
      regFlag = args[++i];
    } else if (!args[i].empty() && args[i][0] != '-') {
      if (!spec.empty()) {
        std::fprintf(stderr, "error: unexpected argument '%s'\n",
                     args[i].c_str());
        return 1;
      }
      spec = args[i];
    } else {
      std::fprintf(stderr, "error: unknown option '%s'\n", args[i].c_str());
      return 1;
    }
  }
  if (spec.empty()) {
    std::fprintf(stderr,
                 "error: usage: nusa install <name>[@req] [--registry DIR]\n");
    return 1;
  }
  std::string name = spec;
  std::string req;
  size_t at = spec.find('@');
  if (at != std::string::npos) {
    name = spec.substr(0, at);
    req = spec.substr(at + 1);
  }
  if (!validName(name)) {
    std::fprintf(stderr, "error: invalid package name '%s'\n", name.c_str());
    return 1;
  }
  std::string manPath = findManifestUp();
  if (manPath.empty()) {
    std::fprintf(stderr, "error: no nusa.toml found (run inside a package)\n");
    return 1;
  }
  std::vector<std::string> regs = registryList(regFlag);
  if (regs.empty()) {
    std::fprintf(stderr,
                 "error: no registry (set $NUSA_REGISTRY or --registry)\n");
    return 1;
  }
  // Pilih versi dulu (untuk bentuk req yang disimpan).
  IndexEntry sel;
  bool got = false;
  std::string lastErr;
  for (const auto &reg : regs) {
    std::vector<IndexEntry> entries;
    if (!readIndex(reg, name, entries, lastErr))
      continue;
    IndexEntry cand;
    if (selectVersion(entries, req, cand, lastErr, name)) {
      sel = cand;
      got = true;
      break;
    }
  }
  if (!got) {
    std::fprintf(stderr, "error: %s\n",
                 lastErr.empty() ? "package not found" : lastErr.c_str());
    return 1;
  }
  // Bentuk req tersimpan: bare → caret; eksplisit → apa adanya.
  std::string stored = req.empty() ? ("^" + sel.version) : req;
  std::vector<std::string> addArgs = {name, stored};
  // (cmdAdd menolak duplikat; update dikerjakan di bawah via build.)
  Manifest m;
  std::string err;
  if (!parseManifest(manPath, m, err)) {
    std::fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  bool present = false;
  for (const auto &d : m.deps) {
    if (d.name == name)
      present = true;
  }
  if (!present) {
    int rc = cmdAdd(addArgs);
    if (rc != 0)
      return rc;
  }
  std::string rootDir = fs::path(manPath).parent_path().string();
  if (rootDir.empty())
    rootDir = ".";
  (void)driver;
  return cmdUpdateAt(rootDir, regs);
}

int runPkg(Driver &driver, const std::vector<std::string> &args) {
  if (args.empty()) {
    std::fprintf(stderr,
                 "error: usage: nusa pkg <init|add|remove|update|build|publish|search|install|tree|verify|outdated|audit> [...]\n");
    return 1;
  }
  const std::string &cmd = args[0];
  std::vector<std::string> rest(args.begin() + 1, args.end());
  if (cmd == "init")
    return cmdInit(rest);
  if (cmd == "add")
    return cmdAdd(rest);
  if (cmd == "remove")
    return cmdRemove(rest);
  if (cmd == "update")
    return cmdUpdate(rest);
  if (cmd == "build")
    return cmdBuild(driver, rest);
  if (cmd == "publish")
    return cmdPublish(driver, rest);
  if (cmd == "search")
    return cmdSearch(rest);
  if (cmd == "install")
    return cmdInstall(driver, rest);
  if (cmd == "tree")
    return cmdTree(rest);
  if (cmd == "verify")
    return cmdVerify(rest);
  if (cmd == "outdated")
    return cmdOutdated(rest);
  if (cmd == "audit")
    return cmdAudit(rest);
  if (cmd == "sbom")
    return cmdSbom(rest);
  std::fprintf(stderr, "error: unknown pkg command '%s'\n", cmd.c_str());
  return 1;
}

} // namespace pkg
} // namespace nusa
