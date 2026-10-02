# Cross Compilation — Targets (Phase 33, v1.14.0)

Nusa mengemisikan LLVM IR tekstual lalu memanggil `clang` sistem.
Cross-compilation = clang `--target` + sysroot yang cocok.

```sh
nusa build app.ns --target linux-x86_64 -c -o app.o        # objek ELF x86-64
nusa build app.ns --target linux-aarch64 --emit-ir -o app.ll
nusa build app.ns --target linux-x86_64 --sysroot /path/to/sysroot
```

## Matriks target

| Alias | Triple | OS/Ars | ABI/Objek | Linker | Status |
|---|---|---|---|---|---|
| `android-aarch64` | host clang ini | Android/AArch64 | Bionic/ELF | clang | **penuh (utama)** |
| `linux-aarch64` | `aarch64-linux-gnu` | Linux/AArch64 | glibc/ELF | clang + sysroot | compile-only tanpa sysroot |
| `linux-x86_64` | `x86_64-linux-gnu` | Linux/x86-64 | glibc/ELF | clang + sysroot | compile-only tanpa sysroot |
| `wasm32` | `wasm32-unknown-unknown` | WebAssembly/32 | ILP32/wasm | clang + sysroot | compile-only tanpa sysroot (v2.18.0) |

- Alias atau triple persis diterima (`--target x86_64-linux-gnu`);
  sisanya ditolak dengan daftar. Triple host (`android-aarch64`)
  = perilaku lama bit-identik (tanpa triple di IR).
- Triple eksplisit ditulis ke IR (`target triple = "..."`);
  DataLayout disimpulkan clang dari triple (tidak di-pin).

## Mode emisi

- (default) link executable — cross WAJIB `--sysroot` (tanpanya
  ditolak dini dengan arahan, bukan dump `ld`).
- `--emit-ir` (`-o x.ll`), `--emit-asm` (`-o x.s`), `-c`/
  `--emit-obj` (`-o x.o`): compile-only, tanpa linker/sysroot/
  runtime C (simbol `nusa_*` unresolved di `.o` — wajar, tersambung
  saat link final).
- `nusa run --target <host>` OK; target non-host ditolak
  (binary tak bisa dieksekusi di sini).
- Manifest: `[package] target = "linux-x86_64"` dipakai `pkg build`
  (CLI `--target` menang; `--sysroot` diteruskan).

## Asumsi portabilitas (mengapa hanya LP64 ELF)

Kode yang di-generate mengasumsikan LP64 (`i64` ≡ pointer 64-bit:
pair closure, handle opak, cast trampolin→pthread). Berlaku untuk
ketiga target prioritas (Android/Linux AArch64/x86-64).

## wasm32: subset portabel (v2.18.0, Phase 67)

`wasm32` = bare-metal ILP32 (`wasm32-unknown-unknown`):
compile-only (`--emit-ir`/`--emit-asm`/`-c`), link butuh sysroot,
`run` ditolak. Asumsi LP64 TAK berlaku: pointer round-trip
`i64`↔`i32` secara nilai, tapi thread/mutex/channel/TCP/net
(concurrency + `nusa_rt.c`) tak tersedia tanpa wasi-sysroot —
jangan bawa builtin itu ke wasm. Terjamin: komputasi murni
(arith/control/string/array/struct inline). Detail + follow-up
(WASI, entry `_start`, runtime port): `docs/wasm-2.18.md`.

## Dievaluasi, belum didukung

- **Windows** (x86-64): COFF + LLP64 (`long` 32-bit mematahkan
  asumsi LP64) + linker MSVC/LLD-COFF + runtime C MSVCRT
  (getaddrinfo/`nanosleep` berbeda). Butuh audit ABI penuh.
- **macOS** (AArch64): Mach-O + tidak ada toolchain di sini untuk
  verifikasi + perbedaan libc. Butuh mesin dan uji.
- Bare-metal lain/wasm64: di luar roadmap saat ini.
