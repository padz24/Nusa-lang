# Nusa 3.0.0-rc.1 — Release Record (Phase 88)

Status: release candidate. Freeze (mengikat sampai final):
semantik bahasa (`docs/language-spec-3.0.md` normatif),
arsitektur compiler (`docs/compiler.md`), API standard library
(`docs/standard-library.md` v3.0.0-rc.1), format paket/manifest
(`docs/pkg.md`, `compiler =` berversi penuh termasuk suffix
pra-rilis, semver sadar-pra-rilis).

NOL perubahan bahasa/arsitektur/API/format sejak beta:
satu-satunya delta beta → rc.1 adalah label versi
(`3.0.0-beta` → `3.0.0-rc.1`, help `Beta` → `RC`),
gate RC (`compiler/tests/e2e_rc.sh`), record ini,
dan regenerasi website. `e2e_beta.sh` dipertahankan sebagai
regresi (banner `beta|rc.*` sejak Phase 88).

## 1. Penomoran (keputusan tercatat)

- `project(... VERSION 3.0.0)` tetap numerik (kebutuhan
  `packaging/deb.sh`: paket `3.0.0-1`).
- Label manusia `NUSA_VERSION_STR = "3.0.0-rc.1"` (sumber
  tunggal di CMakeLists; mengalir ke `--version`, help,
  `compiler =` scaffold, LSP). Final menghapus suffix.
- Template `nusa new` mencatat versi penuh; `e2e_new`
  (regex sadar-suffix sejak Phase 84) lolos tanpa perubahan;
  `e2e_repro` (pin dinamis) lolos tanpa perubahan.
- `e2e_beta` (Phase 84) dilonggarkan ke `beta|rc.*` agar
  regresi build-all tetap hijau; gate RC ketat
  (`3.0.0-rc.1` eksak + tanpa kata `Beta` di help) ada di
  `e2e_rc.sh`.

## 2. Baterai RC (mandat Phase 88: tanpa perubahan besar semantik)

| Jenis | Pelaksana | Hasil |
|---|---|---|
| unit | `ctest` (test_*) | 13/13 hijau (bagian full suite) |
| integration | full `ctest -j2` | **86/86 hijau** (1165 dtk, 0 gagal) |
| regression | `e2e_beta` + `e2e_migrate` + `e2e_compat` + suite | hijau (in-suite) |
| freeze | `e2e_rc` (rc-freeze) + `e2e_spec3` + `e2e_freeze` | hijau (in-suite) |
| security | `e2e_sec` 9/9 (warisan Phase 87, tanpa perubahan) | hijau (in-suite, 12 dtk) |
| fuzz | `e2e_verify` seed bawaan + `424242` + `30084` | hijau (in-suite) |
| benchmark | `bench/suite.sh` via `e2e_perf` (warisan Phase 86; tanpa klaim baru) | hijau (in-suite, 37 dtk) |
| real-world | `e2e_rc` build-all (52+35) + `e2e_realworld` | hijau (in-suite) |

## 3. Diketahui (bukan blocker RC1)

- Flake beban paralel sesekali (pola tercatat sejak Phase 79;
  rerun = hijau).
- 3 pemakai channel-tanpa-tag + 2 paket native (tls/sqlite) —
  lihat `docs/migration-3.0.md`; tanpa aksi bahasa.
- Temuan desain F1–F6 (`docs/realworld-3.0.md`) dan S2–S3 info
  (`docs/security-3.0.md`) dijadwalkan pasca-3.0 via RFC,
  bukan blocker.
- rc.2/rc.3 hanya bila diperlukan (master Phase 88).

## 4. Conformance

`e2e_rc.sh` (baner RC eksak + freeze + build-all + spot-run +
IR `@main` + bersih-artefak) + full suite **86/86 hijau**
(1165 dtk) + website no-drift (`python3 scripts/gen.py --check`).
