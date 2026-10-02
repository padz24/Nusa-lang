#pragma once
// Bindgen C subset → deklarasi `extern fn` Nusa (Phase 66).
// Murni C++ standar (tanpa lexer Nusa; C punya sintaks sendiri).
#include <string>
#include <vector>

namespace nusa {
namespace bindgen {

struct BindResult {
  bool ok = false;
  std::string error;                 // bila !ok
  std::string code;                  // binding .ns (fmt-clean)
  std::vector<std::string> warnings; // skip beralasan (tak fatal)
};

// Hasilkan binding dari isi header C. ok=false bila file kosong
// tanpa fungsi teremisikan (atau input kosong).
BindResult generateBindings(const std::string &file,
                            const std::string &source);

} // namespace bindgen
} // namespace nusa
