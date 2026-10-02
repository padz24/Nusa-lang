#pragma once
// Nusa Language Server (Phase 36, v1.17.0): LSP via stdio.
// Fitur: diagnostics, completion, hover, goto-definition,
// document symbols, formatting, rename. Lihat docs/lsp.md.
#include <string>

namespace nusa {
namespace lsp {

// Loop server di stdin/stdout (JSON-RPC + Content-Length).
// Kembali saat `exit` (kode = 0 bila didahului `shutdown`).
int run();

} // namespace lsp
} // namespace nusa
