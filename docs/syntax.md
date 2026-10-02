# Sintaks Nusa — v2.0.0

Spesifikasi sintaks yang **didukung penuh** (terkompilasi menjadi
native executable), kecuali dinyatakan lain.

## Program & modul

```nusa
// project/main.ns
import math;

fn main() {
    print(add(2, 3));
}
```

```nusa
// project/math.ns
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}
```

- Item top-level bebas campur: `import`, `type`, `trait`, `impl`,
  `struct`, `enum`, `fn`, `extern fn` (generik — lihat `types.md`,
  `traits.md`; enum — `enums.md`; extern — `ffi.md`).
- `import math;` memuat `<dir>/math.ns` (transitif; resolusi penuh
  di `docs/modules.md`). Dependensi antar-paket via `nusa.toml`
  (lihat `docs/pkg.md`).
- Tiap modul namespace sendiri; import menggabung nama PUBLIK.
  Konflik (nama sama) → error duplikat.
- Visibilitas dasar: nama `_`-awalan module-private (`_helper` beda
  file → error); sisanya publik.
- Circular import → `circular import detected: a -> b -> a`.
- `fn main()` wajib ada (di salah satu modul) sebagai entry point.
  Urutan deklarasi bebas; rekursi bisa dikompilasi.

## Struct

```nusa
struct Person {
    name: string;
    age: i64;
}

fn main() {
    let person = Person {
        name: "Nusa",
        age: 1
    };
    print(person.age);
}
```

- Field bertipe (`;` opsional); urutan inisialisasi bebas; semua field
  wajib diisi; nested struct bisa (`a.b.c`).
- Akses `p.field` menghasilkan tipe field (bisa dihitung: `p.age + 1`).
- Batasan v0.8.0 yang tersisa: tanpa field array, tanpa `==`/print
  struct, tanpa indexed assignment (konsisten v0.7.0). Param/return
  struct by-value didukung sejak v1.2.0 (lihat `docs/structs.md`).

## Fungsi

```
FuncDecl := "fn" Ident "(" [Param ("," Param)*] ")" ["->" Type] Block
Param    := Ident ":" Type
```

- Anotasi parameter wajib; `-> T` absen = `void`.
- `main`: tanpa parameter; return void (absen/`-> void`).
- Nama reserved: `print`, `len`, `puts`, `printf`, `exit`.
- Local scope leksikal (v0.6.0): tiap Block scope baru; shadowing
  antar-scope diizinkan, duplikat se-scope ditolak.
- `return`: harus terakhir di block-nya; fungsi non-void harus selalu
  me-return di semua path (`if`+`else` dihitung; `while` tidak).
- `extern fn name(params) -> T;` (v1.13.0, tanpa body): deklarasi
  fungsi C; signature terbatas (lihat `docs/ffi.md`).

## Statement

| Statement | Contoh | Eksekusi |
|---|---|---|
| `let` | `let x = 10 + 20 * 3;` atau `let age: i64 = 15;` | ✅ |
| `print` | `print(result);` / `print("Hi");` | ✅ string, integer, boolean |
| `return` | `return a + b;` / `return;` | ✅ (aturan di atas) |
| call | `greet("Nusa");` | ✅ (nilai dibuang) |
| `if`/`else` | `if x > 5 { } else { }`, `else if` | ✅ (kondisi `bool`) |
| `while` | `while n > 0 { }` | ✅ (kondisi `bool`) |
| assign | `n = n - 1;` | ✅ (harus ada, tipe sama) |
| `match` | `match x { 1 => { }, _ => { }, }` | ✅ v1.6.0 (`docs/pattern-matching.md`) |
| `enum` | `enum Color { Red, Green(i64) }` | ✅ v1.7.0 (`docs/enums.md`) |
| lambda | `\|x: i64\| x + 1`, `\|x\| { return x; }` | ✅ v1.8.0 (`docs/closures.md`) |

`;` opsional (diwarisi v0.1.0). Variabel harus dideklarasikan sebelum
dipakai (`undefined variable` jika tidak).

## Ekspresi & precedence

Dari terendah ke tertinggi:

| Level | Operator | Asosiatif |
|---|---|---|
| equality | `==` `!=` | kiri |
| comparison | `<` `>` `<=` `>=` | kiri |
| term | `+` `-` (biner) | kiri |
| factor | `*` `/` | kiri |
| unary | `-x` `+x` | kanan |
| primary | literal, call, struct-lit, variabel, `(...)` | — |
| postfix | `[i]` index, `.f` field (berantai) | kiri |

Panggilan: `name(arg, ...)` — arity & tipe dicek
(`wrong number of arguments`, `mismatched argument type`).
`main` tak bisa dipanggil. Konstruksi `P { f: v }` dibedakan dari
block via lookahead (`{` diikuti `}`/`field :`, else milik `if`).

Contoh:

```nusa
let x = 10 + 20 * 3;   // (+ 10 (* 20 3))  → 70, bukan 90
let y = (10 + 20) * 3; // (* (+ 10 20) 3)  → 90
let b = 1 + 2 * 3 == 7; // (== (+ 1 (* 2 3)) 7)
let z = -x * 2;        // (* (- x) 2)
```

## Tipe (v0.4.0 + array v0.7.0 + struct v0.8.0 + alias v1.2.0)

Tipe dasar: `i64`, `bool`, `string`, `void` (reserved keywords),
plus array `[T]` (elemen homogen; ukuran bukan bagian tipe)
plus tipe bernama struct (`Person`, dikenali bila dideklarasikan —
jika tidak → `unknown type`)
plus alias tipe (`type Nums = [i64];` — transparan, berlaku persis
seperti targetnya; forward reference OK; siklus/rantai dalam ditolak)
plus tipe fungsi (`fn(i64) -> i64` — hanya di posisi tipe; value-nya
nama fn top-level atau lambda/closure, lihat `docs/closures.md`)
plus aplikasi struct generik (`Point<i64>` — lihat `types.md`).
`void` tidak boleh menjadi tipe variabel atau nilai `print`.

```nusa
let age: i64 = 15;        // anotasi: inisialisasi harus i64
let active: bool = true;  // anotasi: inisialisasi harus bool
let name: string = "Nusa";
let inferred = 10 + 5;    // tanpa anotasi → disimpulkan i64
```

Aturan:
- Anotasi vs inisialisasi harus sama: `let x: i64 = "hello";` ditolak
  (`mismatched types ... declared as 'i64' but initializer has type 'string'`).
- Aritmetika `+ - * /` dan perbandingan `< > <= >=`: operand `i64`.
- Equality `==` `!=`: kedua sisi bertipe sama (non-string, non-void).
- Unary `-`/`+`: operand `i64`.
- `fn main` bertipe `fn() -> void`; fungsi lain `fn(params) -> ret`
  sesuai deklarasi (generik: `fn f<T>(x: T) -> T`, lihat `docs/types.md`).

## Array (v0.7.0)

```nusa
let numbers: [i64] = [1, 2, 3, 4];  // literal homogen
print(numbers[0]);                   // indexing → 1
print(len(numbers));                 // builtin len() → 4
let total = numbers[0] + numbers[3]; // elemen dipakai seperti nilai biasa
```

Aturan:
- Elemen sejenis, non-void, non-array; `[]` ditolak (tulis isinya).
- Index harus `i64`; di luar `[0, len)` → `runtime error: index out of
  bounds` + exit(1) (bounds check selalu aktif).
- `len()` butuh tepat 1 argumen array (`len` reserved, bukan variabel).
- Array fixed-size dari literal: tanpa `push`, tanpa `a[0] = x`,
  tanpa nested, tanpa param/return array, tanpa `==`/print array.
- Copy utuh bisa: `let b = a;` / `b = a;` (element-wise via agregat).

## Fungsi sebagai nilai

Function value didukung: nama fn top-level sebagai value (`let op =
add;`) + lambda/closure (`|x| x + 1`, capture by-value). Detail:
`docs/types.md` (tipe `fn`), `docs/closures.md` (lambda). Nama
fungsi hidup di namespace terpisah dari variabel untuk posisi
panggilan langsung.

## Literal & identifier

- Integer desimal 64-bit: `0`, `42`, `9223372036854775807` (→ `i64`).
  Overflow (`...808`) dan `123abc` ditolak lexer.
- String `"..."` dengan escapes `\" \\ \n \t \r \0` (→ `string`).
- Boolean `true` / `false` (→ `bool`).
- Identifier: huruf/`_` diawali, diikuti alnum/`_` (`x`, `result`, …).
  Nama tipe (`i64`, `bool`, `string`, `void`) reserved, tak bisa menjadi
  nama variabel.

## Token lengkap (lexer)

Keywords: `fn let return if else while true false i64 bool string void struct import type trait impl for self match enum extern async await macro` (`async`/`await` reserved v2.0 untuk RFC-0004; `macro` reserved v2.20.0, Phase 69; tanpa sintaks)

Operator: `+ - * / = == != < > <= >= -> => ? |` (`->` untuk return type;
`?` postfix propagasi `Result`;
`-` diikuti `>` selalu arrow — kombinasi Minus+Gt tak pernah valid).

Simbol: `( ) { } [ ] : , ; .` (`.` untuk field access).

Komentar `//` sampai akhir baris (ditangkap lexer untuk `fmt`/`doc`);
`///` menandai dokumentasi API. Setiap token membawa
`file:line:column`.

## Builtin & stdlib (v0.9.5)

Tanpa sintaks baru: `input()`, `assert(b)`, `str_len`, `str_concat`,
`parse_int`, `file_read/write/exists`, `array_sum/min/max`, `len()`
adalah panggilan biasa (global, reserved). Modul `std/` di-`import`
biasa — lihat `docs/standard-library.md`.
