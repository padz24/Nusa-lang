# Spesifikasi Bahasa Nusa — v1.0.0 (dasar; riwayat)

> Dokumen historis (beku sejak v1.0.0, diperkaya catatan versi
> hingga v1.13.0). Definisi bahasa yang berlaku saat ini:
> **`docs/language-spec-3.0.md`** (normatif final; bila bertentangan,
> dokumen itu yang menang). Versi 2.0 diabadikan di
> `docs/language-spec-2.0.md`, 1.20.0 di
> `docs/language-spec-1.0.md`.

## 1. Leksikal

- Encoding sumber: UTF-8 (konten non-ASCII selain di string/kommen
  ditolak lexer per karakter).
- Whitespace: spasi, tab, CR, LF (tanpa makna selain pemisah).
- Komentar: `//` sampai akhir baris; `///` = dokumentasi API.
- Keyword (reserved): `fn let return if else while true false i64 bool
  string void struct import`.
- Identifier: `[A-Za-z_][A-Za-z0-9_]*` di luar keyword.
- Integer: `[0-9]+` desimal, rentang `i64` (overflow/ekor-alpha ditolak).
- String: `"…"` dengan escapes `\" \\ \n \t \r \0`; newline mentah /
  EOF sebelum penutup ditolak.
- Operator/simbol: `+ - * / = == != < > <= >= -> ( ) { } [ ] : , ; .`
  (`->` greedy; `!` tunggal ditolak dengan saran `!=`).

## 2. Grammar (ringkas; normatif penuh di `syntax.md`)

Detail per topik: `types.md`, `functions.md`,
`control-flow.md`, `arrays.md`, `structs.md`, `modules.md`,
`standard-library.md`. Sintaks gabungan: `syntax.md`.

```
Program    := (Import | TypeAliasDecl | TraitDecl | ImplBlock |
               StructDecl | EnumDecl | FuncDecl | ExternDecl)*
Import     := "import" Ident [";"]
TypeAliasDecl := "type" Ident "=" Type [";"]                          // v1.2.0
TraitDecl  := "trait" Ident "{" (FuncSig ";")* "}"                    // v1.3.0
ImplBlock  := "impl" Ident "for" Type "{" FuncDecl* "}"               // v1.3.0
StructDecl := "struct" Ident ["<" Ident ("," Ident)* ">"] "{" (FieldDecl [";"])* "}"
FieldDecl  := Ident ":" Type
EnumDecl   := "enum" Ident ["<" Ident ("," Ident)* ">"]               // v1.7.0
              "{" Variant ("," Variant)* [","] "}"
Variant    := Ident ["(" [Type ("," Type)*] ")"]
FuncDecl   := "fn" Ident ["<" Ident ("," Ident)* ">"] "(" [Param ("," Param)*] ")" ["->" Type] Block
ExternDecl := "extern" "fn" Ident "(" [Param ("," Param)*] ")"        // v1.13.0
              ["->" Type] ";"
Param      := Ident ":" Type
Block      := "{" Stmt* "}"
Stmt       := LetStmt | AssignStmt | PrintStmt | ReturnStmt | ExprStmt
            | IfStmt | WhileStmt | MatchStmt                             // v1.6.0
Type       := "i64" | "bool" | "string" | "void" | "[" Type "]" | Ident
            | "fn" "(" [Type ("," Type)*] ")" ["->" Type]          // v1.2.0
Expr       := equality (precedence: == != < > <= >= + - * / unary postfix)
```

`main` (`fn main()`, tanpa parameter, return `void`) wajib tepat satu
per program gabungan. `;` opsional di akhir statement.

## 3. Sistem tipe

Statis, nominal untuk struct, struktural untuk sisanya; tanpa coercion.
Alias `type X = T;` (v1.2.0) transparan — identik dengan
`T`, namespace bersama struct. Tipe fungsi `fn(T)->T` (v1.2.0): value
hanya nama fn top-level (tanpa closure); berlaku di let/param/return/
call; print/`==`/array-elemen/field-struct ditolak. Fungsi generik
`fn f<T>(x: T) -> T` (v1.2.0): inferensi call-site, monomorphisasi
per kombinasi argumen, body dicek per-instance. Struct generik
`struct P<T> { … }` (v1.2.0): konstruksi inferensi/eksplisit,
monomorphisasi per kombinasi, struct by-value di param/return.
Trait `trait T { fn m(self); }` + `impl T for P { … }` (v1.3.0):
dispatch statis ke fungsi mangled; bounds `T: Trait` (+ ganda)
dicek saat instansiasi. Lihat `types.md`, `traits.md`.
`Option<T>`/`Result<T, E>` (v1.5.0 sebagai struct generik biasa;
**sejak 2.0 enum natif** — lihat `docs/error-handling.md` dan
`docs/migration-2.0.md`) + propagasi `expr?` (operan dan return
fungsi harus `Result` sama).
Lihat `error-handling.md`.
`let x: T = e` menuntut `typeof(e) == T`; tanpa anotasi
menurunkan dari inisialisasi. Nilai `void` tak bisa disimpan/dicetak/
dibandingkan. Operator numerik butuh `i64`; kondisi butuh `bool`;
equality butuh sisi sama (non-`string`/`void`/array/struct).
Panggilan dicek arity + tipe; return dicek terhadap return type
(all-paths untuk non-`void`). Lihat `types.md`.

## 4. Semantik runtime

- Evaluasi eager, urutan kiri-ke-kanan; statement sekuensial.
- Model memori normatif: `docs/memory-model.md` (owned values di
  stack + heap string program-lifetime; tanpa GC/pinjaman).
- Variabel = slot stack per scope (shadowing antar-scope; tiap Block
  scope baru). Assignment menimpa slot terdekat.
- Integer: aritmetika two's-complement wrapping, kecuali pembagi nol
  (statis: compile error; dinamis: `runtime error: division by zero` +
  exit 1).
- Boolean untuk kontrol; tanpa operator short-circuit (`&&`/`||`
  tidak ada di v1.0.0) — semua operand selalu dievaluasi.
- String: pointer NUL-terminated buram; komparasi/manipulasi di luar
  builtin (`str_len`, `str_concat`) tidak ada.
- Array: alloca `[N x T]`; index di-bounds-check (`exit 1` bila gagal);
  copy semantik nilai (agregat).
- Struct: alloca named type; field via offset statis; nilai = pointer;
  copy agregat.
- Panggilan: konvensi C AArch64 via LLVM; rekursi didukung.
- `main` mengembalikan 0 ke OS. `exit(n)` tak terekspos (kecuali via
  abort runtime: assert/OOB/div-0 → exit 1).
- Memori: tanpa GC; string heap (`input`, `str_concat`, `file_read`)
  bocor sengaja hingga roadmap GC.

## 5. Modul dan visibilitas

Satu file = satu modul. `import` merge nama publik; `_`-awalan privat
(fungsi/struct); siklus ditolak; rantai > 100 ditolak. Lihat
`modules.md`. Dependensi antar-paket: `docs/pkg.md` (sejak v1.11.0).

## 6. Diagnostics

Setiap error: `file:line:col`, pesan, snippet + caret span, opsional
`saran help:`. Compiler exit != 0 tanpa sinyal pada input apapun yang
ditolak; hang dilarang (test runner memagari 10 dtk).

## 7. Batasan normatif v1.0.0 (historis)

Paragraf ini merekam batas v1.0.0 dan TIDAK berlaku lagi apa adanya:
generics, traits, match, enum, closures, concurrency, FFI, package
manager, registry, self-hosting, dan cross-toolchain semuanya sudah
mendarat di 1.x/2.x (lihat CHANGELOG + `docs/language-spec-2.0.md`
§1–§13 untuk status normatif tiap area). (`async`/`await` tetap
reserved tanpa sintaks — RFC-0004 Proposed.) Perilaku program yang
mengandalkan area di luar spec 2.0 adalah undefined.
