# Tipe Nusa — v2.0.0

Sistem tipe statis Nusa: anotasi wajib di parameter, opsional di `let`
(dengan inferensi), dicek penuh sebelum codegen. Generik via
monomorphisasi (§Generik, §Struct generik); bounds via traits
(`docs/traits.md`).

## Tipe dasar

| Tipe | Nilai | Representasi LLVM |
|---|---|---|
| `i64` | integer 64-bit bertanda | `i64` |
| `bool` | `true` / `false` | `i1` |
| `string` | teks UTF-8, NUL-terminated | `ptr` (`i8*`) |
| `void` | tidak ada nilai (return/statement) | `void` |

`i64`, `bool`, `string`, `void` adalah keyword reserved.

## Array dan struct

- `[T]`: array fixed-size, elemen homogen `T` (bukan `void`/array).
  Ukuran **bukan** bagian tipe. Lihat `arrays.md`.
- `Person`: tipe bernama dari `struct Person { … }`. Lihat `structs.md`.
- `Color`: tipe jumlah dari `enum Color { … }` (v1.7.0). Lihat `enums.md`.
- `fn(params) -> ret`: tipe fungsi (untuk `main`: `fn() -> void`).

## Alias tipe (v1.2.0, Phase 21a)

```nusa
type Answer = i64;
type Nums = [i64];
type B = A;   // forward reference OK

fn id(x: Answer) -> Answer { return x; }
fn main() { let a: Nums = [1, 2]; print(a[0]); }
```

- Alias transparan: berlaku persis seperti tipe target di semua posisi
  (param, return, field struct, anotasi `let`).
- Menghuni namespace tipe bersama struct: duplikat vs struct/alias
  → `duplicate type`; nama reserved (`print`, `len`, …) ditolak.
- Target tak dikenal → `unknown type` (walau tak dipakai); siklus
  (`type A = A;`, `A ↔ B`) → `circular type alias`; rantai > 100
  → `type alias chain too deep`.
- Konstruksi struct via alias ditolak (`cannot construct type alias`);
  pakai nama struct aslinya.
- Batasan target tetap berlaku: alias array sebagai param/return
  ditolak aturan array yang sama (tanpa slices); alias struct
  by-value didukung seperti struct biasa (sejak v1.2.0, Phase 21d).

## Tipe fungsi (v1.2.0, Phase 21b)

```nusa
fn add(a: i64, b: i64) -> i64 { return a + b; }
fn apply(f: fn(i64, i64) -> i64, x: i64, y: i64) -> i64 {
    return f(x, y);   // panggilan tak langsung via variabel
}
fn main() {
    let op = add;     // function reference (hanya fn top-level)
    print(op(20, 22));
    print(apply(add, 1, 2));
}
```

- `fn(T, …) -> T` hanya di posisi tipe (anotasi param/return/`let`,
  target alias); `-> T` boleh absen (= `void`).
- Value fungsi: nama fn top-level (bukan `main`, bukan builtin, bukan
  `print`) ATAU lambda/closure (v1.8.0, lihat `docs/closures.md`).
- Berlaku di `let`/param/return/call; kesetaraan struktural
  (`fn(i64)->i64 == fn(i64)->i64`).
- Ditolak dengan pesan jelas: `print(f)`, `f == g`, array berisi
  fungsi, field struct bertipe fungsi, memanggil non-fungsi.
- Batasan: tanpa currying (`mk()(1)` tak valid — pakai `let` dulu),
  tanpa array/field fn (kecuali struct env hidden closure).

## Generik (v1.2.0, Phase 21c)

```nusa
fn identity<T>(value: T) -> T { return value; }
fn first<T>(a: T, b: T) -> T { return a; }
fn apply<T, U>(f: fn(T) -> U, x: T) -> U { return f(x); }
fn main() {
    print(identity(41) + 1);   // T := i64
    print(identity("hi"));     // T := string (instance lain)
}
```

- Inferensi di call-site dari tipe argumen (tanpa turbofish eksplisit);
  konflik (`f(1, "s")`) dan tak-terinferensi (`zero<T>()`) ditolak jelas.
- Monomorphisasi: tiap kombinasi argumen → instance konkret
  (`identity_i64`, `identity_string`); template tak pernah di-emit.
- Body template dicek PER-INSTANCE (model C++): operasi yang valid
  untuk instance lolos; template tak-dipanggil tak dicek sama sekali.
- Berlaku di semua posisi konkret (let/param/return/call, termasuk
  higher-order generik); batasan ABI natural berlaku per instance
  (argumen array ditolak aturan yang sama; struct by-value didukung
  sejak Phase 21d).
- Batasan: tanpa constraints/bounds di sini (pakai traits, Phase 22 —
  lihat `docs/traits.md`); tanpa argumen tipe eksplisit di panggilan
  fungsi; `main` tak boleh generik; fungsi generik tak bisa jadi
  value (panggil dengan argumen konkret).

## Struct generik (v1.2.0, Phase 21d)

```nusa
struct Point<T> { x: T; y: T; }
fn getx<T>(p: Point<T>) -> T { return p.x; }
fn main() {
    let p = Point { x: 1, y: 2 };          // inferensi T := i64
    let q: Point<i64> = Point<i64> { x: 3, y: 4 };  // eksplisit
    print(getx(p) + q.x);
}
```

- Konstruksi menginferensi dari nilai field, atau argumen eksplisit
  (`Point<i64> { … }`); konflik/arity/hilang dilaporkan presisi.
- Monomorphisasi per kombinasi argumen (`Point_i64`); template
  tak pernah di-emit; body tak-dipakai tak dicek (model C++).
- Berlaku di semua posisi konkret (let/param/return/field
  bersarang/alias); struct by-value di param/return didukung
  (menutup TODO v0.8.0 "records").
- Ditolak jelas: tanpa argumen, arity salah, non-generik berargumen,
  rekursi ukuran-infinit, tabrakan nama mangled, dup tparam.
- Batasan: tanpa constraints/bounds (Phase 22, butuh traits); tanpa
  argumen tipe eksplisit di panggilan fungsi; spasi sebelum `=`
  setelah `>` (`Point<i64> = …`, bukan `Point<i64>=…`).

## Anotasi dan inferensi

```nusa
let age: i64 = 15;   // anotasi: dicek sama dengan inisialisasi
let x = 10 + 5;      // tanpa anotasi: disimpulkan i64
```

Anotasi yang bertentangan ditolak:

```nusa
let x: i64 = "hello";
// x:5: error: mismatched types in 'let': 'x' is declared as 'i64'
//            but initializer has type 'string'
```

## Kompatibilitas operator

- Aritmetika `+ - * /` dan perbandingan `< > <= >=`: operand `i64`.
- Equality `==` `!=`: kedua sisi bertipe sama (bukan `string`/`void`/
  array/struct).
- Unary `-`/`+`: operand `i64`.
- Pembagi literal `0` ditolak statis (`division by zero`); pembagi
  dinamis nol → runtime error + exit(1).

## Batasan (stabil v2.0.0)

- Tanpa coercion (int→string implisit tidak ada).
- `void` tak bisa menjadi tipe variabel, parameter, elemen, atau field
  (hanya return type dan hasil `assert`).
- Aritmetika wrapping (kecuali div-0); `INT64_MIN / -1` wrap diam.
