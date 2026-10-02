# WASM Target — Design (Phase 67, target 2.18.0)

Status: audit → Implementation (milestone 1: alias wasm32
compile-only). Spec tak berubah.

## 1. Keputusan triple

`wasm32` → `wasm32-unknown-unknown` (bare metal; didukung clang
21 LLVM ini). BUKAN `wasm32-wasi` (butuh wasi-sysroot yang tak
ada di sini; kandidat follow-up bila sysroot tersedia).

## 2. Cakupan milestone 1 (cermin target Linux)

- `--emit-ir`/`--emit-asm`/`-c`: compile-only, tanpa sysroot.
- Link (`-o` exe) tanpa sysroot → ditolak guard existing
  ("needs --sysroot"). `run --target wasm32` → ditolak (non-host).
- Alias + triple persis diterima; pesan error diperbarui.

## 3. Batas jujur (platform abstraction)

- Asumsi LP64 codegen (`i64` ≡ pointer) TIDAK berlaku di wasm32
  (ILP32): pointer round-trip i64→i32 secara nilai, TAPI kelas
  permukaan handle/closure-pair/thread/net = TAK DIDUKUNG
  (runtime memakai pthread/socket; tak ada wasi-sysroot).
- Portable garantis: komputasi murni (arith/control/string/
  array/struct inline). Sandbox WebAssembly = sifat inheren
  format (linear memory), bukan klaim implementasi.
- WASI sysroot + runtime port + entry `_start` = follow-up.

## 4. Acceptance criteria

- IR triple `wasm32-unknown-unknown`; objek `file` =
  WebAssembly; link/run ditolak jelas; e2e_cross kasus baru.
