#pragma once
#include <string>

namespace nusa {

// Generator dokumentasi API v0.9.0 (`nusa doc`): Markdown dari satu file
// `.ns` — signature fungsi/struct + komentar `///` yang menempel tepat
// di atasnya. Item tanpa doc tetap dicantumkan ("No documentation").
// Metadata JSON v3.9.0 (Phase 108): render mesin dari data yang SAMA
// (fungsi/struct/alias/const/enum/trait/extern + tipe + doc) untuk
// serialization/codegen/tooling — tanpa refleksi runtime bahasa.
// Import TIDAK diikuti (satu file; gabungkan output bila perlu).
struct DocResult {
  std::string markdown;
  bool ok = true;
  std::string error;
};

DocResult generateDoc(const std::string &file, const std::string &source);

// Render JSON dari sumber yang sama (skema: docs/reflection-3.9.md).
// Gagal (lex/parse) -> r.ok == false dengan r.error yang sama.
std::string generateDocJson(const std::string &file, const std::string &source,
                            bool &ok, std::string &error);

} // namespace nusa
