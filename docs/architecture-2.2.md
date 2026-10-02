# Arsitektur Compiler Nusa 2.2 (Phase 51 — Design Proposal)

Status: Proposed → Implementation (satu milestone). Spec bahasa tidak
berubah (normatif tetap `docs/language-spec-2.0.md`).

## 1. Lapisan saat ini (terukur 2.1.0, total ~26000 baris)

| Lapisan | Modul | LOC utama |
|---|---|---|
| Frontend | lexer (354), parser (1866), ast (1030), types, diagnostics | ~3500 |
| Semantic | Semantic (713+87), TypeChecker (4156+375) | ~5300 |
| Backend | codegen (2587+208) | ~2800 |
| Runtime | runtime/nusa_rt.c | ~409 |
| Tooling | fmt (447), doc (234), lsp (1600), incr (263), driver (755+347+268+362+353) | ~4300 |
| Package | pkg (2024+298) | ~2300 |

## 2. Kopling terukur (include antar-modul, 2.1.0)

Sehat (dipertahankan):

- Frontend hanya bergantung ke dalam/di samping (lexer/parser/ast/
  diagnostics/types). `fmt` → hanya `lexer`. `doc` → ast/lexer/
  parser/diagnostics. `Modules` → hanya ast.
- `Driver` → semua lapisan (wajar: orkestrator tunggal).
- `TypeChecker.h` → `Semantic.h` (butuh `SemanticError`; wajar:
  satu Semantic Layer).

Pelanggaran batas (temuan audit):

1. `codegen/Codegen.cpp` → `semantic/Semantic.h`, HANYA untuk
   `isBuiltin()` (satu fungsi murni tanpa state). Backend tidak
   boleh bergantung pada Semantic Layer.
2. `pkg/Pkg.cpp` → `driver/Driver.h` (`pkg::runPkg` menerima
   `Driver&`, `cmdBuild` memanggil `driver.runBuild`). Package
   System tidak boleh bergantung pada Driver (inversi: driver yang
   mengorkestrasi pkg). DITUNDA (lihat §4).
3. `incr/Incr.cpp` → `pkg/Pkg.h`, HANYA untuk `pkg::sha256Hex`.
   Utilitas checksum bukan pengetahuan paket. DITUNDA (lihat §4).
4. `lsp/Lsp.cpp` → `semantic/Semantic.h` (`SemanticAnalyzer`
   dipakai langsung). Tooling memakai semantic = dapat diterima
   untuk v1 (LSP satu-file), dicatat sebagai utang jujur.

## 3. Target (mandat master, tanpa rewrite total)

```
NUSA SOURCE → Lexer → Parser → AST → Semantic → Type System
   → HIR → MIR → LLVM IR → LLVM 21.1.8 → Target
```

HIR/MIR penuh = Phase 52. Phase 51 hanya menegakkan batas modul
agar HIR/MIR kelak punya tempat yang jelas.

## 4. Milestone 1 (SATU-SATUNYA perubahan kode Phase 51)

Ekstrak registry builtin ke lapisan Frontend:

- Baru: `compiler/frontend/Builtin.{h,cpp}` — `nusa::isBuiltin()`
  (implementasi dipindah verbatim dari `Semantic.cpp`; nol
  perubahan perilaku).
- `semantic/Semantic.h`: hapus deklarasi `isBuiltin`.
- `Semantic.cpp`, `TypeChecker.cpp`: include frontend baru.
- `codegen/Codegen.cpp`: ganti include `semantic/Semantic.h`
  → `frontend/Builtin.h`. Backend kini nol-include ke semantic/.
- `CMakeLists.txt`: tambah satu source ke `NUSA_LIB_SOURCES`.
- Batas mesin: `codegen/` tidak include `semantic/Semantic.h` /
  `semantic/TypeChecker.h` (dikunci `e2e_arch.sh`). `Scopes.h`
  (template utilitas header-only yang kebetulan tinggal di
  `semantic/`) dikecualikan eksplisit — kandidat pindah ke
  `support/` pada milestone berikut, bersama (2) dan (3).

DITUNDA dengan alasan (bukan diabaikan):

- (2) Pkg→Driver butuh interface build-request/callback; menyentuh
  alur `pkg build/publish/install`. Risiko sedang, dikerjakan
  sebagai milestone tersendiri setelah batas frontend stabil.
- (3) `sha256Hex` pindah ke modul support netral bersamaan dengan
  kebutuhan checksum berikutnya (reproducible builds, Phase 61).
- (4) LSP→semantic diterima untuk v1 (batas jujur `docs/lsp.md`).

## 5. Acceptance criteria

- `grep -rn 'semantic/' compiler/codegen/` kosong (dikunci test).
- Seluruh suite hijau (unit + e2e incl. `e2e_stability` baru Phase 50).
- Program `.ns` nyata + LLVM IR + benchmark identik baseline 2.1.0.
- Test baru: `compiler/tests/e2e_arch.sh` (batas lapisan mesin).
