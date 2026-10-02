#pragma once
// REPL Nusa — milestone 1 (Phase 106): loop baca-evaluasi-cetak di atas
// infrastruktur compiler yang SAMA (Driver::runBuild; tanpa bahasa
// kedua, tanpa JIT — lih. docs/execution-3.6.md, docs/repl-3.7.md).
// Implementasi di Repl.cpp sebagai Driver::runRepl (dideklarasikan di
// Driver.h agar konsisten dengan runBench/runCheck/dll).
#include <string>
#include <vector>

namespace nusa {
namespace repl {

// Tulis sesi (daftar baris) menjadi program main() lengkap.
std::string renderSession(const std::vector<std::string> &lines);

// Kedalaman {[(} dengan kesadaran string "..." (escape) + komentar
// // — untuk prompt lanjutan. <0 berarti kelebihan tutup (biar
// parser yang melapor bersih).
int scanDepth(const std::string &text);

// Baris tanpa ';' akhir = ekspresi → bungkus print(...); (konvensi
// REPL, bukan sintaks baru). Baris ';' = statement apa adanya.
std::string wrapLine(const std::string &line);

} // namespace repl
} // namespace nusa
