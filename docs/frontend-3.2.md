# Compiler Frontend 2.0 — Milestone 1: Akurasi Diagnostik (Phase 101)

Status: slice implementasi. Mandat master Phase 101 (lexer, parser,
AST, HIR, semantic, type checker; target 3.2.0). NOL perubahan
semantik bahasa; NOL rewrite (aturan: tanpa design review).

## 1. Inspeksi → bukti (pemilihan milestone)

- `--stats` (Phase 65): `selfhost/parse.ns` frontend 166ms dari
  total 4842ms (3.4%); `hello.ns` 32ms dari 2121ms. clang >90%.
  → Micro-opt frontend DITOLAK (bukti, bukan asumsi; cermin
  keputusan Phase 34).
- Recovery: probe manual 2 error parse lintas-fungsi → keduanya
  dilaporkan (`synchronize` blok/trait/top-level bekerja).
- Fuzz: `e2e_verify` seed-101 ×60 program → 72/72 hijau
  (0 crash/hang/mismatch).
- Adversarial: paren-2000 (guard depth), EOF tak-tutup, `<<<<`×500
  → semua error bersih rc=1, tanpa hang.
- Temuan arsitektur: lapisan HIR tak ada (hanya AST → MIR → LLVM);
  klaim "Nusa HIR" di asumsi master 4.0 overclaim — ranah Phase 102
  (NUSA IR 2.0), dicatat bukan dikerjakan.
- Celah nyata: 17 string user-facing berstempel versi basi
  (`... not supported in v0.7.0`, `v0.6.0 supports ...`,
  `unavailable in v2.11`, `... in v1`) — menyesatkan pasca-3.0.
  → Milestone 1 = sapu akurasi diagnostik (terukur: grep → 0).

## 2. Perubahan

- `compiler/semantic/TypeChecker.cpp` (15 situs): buang suffix
  ` in vX.Y.Z` / ` in v1` dari pesan + hint error (prefix
  dipertahankan — seluruh e2e/unit mencocokkan prefix).
- `compiler/parser/Parser.cpp`: hint `expected statement`
  `v0.6.0 supports ...` → contoh bentuk aktual
  (let/print/return/if/while/match/assign/call).
- `compiler/pkg/Pkg.cpp`: `nusa audit` → `signatures: unavailable
  (checksums only)`.
- Regresi baru: `testStmtHintListsCurrentForms`
  (`compiler/tests/test_parser.cpp`) — kunci pesan + daftar
  bentuk + anti-regresi stempel versi.

## 3. Diverifikasi

- Unit: `test_parser` + `test_typecheck` + `test_pkg` hijau.
- E2E pesan-berat: alias 11/11, fn 14/14, traits 18/18, eco2 13/13,
  diagnostics 5/5, error 9/9, hardening 37/37, tooling PASS.
- `e2e_release` 6/6 pasca-perubahan; program `.ns` + IR `@main` OK.
- Bench: tak relevan (NOL jalur performa tersentuh).

## 4. Ditunda jujur

- Quirk `a < (b)` → `a <(b)` (Phase 100); reflow struct
  `selfhost/parse.ns`; tulis-ulang spasi string `string2_test.ns`
  — tetap terdaftar, bukan slice ini.
- HIR, incremental parsing, memory-opt frontend: butuh design
  review (Phase 102+).
