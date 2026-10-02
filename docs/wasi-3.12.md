# WASI / Sandboxed Execution — Milestone 1: Model Kapabilitas (Phase 111)

Status: slice model + gate (target 3.12.0). Mandat master: WASI,
program dalam sandbox, model keamanan eksplisit; pakai: plugin,
server-workload, perkakas, kode tak dipercaya.

## 1. Fakta lingkungan (CI ini, terverifikasi)

- `wasm32` = alias compile-only → `wasm32-unknown-unknown`
  (sejak 2.18.0): `--emit-ir` (triple), `-c` (objek WebAssembly
  via `file`), link/run ditolak. TAK BERUBAH slice ini.
- Toolchain: `wasm-ld` ADA, `node` ADA (WebAssembly), wasmtime/
  wasmer TIDAK ADA, WASI sysroot (libc/crt) TIDAK ADA →
  link+eksekusi wasm penuh BELUM dimungkinkan di sini
  (follow-up + kriteria terima di §4).

## 2. Gate kapabilitas (yang dibangun)

- Saat LINK `--target wasm32*`, IR dipindai untuk 10 subsistem
  (`threads/mutexes/channels/atomics/sleep/time/random/
  processes/networking/crypto` — cermin simbol `@nusa_*`
  `needRt`): positif → tolak eksplisit dengan NAMA
  kapabilitas + arahan (`docs/wasi-3.12.md`), DENGAN atau TANPA
  `--sysroot` (sysroot tak menyediakan threads/jaringan).
- Komputasi murni (+ libc host) → jalur lama tak tersentuh
  (pesan `needs --sysroot`, kompat `e2e_cross` byte-stabil).
- Inspeksi (`--emit-ir/-asm/-c`) TAK PERNAH ditolak.
- Implementasi: tabel `kRtCaps` + `usedRtCaps` +
  gate pasca-emitIR (`compiler/driver/Driver.cpp`).

## 3. Model keamanan eksplisit (mandat)

| Lapisan | Status | Menjamin |
|---|---|---|
| Penolakan kapabilitas (tooling) | ADA (slice ini) | program terblokir gagal SEBELUM artefak; pesan menyebut kapabilitas |
| WASI libc/capability-fd | BELUM (tanpa sysroot) | — |
| Runtime wasm terisolasi | BELUM (tanpa engine) | — |
| Batas sumber daya (CPU/mem) | WAJIB di host (wasm tak membatasi sendiri) | didokumentasikan, bukan dienforce |
| `nusa sandbox` proses | ADA (pra-3.0) | defense-in-depth komplementer hari ini |

Ancaman yang diakui: klaim sandbox selalu bergantung host
pelaksana; tanpa engine, Nusa tak mengklaim isolasi wasm apa pun.
File I/O (libc fopen) = kemampuan BELUM dipetakan (follow-up:
WASI-fs atau tolak-seperti-runtime).

## 4. Ditunda jujur + kriteria terima

- Link + run wasm nyata: BUTUH wasi-sysroot + engine; terima =
  `e2e_wasi` kasus link-run hijau (program komputasi murni
  dieksekusi via engine dengan impor WASI).
- Plugin/server-sandbox: butuh engine + capability-fd + batas
  sumber daya host (Phase 112 memakai hasil ini).
- File/host-libc mapping: dipetakan atau digate seperti runtime.
