#pragma once
#include <string>

namespace nusa {

// Formatter Nusa sederhana (v0.9.0): normalisasi token-stream.
//
// Aturan (terdokumentasi, deterministik, idempoten):
//  - indentasi 4 spasi per kedalaman `{}`.
//  - `{` → ` {` + baris baru; `}` → baris baru + `}` (+ ` else` menempel).
//  - `;` mengakhiri baris. `,` → `, ` (tanpa spasi sebelum).
//  - `.` menempel dua sisi; `(`/`[` menempel ke kiri; `:` → `: `.
//  - operator biner/`->`/`=` dikelilingi spasi.
//  - string di-escape ulang; integer/ident/keyword apa adanya.
//  - komentar `//` dipertahankan: sebaris sendiri (indentasi berjalan)
//    atau trailing (`code; // note`). `///` diperlakukan sama.
//  - tepat satu newline di akhir; tanpa spasi ekor.
struct FormatResult {
  std::string text;
  bool ok = true;
  std::string error;
};

FormatResult formatSource(const std::string &file, const std::string &source);

} // namespace nusa
