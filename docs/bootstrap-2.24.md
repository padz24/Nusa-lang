# Compiler Bootstrap 2.0 — Design (Phase 73, target 2.24.0)

Status: rantai multi-komponen (lexer + parser). Payung master:
PHASE 73 — COMPILER BOOTSTRAP 2.0 ("frontend utama semakin
banyak menggunakan Nusa"; C++ tetap untuk runtime/LLVM/
tooling/bootstrap layer). Spec bahasa tak berubah.

## 1. Generalisasi rantai (satu pola, N komponen)

Kontrak komponen (docs/bootstrap.md, sejak Phase 39) sudah
dipenuhi `parse.ns` (check hijau, argv(1), stdout stabil,
deterministik) — kecuali referensi Stage 0-nya `nusa ast`,
bukan `nusa lex`. Maka `runBootstrap` diparameterisasi kind:

```
nusa bootstrap [file.ns] [--as lex|ast] [-o DIR]
```

- `--as` default `lex` (kompatibel: semua invokasi lama sama).
- Artefak Stage 1: `<DIR>/<base>_stage1` (`lex.ns`→`lex_stage1`
  sama seperti dulu; `parse.ns`→`parse_stage1`).
- Referensi Stage 0 per kind (helper bersama, tanpa duplikasi
  logika runLex/runAst): lex = dump token; ast = lex+parse+
  `program.dump()`.
- Stage 2 = self-application (komponen memproses dirinya
  sendiri — kasus terkuat) 2x (determinisme) + banding Stage 0.

## 2. Yang TIDAK berubah

- C++ tetap bootstrap layer + referensi Stage 0 (differential
  butuh oracle independen — mengganti referensi dengan Nusa
  meruntuhkan pembuktian).
- Korpus valid saja (batas selfhosting.md berlaku).
- `e2e_selfhost.sh`/`e2e_parse.sh` tetap oracle diferensial
  per-komponen; bootstrap mengunci RANTAI (build+repro).

## 3. Peta implementasi

- `Driver::runBootstrap(path, outDir, kind)` + helper
  `stage0Ref`; `main.cpp`: flag `--as` (nilai selain lex|ast
  = error) + help text.
- `e2e_bootstrap.sh`: seksi parse-chain (bootstrap penuh +
  standalone vs `nusa ast` + determinisme) + negatif `--as`.
- Docs: `docs/bootstrap.md` (rantai ganda), `compiler.md`
  +1 baris, `CHANGELOG.md` [2.24.0], regen website.

## 4. DITUNDA (Phase 74 FULL)

Semantic/checker selfhost (butuh tabel simbol growable —
penelitian terpisah), rantai 3+ komponen otomatis,
Stage N == Stage N+1 antar-biner Nusa (belum ada 2 biner
turunan untuk dibandingkan).

## 5. Acceptance criteria

- `nusa bootstrap` lama byte-perilaku-sama (4 kasus lama hijau);
  `nusa bootstrap parse.ns --as ast` PASS + artefak mandiri;
  suite hijau; bench identik; `.ns.ll` bersih.
