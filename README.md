# Nusa — Bahasa Pemrograman (v2.0.0 Stabil)

Nusa adalah bahasa pemrograman compiled (`.ns` → native executable)
dengan backend **LLVM 21.1.8**, dikembangkan di **Android AArch64 / Termux**.

Dibuat oleh **Padil** — Banyumas, Jawa Tengah.

> Nusa 2.0.0 stabil: enum natif (`Option`/`Result`), operator `?`,
> generics, traits, pattern matching, closures, concurrency
> (thread/channel/atomic), package manager + registry file-backed,
> C FFI, cross-compilation, LSP, DWARF debug info, bootstrap
> terverifikasi, spesifikasi bahasa normatif, dan panduan migrasi
> 1.x → 2.0 — semuanya teruji (50/50 ctest) dan terdokumentasi
> di bawah ini.

## Status

- Nusa 2.0.0: lihat `CHANGELOG.md` + `docs/language-spec-2.0.md` +
  `docs/migration-2.0.md`.
- Fondasi v1.0.0 (tetap berlaku, kini riwayat):
- [x] Compiler: lexer, parser, AST, semantic, typechecker, backend
      LLVM 21.1.8 → native AArch64 (semua terverifikasi)
- [x] Bahasa: semua fitur master bekerja dan terkunci regression test
- [x] Tooling: 8 perintah CLI + formatter idempoten + docgen + runner
- [x] Stdlib minimal-usable (`std/` + 11 builtin) + `docs/standard-library.md`
- [x] Diagnostics: file:line:col + snippet + caret span + `help:`
      (format = contoh master)
- [x] Dokumentasi: 10 file `docs/` wajib + spesifikasi (`docs/spec.md`)
- [x] Program uji 1.0 verbatim (`examples/nusa10.ns`) → `30/Nusa`
- [x] Definition of Done 12/12 (lihat CHANGELOG)

Lihat `CHANGELOG.md` untuk riwayat per versi (v0.1.0 → v2.0.0).

## Pembuat

Bahasa pemrograman Nusa dibuat oleh **Padil**, Banyumas, Jawa Tengah.

## Panduan belajar

Jalur resmi dari nol sampai proyek paket (tiap modul berisi
`README.md` + `main.ns` yang bisa langsung dijalankan):

| Urutan | Modul | Topik |
|---|---|---|
| 01 | `tutorials/01-hello/` | program pertama, `build`/`run`/`check` |
| 02 | `tutorials/02-variables/` | `let`, `i64`/`bool`/`string`, array |
| 03 | `tutorials/03-functions/` | fungsi, struct, gambaran modul |
| 04 | `tutorials/04-generics-traits/` | generic, trait/`impl` |
| 05 | `tutorials/05-match-enum/` | `match`, enum |
| 06 | `tutorials/06-error-handling/` | `Option`/`Result`, operator `?` |
| 07 | `tutorials/07-closures-concurrency/` | lambda, thread + atomic |
| 08 | `tutorials/08-package-project/` | `nusa pkg`, `nusa new`, registry |

```bash
./build/nusa run tutorials/01-hello/main.ns
# Hello, Nusa!
```

Rujukan pendamping:

- `docs/getting-started.md` — instalasi sampai program pertama
- `docs/language-spec-2.0.md` — spesifikasi bahasa (normatif)
- `docs/migration-2.0.md` — migrasi 1.x → 2.0
- `website/` — situs statis offline
  (`python3 -m http.server`, buka `/website/`)
- `dataset/` — dataset + notebook Colab untuk melatih asisten
  "Nusa Helper" dari nol (1006 baris dari 55 dokumen)

## Prasyarat (Termux AArch64)

```bash
pkg install clang cmake llvm
llvm-config --version   # harus 21.1.8
clang --version         # harus 21.1.8
cmake --version
uname -m                # harus aarch64
```

## Build

```bash
cd ~/nusa
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## Install global (.deb Termux)

```bash
bash packaging/deb.sh   # hasil: website/downloads/nusa_2.0.0-1_aarch64.deb
apt install ./website/downloads/nusa_2.0.0-1_aarch64.deb
nusa --version
```

Unduhan + pilihan versi stabil: halaman `website/downloads.html`
(serve lokal via `python3 -m http.server`).

## Pakai

```bash
./build/nusa --version
# nusa version 2.0.0 (LLVM 21.1.8, AArch64 Android/Termux)
./build/nusa build examples/hello.ns -o hello
./hello
# Hello, Nusa!

./build/nusa run examples/hello.ns

# Variables & expressions (v0.3.0):
./build/nusa run examples/arithmetic.ns
# 30
# 70
# true

# Static types (v0.4.0):
./build/nusa run examples/types.ns
# 15
# true
# Nusa
# 30

# Functions (v0.5.0):
./build/nusa run examples/functions.ns
# 30

# Control flow (v0.6.0):
./build/nusa run examples/control.ns
# 10
# 3
# 2
# 1

# String & array (v0.7.0):
./build/nusa run examples/arrays.ns
# Hello
# 1
# 4
# 30

# Struct & module (v0.8.0):
./build/nusa run examples/person.ns
# Nusa
# 1
cd examples && ../build/nusa run main.ns
# 6
# Ada
cd ..

# Tooling (v0.9.5):
./build/nusa check examples/hello.ns
# Checked examples/hello.ns: no errors
./build/nusa test tests/
# PASS tests/math_test.ns
# ...
./build/nusa fmt --check examples/hello.ns
./build/nusa doc std/math.ns | head -n 8
```

LLVM IR yang dihasilkan ada di `<file>.ns.ll` di samping source.

Lihat `docs/syntax.md` untuk grammar yang didukung saat ini.

## Struktur

```
nusa/
├── CMakeLists.txt
├── examples/hello.ns
├── examples/expr.ns
├── examples/arithmetic.ns
├── examples/types.ns
├── examples/functions.ns
├── examples/control.ns
├── examples/arrays.ns
├── examples/person.ns
├── examples/math.ns
├── examples/main.ns
├── examples/nusa10.ns
├── examples/generics.ns
├── examples/traits.ns
├── examples/match.ns
├── examples/enums.ns
├── examples/closures.ns
├── examples/error_option_result.ns
├── examples/concurrency.ns
├── tutorials/01-hello … 08-package-project
│   └── tiap modul: README.md + main.ns runnable
├── website/index.html (+ ecosystem/tutorials/examples.html)
├── std/io.ns
├── std/math.ns
├── std/string.ns
├── std/collections.ns
├── std/fs.ns
├── std/system.ns
├── tests/math_test.ns
├── tests/strings_test.ns
├── tests/files_test.ns
├── compiler/
│   ├── lexer/        Lexer + Token + komentar (`//`/`///`)
│   ├── parser/       Struct/import/literal/field + lookahead
│   ├── ast/          AST nodes + s-expr dump
│   ├── types/        Type i64/bool/string/void/func/array/struct
│   ├── semantic/     Namespaces + Scopes + structs + builtins
│   ├── diagnostics/  Error reporting file:line:col + snippet
│   ├── codegen/      LLVM functions/blocks/arrays/structs/libc
│   ├── driver/       CLI + ModuleLoader + test-runner + check-stages
│   ├── fmt/          Formatter token-based (v0.9.0)
│   ├── doc/          Generator API Markdown (v0.9.0)
│   └── tests/        unit + integration tests
├── docs/
│   ├── getting-started.md
│   ├── syntax.md
│   ├── types.md
│   ├── functions.md
│   ├── control-flow.md
│   ├── arrays.md
│   ├── structs.md
│   ├── modules.md
│   ├── standard-library.md
│   ├── spec.md
│   └── compiler.md
└── CHANGELOG.md
```

## Batasan v1.0.0 (final, sesuai master)

- Nesting > 500 ditolak (`too deeply nested`); rantai import > 100
  ditolak; keduanya tanpa crash.
- Struct: tanpa field array, param/return struct, `==`/print struct,
  indexed assignment (konsisten v0.7.0); tanpa methods/traits.
- Modul: flat (tanpa subdir/`::`/package manager); merge dengan
  konflik = error; field selalu publik.
- Stdlib minimal: string buram (tanpa `==`/substring), file tanpa error
  rinci (`""`/`false`), heap string bocor (GC = roadmap 1.x/2.x).
- `fmt` sederhana (aturan token, §compiler.md); `test` tanpa paralel.
- Fungsi top-level saja; `return` all-paths; tanpa `break`/`continue`.
- Tanpa generics (sesuai master).
- Lihat `CHANGELOG.md` untuk detail.
