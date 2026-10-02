#pragma once
// Nusa → header C (Phase 110, milestone 1): subset API Nusa yang
// representasi C-nya jujur (lih. docs/abi-3.10.md) diekspor sebagai
// deklarasi C; sisanya dilewati + warning beralasan (bukan fatal).
// Kebalikan `nusa bindgen` (C → Nusa). Tanpa sintaks/semantik baru.
#include <string>
#include <vector>

namespace nusa {
namespace headers {

struct HeaderResult {
  bool ok = false;
  std::string error;                 // bila !ok
  std::string code;                  // header .h
  std::vector<std::string> warnings; // skip beralasan (tak fatal)
};

// Hasilkan header C dari isi file .ns. ok=false bila kosong/tanpa
// item tereksposikan.
HeaderResult generateHeader(const std::string &file,
                            const std::string &source);

} // namespace headers
} // namespace nusa
