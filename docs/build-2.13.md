# Build System 2.0 — Design (Phase 62, target 2.13.0)

Status: audit → Implementation (milestone 1: `clean` + `--profile`).
Spec tak berubah; tooling.

## 1. Audit CLI vs mandat (2.12.0)

Ada: build/test/run/check/install/publish, `-O0/1/2/3/s/z`,
`-g`, `--fresh`, `--target/--sysroot`, `--emit-ir/asm/-c`,
incremental (fingerprint+cache), build caching (precompiled
runtime). Belum: `clean`, profiles, parallel `test`
(sekuensial fork+alarm; output deterministik — dipertahankan
sengaja, paralelisme di ctest), custom `[profile.*]` manifest.

## 2. Milestone 1

**`nusa clean [dir=.]`** (default cwd): hapus artefak
tergenerasi — `*.ns.ll` (rekursif), `.nusa-cache/` (rekursif),
`target/`, `dist/` (top-level paket). TIDAK menyentuh sumber
(`*.ns`), manifest, lock, atau biner luar `target/`. Cetak
ringkasan jumlah. Idempoten (bersih ganda = 0 dihapus, rc 0).

**`--profile <dev|debug|release>`** (untuk `build` + `pkg build`):

- `dev` (default implisit): `-O0`, tanpa `-g` (perilaku hari ini).
- `debug`: `-O0` + `-g` (DWARF).
- `release`: `-O2`, tanpa `-g`.
- Flag eksplisit (`-O*`/`-g`) menang atas profil (urutan bebas).
- Profil tak dikenal → error + daftar valid.
- Profil custom `[profile.NAME]` = follow-up (butuh skema
  manifest + merge + validasi; tercatat).

## 3. DITUNDA beralasan

`test` paralel (determinisme output > kecepatan; `ctest -j`
mencakup), custom profiles, `run --profile` (runRun tanpa
penerus flag — butuh refactor pemisah argumen; follow-up),
remote/distributed cache.

## 4. Acceptance criteria

- `e2e_buildsys.sh`: clean menghapus + idempoten + tak sentuh
  sumber; tiap profil build+run benar (`-O2` biner jalan,
  `-g` ada DWARF); eksplisit menang; profil asing ditolak;
  `pkg build --profile release` jalan.
- Seluruh suite hijau; bench identik.
