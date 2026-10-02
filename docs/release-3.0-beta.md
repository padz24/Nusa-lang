# Nusa 3.0.0-beta — Release Record (Phase 84)

Status: beta. Freeze (mengikat sampai final): semantik bahasa
(`docs/language-spec-3.0.md` normatif), arsitektur compiler
(`docs/compiler.md`), API standard library
(`docs/standard-library.md` v3.0.0-beta), format paket/manifest
(`docs/pkg.md`, `compiler =` berversi penuh termasuk suffix
pra-rilis, semver sadar-pra-rilis).

## 1. Penomoran (keputusan tercatat)

- `project(... VERSION 3.0.0)` tetap numerik (kebutuhan
  `packaging/deb.sh`: paket `3.0.0-1`).
- Label manusia `NUSA_VERSION_STR = "3.0.0-beta"` (sumber
  tunggal di CMakeLists; mengalir ke `--version`, help,
  `compiler =` scaffold, LSP). Final menghapus suffix.
- Template `nusa new` mencatat versi penuh; lock 2.x diminta
  `pkg update`. Uji `e2e_new` diperketat (regex sadar-suffix);
  `e2e_deb` sengaja tetap inti-numerik.

## 2. Baterai beta (mandat: unit/integration/regression/fuzz/benchmark/real-world)

| Jenis | Pelaksana | Hasil |
|---|---|---|
| unit | `ctest` (test_*) | hijau (bagian full suite) |
| integration | full `ctest -j2` (83 uji) | 83/83 hijau (1056 dtk) |
| regression | `e2e_compat` + `e2e_migrate` + suite | hijau |
| fuzz | `e2e_verify` seed bawaan + `NUSA_VERIFY_FUZZ_SEED=424242` (52/52 keduanya) + seed beta `30084` (40 prog: 0 mismatch/0 crash/0 hang) | hijau |
| benchmark | `bench.sh` closureapply 1.91 s / exe 6776 B / run 0.15 s; biner 1623920 B; `parse.ns` 3.4 s wall | ≈ baseline 2.30.1 (varian wajar) |
| real-world | `e2e_beta` build-all 46 program + spot-run | 4/4 hijau |

## 3. Diketahui (bukan blocker beta)

- Flake beban paralel sesekali (`e2e_playground` Phase 83:
  gagal in-suite, hijau isolasi+rerun) — pola tercatat sejak
  Phase 79; rerun = hijau.
- 3 pemakai channel-tanpa-tag (2 di stdlib) + 2 paket native
  (tls/sqlite) — lihat `docs/migration-3.0.md`; tanpa aksi
  bahasa.
- `nusa migrate` heuristik marker didokumentasikan (§5 guide).

## 4. Conformance

`e2e_beta.sh` (baner + build-all + spot-run + bersih-artefak)
+ seluruh suite hijau + website no-drift.
