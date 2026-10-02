# Bootstrap Full 4-Stage — Design (Phase 74, target 2.25.0)

Status: rantai penuh + validasi Stage N == Stage N+1 + dokumentasi
lengkap. Mandat master: PHASE 74 — SELF-HOSTING COMPILER FULL
(Stage 0→1→2→3, kesetaraan "sebisa mungkin", Target 2.25.0).
Spec bahasa tak berubah.

## 1. Peta 4 stage (operasional, dikunci mesin)

| Stage | Apa | Dibangun oleh | Output |
|---|---|---|---|
| 0 | compiler C++ (`build/nusa`) | cmake+LLVM | oracle |
| 1 | komponen Nusa → native (`<base>_stage1`, `--fresh`) | Stage 0 | biner |
| 2 | biner Stage 1 atas sumbernya sendiri, 2x | Stage 1 | dump D2 |
| 3 | REBUILD Stage 1 dari nol (`--fresh`, dir lain) + 1x run | Stage 0 | dump D3 |

Validasi (di `nusa bootstrap`, semua byte-identik):

- D2a == D2b (determinisme Stage 1).
- D2 == Stage 0 (kebenaran vs oracle independen).
- D3 == D2 (reproduksibilitas rebuild — "rebuild compiler
  using itself": tidak ada timestamp/path absolut yang bocor
  ke artefak perilaku).

## 2. Batas jujur "sebisa mungkin"

Frontend Nusa berhenti di dump AST (tanpa semantik/MIR/IR/
codegen) — maka "compiler builds itself" terpenuhi untuk
awalan pipeline (lex→parse), bukan ujung LLVM. Tabel jujur
Nusa-vs-C++ per tahap pipeline ada di §3 dokumen ini
(dičermin di `docs/bootstrap.md`).

## 3. Tabel Nusa-vs-C++ per tahap pipeline

| Tahap | Nusa | C++ | Validasi |
|---|---|---|---|
| lexing | `lex.ns` | `compiler/lexer` | `e2e_selfhost` + rantai |
| parsing+AST | `parse.ns` | `compiler/parser+ast` | `e2e_parse` + rantai |
| semantic | — | `compiler/semantic` | (Phase 74+: penelitian) |
| MIR/opt | — | `compiler/mir` | `test_mir`/`e2e_opt` |
| LLVM IR/codegen | — | `compiler/codegen` + LLVM 21.1.8 | `verifyModule`/IR spot |
| runtime | — | `runtime/` + libc | e2e runtime |

## 4. Peta implementasi

- `runBootstrapKind(path, outDir, kind, rebuildDir="")`:
  rebuildDir kosong = perilaku 2.24.0 persis; terisi =
  Stage 3 (fresh-rebuild + 1 run + banding D2, diagnosis
  beda-pertama cermin Stage 2).
- CLI: `--rebuild DIR` + help; nilai `--as` tetap lex|ast.
- `e2e_bootstrap.sh`: kasus rebuild lex (cepat) + rebuild
  parse (penuh) + negatif; ambang PASS 7→10 kasus.
- Docs: file ini (proses lengkap + reproduksi manual +
  diagnosis) + `docs/bootstrap.md` pointer + `compiler.md`
  + `CHANGELOG.md` [2.25.0] + regen website.

## 5. Acceptance criteria

- `nusa bootstrap` tanpa `--rebuild` byte-perilaku-sama
  (7 kasus lama hijau); dengan `--rebuild` mencetak Stage 3
  + REBUILD PASS untuk lex dan parse; suite hijau (73+);
  bench identik; `.ns.ll` bersih.
