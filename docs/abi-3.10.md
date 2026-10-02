# Nusa ABI 3.10 — Spesifikasi (Phase 109)

Status: normatif atas codegen 3.0.0 (LLVM 21 backend). "ABI" di sini =
kontrak biner antar unit kompilasi Nusa + batas C/FFI + platform.
Diturunkan dari `compiler/codegen/Codegen.cpp` + bukti IR
(`e2e_abi.sh` mengunci pola; contoh IR di bawah dari AArch64).

## 1. Konvensi pemanggilan (semua target)

- Default LLVM C calling convention (`defaultcc`; tanpa anotasi
  CC eksplisit di IR). Test mengunci `define` tanpa `cc` token.
- Entry: `define i32 @main(i32, ptr)` (argc/argv C; Nusa `main`
  void; argv disalin ke global internal `nusa_argc/nusa_argv`).

## 2. Representasi primitif

| Nusa | LLVM IR | Catatan |
|---|---|---|
| `i64` | `i64` | wrap (kecuali div) |
| `bool` | `i1` | internal (`define i1 @isyes(i1)`) |
| `string` | `ptr` | C-string heap (malloc), NUL-terminated |
| `void` | `void` | |
| `fn(T)->U` (value) | `ptr` | pair closure, BUKAN function pointer mentah |
| `[T]` | `[N x T]` alloca lokal | bukan bagian ABI lintas-fungsi (param/return ditolak) |

## 3. Struct: `%Name = type { ... }` non-packed, urutan deklarasi

```llvm
%Point = type { i64, ptr, i1 }
define %Point @mk(i64 %v)
```

Layout = LLVM non-packed default (alignment alami target;
TIDAK `packed`). Nested struct inline sebagai tipe bernama
(duplikasi nama = error). By-value di param/return (mis.
`define %Point @mk(i64)`, `define i64 @get(%E)`).

## 4. Enum: hidden struct `{tag: i64, slot...}`

```llvm
%E = type { i64, i64 }   /* tag + satu slot payload i64 */
```

Field `tag: i64` pertama; slot `v{tag}_{pos}` sesudahnya
(satu slot per posisi payload; varian unit tanpa slot).
Dispatch via `tag` + GEP (bukti: `q.tag` + `CreateStructGEP`).
Konstruktor `E.Some(7)` = literal struct (tak ada simbol).

## 5. Closure: `%__nusa_closure = type { ptr, ptr }`

`{trampoline, env}` heap (`malloc`); param bertipe-`fn` = `ptr`
ke pair; panggil = muat trampoline + `call R %tramp(ptr %env,
args...)`. Trampolin per-lambda `@"$lambda_N.tramp"`
(InternalLinkage). Capture by-value ke struct env.

## 6. Name mangling (simbol eksternal, default visibility)

| Konstruksi | Bentuk | Bukti |
|---|---|---|
| fn biasa | nama apa adanya | `define i64 @add` |
| fn generik | `name_T` (tipe argumen) | `define i64 @id_i64` |
| struct generik | `Name_T` | `Point_i64` (lih. e2e_structs_gen) |
| metode trait | `Trait_method_Type` | `define ptr @Show_show_A` |
| lambda | `$lambda_N` + `.tramp` | `@"$lambda_0.tramp"` internal |
| string literal | `.str(.N)` | PrivateLinkage |
| runtime/program | `nusa_argc`, `nusa_argv` | InternalLinkage |

Fungsi Nusa = ExternalLinkage, visibilitas default (tak ada
`hidden`/`dll*`/prefix `nusa.`). Tabrakan nama mangled vs
deklarasi user = error compile (checker).

## 7. Batas C/FFI vs Nusa vs platform

- Nusa ABI: §1–§6 (internal; berubah = breaking + RFC + uji ini).
- C ABI: `extern fn` dideklarasikan saja (`declare`, tanpa body);
  `bool` Nusa (`i1`) ↔ C `int` (`i32`, zext arg + icmp return);
  `string` ↔ `char*`; variadik/struct-return-callback tak didukung
  (ditolak checker — bukan UB diam). Bukti: `declare i64
  @myputs(ptr)`.
- Platform ABI: mengikuti target clang (AArch64 Android/Bionic
  primer; `docs/targets.md`); asumsi LP64 (tercatat; WASM
  ILP32 = komputasi murni — lih. `docs/wasm-2.18.md`).
- Bukan ABI stabil: `.ll` samping-sumber, fingerprint
  `.nusa-cache/`, urutan GEP internal, nama value `%`.

## 8. Kunci mesin

`compiler/tests/e2e_abi.sh`: 6 kasus (struct/enum/main/
mangle/closure/extern) atas `--emit-ir` dua probe mandiri.
Perubahan bentuk = FAIL eksplisit (disengaja — drift ABI harus
melalui RFC + versi mayor).
