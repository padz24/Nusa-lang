# Nusa 3.0.0 — Release Record (Phase 89, FINAL)

Status: final stable. Freeze sejak beta dipertahankan:
semantik bahasa (`docs/language-spec-3.0.md` normatif),
arsitektur compiler (`docs/compiler.md`), API standard library
(`docs/standard-library.md` v3.0.0), format paket/manifest
(`docs/pkg.md`). Delta rc.1 → final: hapus suffix
(`3.0.0-rc.1` → `3.0.0`, help `RC` → `Stable`), gate final
(`compiler/tests/e2e_release.sh`), record ini, changelog,
regen website. `e2e_beta`/`e2e_rc` dipertahankan sebagai regresi.

## 1. Penomoran

- `project(... VERSION 3.0.0)` = label manusia `NUSA_VERSION_STR
  = "3.0.0"` (sumber tunggal di CMakeLists; deb `3.0.0-1`).
- `e2e_new` (suffix-aware) + `e2e_repro` (pin dinamis) lolos
  tanpa perubahan; `e2e_beta`/`e2e_rc` dilonggarkan ke final
  sebagai regresi; ketegasan final ada di `e2e_release.sh`.

## 2. Definition of Done (§45 master)

- [x] Language specification stable (`language-spec-3.0.md` normatif)
- [x] Compiler stable (full suite hijau, §4)
- [x] Self-hosting compiler stable (`e2e_selfhost` + `e2e_parse`)
- [x] Bootstrap process documented (`bootstrap*.md`)
- [x] Memory model stable (`memory-model.md` + `mem-2.5.md`)
- [x] Type system stable (suite types/generik/trait)
- [x] Generic system stable (`e2e_generics` + `e2e_structs_gen`)
- [x] Trait system stable (`e2e_traits`)
- [x] Pattern matching stable (`e2e_match`)
- [x] Error handling stable (`e2e_error`)
- [x] Concurrency stable (`e2e_concurrency` + `e2e_stress`)
- [x] Async stable (`std/tasks.ns` + `e2e_async`; sintaks tetap reserved via RFC-0004)
- [x] Runtime stable (`e2e_runtime`, 29+ simbol identik)
- [x] Standard library mature (18 modul, `standard-library.md` v3.0.0)
- [x] Package manager stable (`e2e_pkg` + `e2e_eco2`)
- [x] Registry stable (`e2e_registry` + `e2e_deb`)
- [x] Reproducible builds (`e2e_repro`)
- [x] Security model documented (`security-2.10.md` + `security-3.0.md`; audit 0 critical/high)
- [x] FFI stable (`e2e_ffi` + `e2e_bindgen`)
- [x] Android AArch64 stable (target utama, CI ini)
- [x] Linux AArch64 stable (triple guard `e2e_cross`)
- [x] Linux x86_64 stable (triple guard `e2e_cross`)
- [x] WebAssembly target stable (`wasm32` compile-only, `e2e_cross`)
- [x] LSP stable (`e2e_lsp`)
- [x] Formatter stable (`e2e_tooling` + fmt idempoten)
- [x] Debugging tooling usable (DWARF + `e2e_debug`)
- [x] Profiling tooling available (`nusa bench`/`--stats` + `e2e_prof`)
- [x] Fuzzing infrastructure available (`fuzz_gen.py` + `e2e_verify`)
- [x] Regression tests extensive (87-test ctest + korpus 87 file)
- [x] Real-world applications tested (`realworld/` 6 proyek + `e2e_realworld`)
- [x] Performance benchmarks available (`bench/suite.sh` + `perf-3.0.md`)
- [x] Security audit completed (`security-3.0.md`, S1 diperbaiki)
- [x] Migration tooling/documentation available (`nusa migrate` + `migration-3.0.md`)
- [x] Documentation complete (website 254+ halaman, no-drift)

## 3. Diketahui (bukan blocker; RFC pasca-3.0)

- Flake beban paralel sesekali (rerun = hijau).
- 3 pemakai channel-tanpa-tag + 2 paket native (lihat `migration-3.0.md`).
- Temuan F1–F6 (`realworld-3.0.md`), S2–S3 (`security-3.0.md`).
- Evolusi bahasa wajib via RFC (master §48).

## 4. Conformance

`e2e_release.sh` (baner final + DoD artefak + tooling +
build-all + spot-run + IR `@main` + bersih) + full suite
**87/87 hijau** (1939 dtk, retry pasca-regen; run pertama
86/87 — satu-satunya gagal `e2e_ecosystem` karena site-drift
wajar pra-regen, hijau 23/23 pasca-regen) + website no-drift.
