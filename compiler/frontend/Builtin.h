#pragma once
// Frontend knowledge: builtin runtime names (Phase 51).
//
// Dipindah verbatim dari `compiler/semantic/Semantic.{h,cpp}` agar
// Backend (codegen) dan Semantic Layer berbagi satu definisi TANPA
// codegen bergantung pada Semantic Layer. Murni fungsi tanpa state;
// daftar = kontrak bahasa (tiap nama bertumbuh per fase, lih. bawah).
#include <string>

namespace nusa {

// Builtin global: dapat dipanggil tanpa deklarasi, tak bisa
// dideklarasikan ulang (reserved). Implementasi diemisikan langsung
// oleh codegen ke pemanggilan libc/LLVM (`genBuiltinCall`).
bool isBuiltin(const std::string &name);

} // namespace nusa
