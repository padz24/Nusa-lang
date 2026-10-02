# Nusa Language Specification 2.0

- Status: **normative** (mengikat implementasi, bukan sebaliknya).
- Describes: compiler Nusa **2.0.0** (LLVM 21.1.8, AArch64 Android).
- Menggantikan `docs/language-spec-1.0.md` (1.20.0; disimpan
  sebagai riwayat). Delta 1.0 → 2.0 ditandai **(2.0)**.
- Detail per topik dirujuk ke `docs/*.md`; bila bertentangan,
  dokumen ini menang.
- Konvensi: "MUST" = wajib, "MUST NOT" = dilarang, "undefined" =
  perilaku tak terdefinisi (lihat Appendix A).

## 1. Lexical grammar

- Encoding sumber MUST UTF-8. Byte non-ASCII di luar string/komentar
  MUST ditolak lexer.
- Whitespace: spasi, tab, CR, LF (pemisah tanpa makna).
- Komentar: `//` sampai akhir baris; `///` = dokumentasi API
  (tidak memengaruhi semantik).
- Keywords (reserved, MUST NOT dipakai sebagai identifier):
  `fn let return if else while true false i64 bool string void struct
  import type trait impl for self match enum extern`
  **(2.0)** `+ async await` (reserved untuk RFC-0004; tanpa sintaks —
  pemakaian ditolak parser).
  **(2.20.0)** `+ macro` (reserved untuk macro system; tanpa sintaks —
  pemakaian ditolak parser; lihat `docs/macro-2.20.md`).
- Identifier: `[A-Za-z_][A-Za-z0-9_]*` di luar keywords.
- Integer: `[0-9]+` desimal, nilai MUST dalam `i64`; overflow atau
  ekor-alpha (`123abc`) MUST ditolak.
- String: `"…"` dengan escapes `\" \\ \n \t \r \0`; newline mentah
  atau EOF sebelum penutup MUST ditolak.
- Operator: `+ - * / = == != < > <= >= -> => ? |` (`->` greedy;
  `!` tunggal MUST ditolak dengan saran `!=`).
- Simbol: `( ) { } [ ] : , ; .`
- Setiap token membawa `file:line:column`.
- Tests: e2e_frontend.sh, e2e_hardening.sh.

## 2. Syntax

```ebnf
Program    := (Import | TypeAlias | TraitDecl | ImplBlock |
               StructDecl | EnumDecl | FuncDecl | ExternDecl)*
Import     := "import" Ident [";"]
TypeAlias  := "type" Ident "=" Type [";"]
TraitDecl  := "trait" Ident "{" (FuncSig ";")* "}"
ImplBlock  := "impl" Ident "for" Type "{" FuncDecl* "}"
StructDecl := "struct" Ident ["<" Ident ("," Ident)* ">"]
              "{" (Ident ":" Type [";"])* "}"
EnumDecl   := "enum" Ident ["<" Ident ("," Ident)* ">"]
              "{" Variant ("," Variant)* [","] "}"
Variant    := Ident ["(" [Type ("," Type)*] ")"]
FuncDecl   := "fn" Ident ["<" Ident ("," Ident)* ">"]
              "(" [Param ("," Param)*] ")" ["->" Type] Block
ExternDecl := "extern" "fn" Ident "(" [Param ("," Param)*] ")"
              ["->" Type] ";"
Param      := Ident ":" Type
Block      := "{" Stmt* "}"
Stmt       := LetStmt | AssignStmt | PrintStmt | ReturnStmt | ExprStmt
            | IfStmt | WhileStmt | MatchStmt
Type       := "i64" | "bool" | "string" | "void" | "[" Type "]"
            | Ident ["<" Type ("," Type)* ">"]
            | "fn" "(" [Type ("," Type)*] ")" ["->" Type]
```

- `fn main()` (tanpa parameter, return `void`) MUST ada tepat satu
  per program gabungan.
- `;` opsional di akhir statement/item.
- `return` MUST terakhir di block-nya, KECUALI bare `return;`
  (fungsi void) yang legal di tengah block **(2.0: diperbaiki —
  dulu gagal verifikasi IR di `main`; kini keluar dengan 0)**;
  konstruksi `P { f: v }` dibedakan dari block via lookahead.
- Tests: e2e_types.sh, e2e_functions.sh, e2e_control.sh,
  e2e_struct_module.sh.

## 3. AST semantics

Pemetaan normatif sintaks → makna (nama node informatif):

- `LetStmt` = binding baru se-scope; `AssignStmt` = tulis slot
  terdekat yang sudah ada (MUST ada, tipe sama).
- `IfStmt`/`WhileStmt` = cabang/loop dengan kondisi `bool`.
- `MatchStmt` = desugar ke `let` scrutinee + rantai `if`/`else`
  (urutan arm = urutan sumber; guard `if` di-inline per arm).
- `Call` = panggilan langsung (arity + tipe dicek) atau tak langsung
  via value bertipe-`fn`/closure-pair.
- `Lambda` = lift ke fungsi hidden + pasangan heap
  `{trampoline, env}`; capture SELALU by-value.
- `EnumDecl` = struct hidden `{tag, slot...}` + konstruktor
  qualified `E.V(...)` / `E<T>.V(...)` (bukan first-class value).
  **(2.0)** `Option`/`Result` std = enum biasa (tidak ada status
  khusus di checker selain pesan hint).
- `ImplBlock` = rewrite `recv.m(args)` → fungsi mangled statis.
- Tests: e2e_match.sh, e2e_enums.sh, e2e_closures.sh, e2e_traits.sh.

## 4. Type system

- Statis, tanpa coercion. Nominal untuk struct/enum, struktural
  untuk sisanya.
- `let x: T = e` menuntut `typeof(e) == T`; tanpa anotasi
  menurunkan dari inisialisasi. Nilai `void` MUST NOT disimpan,
  dicetak, atau dibandingkan.
- Numerik `+ - * /` dan `< > <= >=`: operand `i64`. `==`/`!=`:
  sisi sama, non-`string`/`void`/array/struct. Unary `-`/`+`: `i64`.
- Alias `type X = T;` transparan (identik `T`); namespace bersama
  struct; siklus/rantai > 100 MUST ditolak (tepat satu error).
- Tipe fungsi `fn(T) -> T`: value
  hanya nama fn top-level (tanpa closure); berlaku di let/param/return/
  call; print/`==`/array-elemen/field-struct ditolak. Fungsi generik
  `fn f<T>(x: T) -> T` (inferensi call-site, monomorphisasi
  per kombinasi argumen, body dicek per-instance). Struct generik
  `struct P<T> { … }` (konstruksi inferensi/eksplisit,
  monomorphisasi per kombinasi, struct by-value di param/return).
  **(2.0)** targs eksplisit di posisi ekspresi body generik
  (`Option<U>.Some(…)`) disubstitusi per-instance (dulu
  `unknown type`).
- Trait `trait T { fn m(self); }` + `impl T for P { … }`:
  dispatch statis ke fungsi mangled; bounds `T: Trait` (+ ganda)
  dicek saat instansiasi. Lihat `traits.md`.
- **(2.0)** `Option<T>` = `enum Option<T> { Some(T), None }`;
  `Result<T, E>` = `enum Result<T, E> { Ok(T), Err(E) }`
  (di `std/`; B1). Operator `expr?`: operan dan return fungsi MUST
  `Result` yang sama (E sama). Konstruksi `None`/varian: tipe
  eksplisit wajib (`Option<i64>.None()`); `some(v)` inferensi.
  Lihat `error-handling.md`, `migration-2.0.md`.
- Panggilan: arity + tipe argumen dicek; return dicek terhadap tipe
  (all-paths untuk non-`void`; cabang `while` tak dihitung).
- Tests: e2e_types.sh, e2e_alias.sh, e2e_fn.sh, e2e_error.sh.

## 5. Memory model

- Owned values di stack + heap string program-lifetime; TANPA
  GC, pinjaman, atau reference counting (normatif penuh:
  `docs/memory-model.md`). **(2.0)** ditegaskan sebagai kontrak
  (RFC-0007 GC = Proposed untuk 2.1, bukan janji).
- Variabel = slot stack per scope (shadowing antar-scope; tiap Block
  scope baru). Alokasi string heap (`input`, `str_concat`,
  `file_read`) bocor by-design.
- Tidak ada referensi user-level dan tidak ada tipe pointer yang
  dapat ditulis (`ptr` bukan tipe sumber — handle runtime seperti
  argv-teks, thread/mutex/channel/atomic, dan pointer C terekspos
  sebagai `i64` opak atau `string`). Tidak ada `free` manual kecuali
  handle concurrency (`mutex_free`, `chan_free`, `atomic_free`).
- Tests: e2e_memory.sh.

## 6. Evaluation rules

- Eager, urutan kiri-ke-kanan; statement sekuensial.
- Integer: two's-complement wrapping, KECUALI pembagi nol (statis:
  compile error; dinamis: `runtime error: division by zero`, exit 1).
- Boolean untuk kontrol; TANPA short-circuit (`&&`/`||` tidak ada).
- String: pointer NUL-terminated buram; operasi di luar builtin
  tidak ada.
- Array: `[N x T]` statis; index di-bounds-check (gagal → exit 1);
  copy semantik nilai.
- Struct/enum: copy agregat; field via offset statis.
- Panggilan: konvensi C AArch64 via LLVM; rekursi didukung.
- `main` mengembalikan 0 **(2.0)** termasuk via bare `return;`
  di tengah body. Abort runtime (assert/OOB/div-0/channel-mismatch)
  → exit 1.
- Tests: e2e_variables.sh, e2e_arrays.sh, e2e_control.sh.

## 7. Modules and packages

- Satu file = satu modul. `import m;` memuat `<dir>/m.ns`
  (transitif). Merge nama PUBLIK; konflik = error duplikat.
- Nama `_`-awalan = privat modul. Siklus MUST ditolak
  (`circular import detected`); rantai > 100 MUST ditolak.
- Paket (`nusa.toml`): path-deps transitif, lock + SHA-256, build
  reproduksibel; registry file-backed (publish/search/install,
  resolver persis/caret, cache terverifikasi); template
  (`nusa new`: hello-world/cli-app/library/server).
- Tests: e2e_struct_module.sh, e2e_pkg.sh, e2e_registry.sh,
  e2e_new.sh.

## 8. Generics

- Fungsi `fn f<T>(x: T) -> T` dan struct `struct P<T> { … }`.
- Inferensi tipe di call-site (tanpa turbofish); monomorphisasi per
  kombinasi argumen; template dibuang sebelum codegen.
- Rekursi tipe-sama berhenti (cache instance); cap kedalaman 64.
- Struct: konstruksi inferensi atau eksplisit `Point<i64> { … }`;
  by-value di param/return.
- DILARANG: metode/impl generik, argumen tipe eksplisit di call,
  fungsi generik sebagai value.
- Tests: e2e_generics.sh, e2e_structs_gen.sh.

## 9. Traits

- `trait T { fn m(self, …); }` (tanpa body; `self` telanjang
  pertama) + `impl T for P { … }` (target: struct termasuk
  generik-konkret, primitif; array/fn/void DILARANG).
- Dispatch SELALU statis (rewrite ke `Trait_method_Type`); bounds
  `T: Trait` (+ ganda `+`) dicek saat instansiasi konkret.
- Koherensi: satu impl per (trait, tipe) program-wide.
- DILARANG: supertrait, body default, `Self`, associated function,
  dynamic dispatch.
- Tests: e2e_traits.sh.

## 10. Pattern matching

- Statement-only: `match e { pola [if g] => { … }, }`.
- Pola: literal int/bool/string (negatif OK), `_`, bind, struct,
  varian enum (`Some(v)`/`None`/`Ok(v)`/`Err(e)`/varian pengguna).
- Exhaustiveness WAJIB: `bool` dan pasangan varian tertutup
  **(2.0)** termasuk `Option`/`Result` natif; himpunan literal
  butuh `_`. Pola unreachable/invalid MUST ditolak.
- **(2.0)** pola varian atas NON-enum DITOLAK (`cannot match
  variant`; encoding struct v1.x tidak ada lagi).
- DILARANG: or-pattern, range-pattern, exhaustiveness int/string
  umum tanpa `_`.
- Tests: e2e_match.sh, e2e_enums.sh.

## 11. Concurrency

- Primitif (builtin): `thread_spawn` (`fn() -> i64` ketat)/
  `thread_join`, `mutex_new/lock/unlock/try_lock/free`,
  `chan_new/send/recv/free` (i64) **(2.0)** `+ chan_new_bool/
  send_bool/recv_bool`, `chan_new_str/send_str/recv_str`
  (tag diverifikasi runtime; mismatch → exit 1),
  `atomic_new/get/set/add/free` (seq-cst), `sleep_ms`.
- Runtime pthread; `-pthread` selalu di-link.
- TIDAK ADA deteksi deadlock/data-race statis; penjadwalan dan
  urutan interleave = undefined (program MUST sinkronisasi
  eksplisit). Tanpa select/close/scoped-threads/`async`.
- Tests: e2e_concurrency.sh.

## 12. Foreign function interface

- `extern fn name(params) -> T;` (tanpa body): deklarasi fungsi C,
  ABI C. Param: `i64`/`string`/`bool`; return: `i64`/`bool`/`void`.
  `bool` = `i32` di ABI. Tanpa pointer umum/struct-by-value/
  callback/variadik/string-return (RFC-0008 = Proposed).
- Link: `nusa build --link/--link-dir` + manifest `[link]`.
  Pustaka asing DIPERCAYA PENUH (keamanan = pin sumber).
- Tests: e2e_ffi.sh.

## 13. Standard library

- Builtin global (tanpa import): `print`, `input`, `assert`,
  `str_len`, `str_concat`, `str_char_at`, `str_slice`, `int_to_str`,
  `parse_int`, `parse_int_valid`, `file_read/write/exists`,
  `array_sum/min/max`, `len`, `time_now_ms/s`, `random_u64`,
  `process_exit/env/cwd/argc/argv`, `tcp_connect/send/recv/close`
  **(2.0)** `+ tcp_listen/accept` (server loopback-only),
  primitif concurrency (§11: 16 lama + 6 bertag).
- Modul `std/` (di-`import`; kanonis `import std.x`):
  `io math string collections fs system error option result time
  random process path thread sync net` (net **(2.0)** `+
  listen_on/accept_one`).
- Android-spesifik terisolasi di layer runtime C
  (`runtime/nusa_rt.c`); API murni-Nusa diutamakan.
- Tests: e2e_stdlib.sh, e2e_stdlib2.sh.

## 14. Diagnostics conformance

- Setiap error: `file:line:col: error:` + pesan + snippet/caret +
  opsional `help:`. Seluruhnya bahasa Inggris.
- Recovery top-level + cap 25 error per tahap; compiler MUST exit
  != 0 tanpa sinyal pada input apapun yang ditolak; hang DILARANG.
- Tests: e2e_diagnostics.sh, e2e_hardening.sh.

## 15. Document split and conformance

Pemisahan normatif (Phase 40, dipertahankan):

1. **Language Specification** = dokumen ini
   (`docs/language-spec-2.0.md`): satu-satunya sumber kebenaran
   bahasa (v1.0 disimpan sebagai riwayat).
2. **Compiler Implementation** = `docs/compiler.md`.
3. **Standard Library Specification** = `docs/standard-library.md`
   (+ §13 di atas untuk daftar normatif).

Kesesuaian dikunci `compiler/tests/e2e_spec2.sh`: 13 topik §1–§13
WAJIB ada; setiap topik menunjuk file uji pengunci (`Tests:`) yang
WAJIB ada di tree; contoh normatif spot-check dieksekusi
(termasuk BREAKING B1 dua arah: natif diterima, bentuk lama
ditolak). Panduan migrasi normatif-pendamping:
`docs/migration-2.0.md`.

## Appendix A. Undefined behavior (non-ekshaustif)

- Mengandalkan apa pun di luar §1–§14 (mis. GC, `&&`, float,
  `break`, `+=`, metode generik, supertrait, `async`).
- Isi NUL (`\0`) di dalam nilai string; literal ≥ 2^63.
- Urutan interleave thread; nilai PRNG; waktu eksekusi.
- Output `nusa lex` untuk input invalid (bentuk baris stabil,
  kanal/status tidak).
- Artefak `.ll`/`.s` (representasi antara, bukan kontrak bahasa).
- Kapan memori bocor-by-design direklamasi (tidak pernah —
  kontrak §5).
