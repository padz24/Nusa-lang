# Trait Nusa — v1.3.0 (Phase 22)

Mekanisme abstraksi Nusa: trait mendeklarasikan metode, `impl`
mengimplementasikannya untuk tipe konkret, panggilan `p.m(...)`
di-resolve statis (ditulis ulang menjadi fungsi biasa bermangled).

```nusa
trait Printable {
    fn print(self);
    fn describe(self) -> string;
}
struct Person { name: string; }
impl Printable for Person {
    fn print(self) { print(self.name); }
    fn describe(self) -> string { return self.name; }
}
fn main() {
    let p = Person { name: "Nusa" };
    p.print();               // → Printable_print_Person(p)
    print(p.describe());
}
```

## Deklarasi

- `trait Name { fn m(self, …) …; }` — metode TANPA body. Param pertama
  WAJIB `self` telanjang (tanpa anotasi); `self: T` ditolak parser.
- Nama trait menghuni namespace sendiri (`trait Foo` + `struct Foo`
  boleh koeksis). Duplikat trait/metode ditolak. `fn main` dilarang
  sebagai metode. Metode generik (`fn m<T>`) ditolak (v1.3.0).
- Keyword reserved: `trait`, `impl`, `for`, `self`.

## Implementasi

- `impl Trait for Type { … }` — target: struct (biasa/generik
  konkret), primitif (`i64`/`bool`/`string`); array/fn/void ditolak.
- Tiap metode: tanpa tparams, `self` pertama, nama ada di trait,
  signature COCOK (substitusi `self` → target): arity, param, return.
- Metode hilang → `missing method`; asing → `is not a member`;
  duplikat impl (trait+tipe sama) ditolak.
- Metode menjadi fungsi biasa bernama-mangled
  (`Trait_method_Type`, mis. `Printable_print_Person`,
  `Describe_describe_Point_i64`) — tabrakan dengan fungsi user
  ditolak jelas.

## Panggilan

- `recv.method(args)` — receiver struct/primitif/instance generik.
  Resolve: (tipe, nama) → tepat satu implementasi.
- Tak dikenal → `unknown method` + daftar metode tipe itu; ganda
  (dua trait) → `ambiguous method` + daftar trait.
- `p.field` tanpa parens tetap field access; bila field tak ada tapi
  metode ada → hint `did you mean to call the method?`.
- Privacy `_` dicerminkan dari fungsi (lintas-file ditolak).
- Tanpa closure: metode sebagai value tak didukung; `p.m` (tanpa `()`)
  adalah field access biasa.

## Bounds (constraints)

```nusa
fn f<T: Printable>(v: T) -> string { return v.show(); }
fn g<T: Show + Count>(v: T) -> i64 { return v.show2(); }
struct Box<T: Show> { v: T; }
```

- Bounds di tparam fungsi dan struct (`+` untuk ganda); nama trait
  harus dikenal (dicek semantic).
- Dicek saat instansiasi pada tipe KONKRET: `type 'X' does not
  implement trait 'Y'` + saran `impl`.
- Body generik tak pernah dicek langsung (model C++): panggilan
  metode di body ter-resolve saat instance konkret.

## Batasan (v1.3.0)

- Tanpa metode generik, tanpa impl generik (`impl<T> …`),
  tanpa supertrait, tanpa body default, tanpa tipe `Self`
  (`-> Self` → `unknown type`), tanpa associated function/statik.
- Tanpa dynamic dispatch (semua statis/monomorphisasi).
- Koherensi: impl harus unik program-wide (duplikat ditolak);
  evasi via alias (`type X = …` ganda) tak terdeteksi — limitasi.
