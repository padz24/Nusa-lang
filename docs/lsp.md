# Nusa Language Server (Phase 36, v1.17.0)

```sh
nusa lsp   # LSP via stdio (JSON-RPC + Content-Length)
```

Server bicara [LSP](https://microsoft.github.io/language-server-protocol/)
murni — tanpa protokol proprietary. Colokkan ke client apa pun
(VS Code, Neovim, helix, Emacs) sebagai `command = ["nusa", "lsp"]`.

## Fitur

| Fitur | Metode | Catatan |
|---|---|---|
| Diagnostics | `publishDiagnostics` | lexer + parser (recovery) + semantic, per open/change |
| Completion | `textDocument/completion` | keyword + builtin + simbol top-level + local terlihat |
| Hover | `textDocument/hover` | signature top-level, bantuan keyword |
| Goto definition | `textDocument/definition` | fungsi + local (scope-aware) |
| Symbols | `textDocument/documentSymbol` | fn/struct/enum(+varian)/trait(+metode)/alias + range |
| Formatting | `textDocument/formatting` | `nusa fmt` (gagal → null edit) |
| Rename | `textDocument/rename` | semua referensi satu definisi |
| References | `textDocument/references` | v2.14.0: def + uses (`includeDeclaration` dihormati) |

## Batas yang jujur (v1)

- **Satu file**: analisis teks terbuka saja (impor disk tak
  di-resolve; proyek multi-file terbaik via `nusa check`).
- **Tanpa typechecker**: hover local tanpa tipe; goto/rename/
  references sintaksis-scope (benar untuk shadowing, tanpa
  inferensi; himpunan references ≡ rename).
- **Tanpa goto untuk tipe** (struct/enum di posisi tipe), tanpa
  prepareRename/codeAction, tanpa folding/selectionRange.
- Posisi byte-based (ASCII tepat; non-ASCII pendekatan).
- `shutdown` → `exit` (kode 0 bila berurutan, 1 bila tidak);
  pesan rusak diabaikan (server tetap hidup); method tak dikenal
  → `-32601`.

## Klien minimal (uji manual)

Kirim (perhatikan `\r\n\r\n`):
```
Content-Length: 65\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}
```
lalu `initialized`, `textDocument/didOpen`, dst. Lihat
`compiler/tests/e2e_lsp.py` (harness python yang dipakai CI).
