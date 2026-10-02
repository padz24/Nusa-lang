#pragma once
#include "compiler/ast/AST.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace nusa {

// Module system v0.8.0 (tanpa package manager).
//
//  - `import math;` di file F memuat `<dir(F)>/math.ns` (teksual, flat).
//  - Namespace per modul; import menggabung nama PUBLIK ke program:
//    konflik (duplikat) ditolak di semantic, bukan di sini.
//  - Visibilitas: nama `_`-awalan modul-private (dicek semantic via file).
//  - Circular import dideteksi dengan pesan rantai yang jelas.
//  - Hasil: satu Program gabungan (imports dikonsumsi) + tabel baris
//    per file untuk diagnostic multi-file yang tepat.
struct LoadedModule {
  std::string path; // identitas absolut ternormalisasi
  std::string display; // untuk pesan (relatif bila bisa)
  std::vector<std::string> lines;
  Program program;
};

struct ModuleError {
  Location loc;
  std::string message;
  std::string hint;
  // true: message sudah berformat lengkap ("f:l:c: ..."), cetak verbatim
  // (dipakai untuk error lexer yang diformat di tempat).
  bool verbatim = false;
  // Panjang underline (lihat SemanticError); default 1.
  int length = 1;
};

class ModuleLoader {
public:
  // Muat entry + semua import transitif ke `merged`.
  // false → errors terisi (file hilang, parse gagal, circular, ...).
  bool load(const std::string &entryPath, Program &merged,
            std::vector<LoadedModule> &modules,
            std::vector<ModuleError> &errors);
  // "<exe>/../std" ("" bila exe tak diketahui). Publik untuk
  // penemuan runtime C oleh driver (Phase 28).
  static std::string exeStdDir();
  // Direktori executable ("" bila tak diketahui). Publik untuk
  // penemuan objek runtime precompiled (Phase 34).
  static std::string exeDir();

private:
  bool loadFile(const std::string &path, const std::string &display,
                const Location &importLoc, std::vector<std::string> &stack);

  static std::string normalize(const std::string &path);
  static std::string displayOf(const std::string &absPath);
  static std::string parentDir(const std::string &absPath);
  // Kandidat path untuk `import name` (absen, berurutan prioritas):
  // direkori pengimpor, $NUSA_PATH, <exe>/../std.
  std::vector<std::string> candidates(const std::string &importerAbs,
                                      const std::string &name);
  static bool fileExists(const std::string &absPath);

  std::unordered_map<std::string, size_t> index_; // abs path → modules idx
  std::vector<LoadedModule> *modules_ = nullptr;
  std::vector<ModuleError> *errors_ = nullptr;
  std::vector<std::string> stack_; // rantai import (abs) untuk siklus
};

} // namespace nusa
