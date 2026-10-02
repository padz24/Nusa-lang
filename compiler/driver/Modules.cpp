#include "compiler/driver/Modules.h"
#include "compiler/diagnostics/Diagnostic.h"
#include "compiler/lexer/Lexer.h"
#include "compiler/parser/Parser.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace nusa {
namespace fs = std::filesystem;

std::string ModuleLoader::normalize(const std::string &path) {
  // Tanpa exceptions (build memakai -fno-exceptions): pakai error_code.
  std::error_code ec;
  fs::path abs = fs::absolute(fs::path(path), ec);
  if (ec)
    return path;
  return abs.lexically_normal().string();
}

std::string ModuleLoader::displayOf(const std::string &absPath) {
  std::error_code ec;
  fs::path cwd = fs::current_path(ec);
  if (!ec) {
    fs::path rel = fs::relative(fs::path(absPath), cwd, ec);
    if (!ec && !rel.empty() && rel.native().rfind("..", 0) != 0)
      return rel.string();
  }
  return absPath;
}

std::string ModuleLoader::parentDir(const std::string &absPath) {
  std::error_code ec;
  fs::path parent = fs::path(absPath).parent_path();
  (void)ec;
  std::string s = parent.string();
  return s.empty() ? "." : s;
}

bool ModuleLoader::load(const std::string &entryPath, Program &merged,
                        std::vector<LoadedModule> &modules,
                        std::vector<ModuleError> &errors) {
  index_.clear();
  stack_.clear();
  modules_ = &modules;
  errors_ = &errors;
  modules.clear();
  errors.clear();

  std::string absEntry = normalize(entryPath);
  std::string dispEntry = entryPath;
  // Entry tanpa direktori (mis. "main.ns") tampil apa adanya.
  if (entryPath.find('/') == std::string::npos &&
      entryPath.find('\\') == std::string::npos)
    dispEntry = entryPath;
  else
    dispEntry = displayOf(absEntry);
  Location entryLoc{entryPath, 1, 1};
  if (!loadFile(absEntry, dispEntry, entryLoc, stack_))
    return false;

  // Gabung: dependensi dulu (urutan discovery dibalik), main terakhir.
  // (Urutan tak memengaruhi hasil — semua tahap two-pass.)
  merged.file = entryPath;
  merged.imports.clear();
  merged.structs.clear();
  merged.aliases.clear();
  merged.traits.clear();
  merged.impls.clear();
  merged.enums.clear();
  merged.consts.clear();
  merged.functions.clear();
  for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
    for (auto &sd : it->program.structs)
      merged.structs.push_back(std::move(sd));
    for (auto &ad : it->program.aliases)
      merged.aliases.push_back(std::move(ad));
    for (auto &td : it->program.traits)
      merged.traits.push_back(std::move(td));
    for (auto &ib : it->program.impls)
      merged.impls.push_back(std::move(ib));
    for (auto &ed : it->program.enums)
      merged.enums.push_back(std::move(ed));
    for (auto &cd : it->program.consts)
      merged.consts.push_back(std::move(cd));
    for (auto &fn : it->program.functions)
      merged.functions.push_back(std::move(fn));
  }
  return errors.empty();
}

bool ModuleLoader::loadFile(const std::string &absPath,
                            const std::string &display,
                            const Location &importLoc,
                            std::vector<std::string> &stack) {
  // Batas kedalaman rantai import (v0.9.5 hardening): loadFile rekursif,
  // rantai linear ribuan modul akan menghabiskan stack C++.
  if (stack.size() > 100) {
    errors_->push_back(ModuleError{
        importLoc, "import chain too deep (over 100 modules)",
        "an import chain this deep is almost certainly a design error"});
    return false;
  }
  // Siklus: target sudah ada di rantai aktif.
  for (size_t i = 0; i < stack.size(); i++) {
    if (stack[i] == absPath) {
      std::string chain;
      for (size_t j = i; j < stack.size(); j++) {
        if (!chain.empty())
          chain += " -> ";
        chain += displayOf(stack[j]);
      }
      chain += " -> " + display;
      errors_->push_back(ModuleError{
          importLoc, "circular import detected: " + chain,
          "break the cycle by moving the shared code into a third module"});
      return false;
    }
  }
  // Sudah dimuat (bukan siklus): dedupe diam-diam.
  if (index_.count(absPath))
    return true;

  std::ifstream in(absPath, std::ios::binary);
  if (!in) {
    errors_->push_back(ModuleError{
        importLoc, "cannot read module file '" + display + "'",
        "make sure '" + display + "' exists in the importing directory"});
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();

  // Lex + parse dengan identitas file modul ini (diagnostic tepat file).
  Lexer lexer(display, ss.str());
  std::vector<Token> tokens = lexer.tokenize();
  bool lexOk = lexer.errors().empty();
  std::vector<std::string> lines =
      DiagnosticEngine::splitLines(ss.str());
  Parser parser(display, std::move(tokens), lines);
  ParseResult pres = parser.parse();

  size_t idx = modules_->size();
  index_[absPath] = idx;
  LoadedModule mod;
  mod.path = absPath;
  mod.display = display;
  mod.lines = std::move(lines);
  mod.program = std::move(pres.program);
  mod.program.file = display;
  modules_->push_back(std::move(mod));

  // Kumpulkan error lex/parse modul ini (tetap lanjutkan modul lain
  // agar semua error terlapor sekaligus).
  bool ok = true;
  if (!lexOk) {
    for (const auto &e : lexer.errors()) {
      // Error lexer sudah berformat "file:line:col: ..." — teruskan verbatim.
      errors_->push_back(ModuleError{Location{display, 0, 0}, e, "", true});
    }
    ok = false;
  }
  for (const auto &e : pres.errors) {
    errors_->push_back(ModuleError{e.loc, e.message, e.hint});
    ok = false;
  }

  // Rekursi ke import modul ini.
  stack.push_back(absPath);
  std::string dir = parentDir(absPath);
  // NOTE: mod dipindah ke modules_; ambil ulang referensinya.
  LoadedModule &stored = (*modules_)[idx];
  // Salin daftar import dulu (hindari referensi basi bila vector tumbuh).
  std::vector<Import> imports = stored.program.imports;
  for (const auto &im : imports) {
    Location useLoc = im.loc;
    useLoc.file = display;
    // Cari kandidat pertama yang ada (lokal → NUSA_PATH → std).
    std::string target;
    bool found = false;
    for (const auto &cand : candidates(absPath, im.module)) {
      if (fileExists(cand)) {
        target = cand;
        found = true;
        break;
      }
    }
    if (!found) {
      // Pesan memakai kandidat lokal (paling mungkin dimaksud user).
      std::string local =
          normalize(dir + "/" + im.module + ".ns");
      errors_->push_back(ModuleError{
          useLoc, "cannot read module file '" + displayOf(local) + "'",
          "make sure '" + im.module + ".ns' exists in the importing directory, "
          "$NUSA_PATH, or std (" +
              displayOf(normalize(exeStdDir() + "/" + im.module + ".ns")) +
              ")"});
      ok = false;
      continue;
    }
    std::string targetDisp = displayOf(target);
    if (!loadFile(target, targetDisp, useLoc, stack))
      ok = false;
  }
  stack.pop_back();
  return ok;
}

// Direktori exe (/proc/self/exe; "" bila gagal).
static std::string exeDirOf() {  char buf[4096];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0)
    return "";
  buf[n] = '\0';
  std::string p(buf);
  size_t slash = p.find_last_of('/');
  if (slash == std::string::npos)
    return "";
  return p.substr(0, slash);
}

std::string ModuleLoader::exeStdDir() {  std::string exe = exeDirOf();
  if (exe.empty())
    return "";
  return normalize(exe + "/../std");
}

std::string ModuleLoader::exeDir() { return exeDirOf(); }

std::vector<std::string>
ModuleLoader::candidates(const std::string &importerAbs,
                         const std::string &name) {
  std::vector<std::string> out;
  // Kanonis `std.x` (Phase 29): prefix `std.` dipetakan ke modul datar
  // (`import std.time;` ≡ `import time;`).
  std::string base = name;
  const std::string prefix = "std.";
  if (base.size() > prefix.size() &&
      base.compare(0, prefix.size(), prefix) == 0)
    base = base.substr(prefix.size());
  out.push_back(normalize(parentDir(importerAbs) + "/" + base + ".ns"));
  if (const char *env = std::getenv("NUSA_PATH")) {
    std::string paths(env);
    size_t start = 0;
    while (start <= paths.size()) {
      size_t sep = paths.find(':', start);
      std::string dir = paths.substr(
          start, sep == std::string::npos ? sep : sep - start);
      if (!dir.empty())
        out.push_back(normalize(dir + "/" + base + ".ns"));
      if (sep == std::string::npos)
        break;
      start = sep + 1;
    }
  }
  std::string std = exeStdDir();
  if (!std.empty())
    out.push_back(normalize(std + "/" + base + ".ns"));
  return out;
}

bool ModuleLoader::fileExists(const std::string &absPath) {
  std::error_code ec;
  return fs::is_regular_file(fs::path(absPath), ec) && !ec;
}

} // namespace nusa
