# Struct Nusa — v2.0.0

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
    print(person.age); // 1
}
```

## Deklarasi

```
StructDecl := "struct" Ident ["<" Ident ("," Ident)* ">"] "{" (FieldDecl [";"])* "}"
FieldDecl  := Ident ":" Type
```

- Field bertipe; `;` opsional; struct kosong diizinkan.
- Duplikat nama struct/field ditolak. Nama reserved
  (`print`/`len`/`exit`/…) tak bisa dipakai.
- Field `void` atau array ditolak (array butuh ukuran statis yang tak
  ada di tipe). Nested struct (`inner: Inner`) bisa.

## Konstruksi

```
StructLit := Ident ["<" Type ("," Type)* ">"] "{" [FieldInit ("," FieldInit)*] "}"
```

- Urutan inisialisasi bebas; **semua field wajib diisi** (missing →
  error; unknown/duplikat → error dengan daftar field sebagai hint).
- Tipe tiap nilai harus sama dengan tipe field (`mismatched field type`).
- Tipe hasil = tipe struct bernama.

## Akses field

- `person.age` (berantai: `a.b.c`, campur index: `a.b[0]`).
- Menghasilkan tipe field — bisa langsung dihitung (`p.age + 1`).
- Field tak dikenal → error + daftar field. Akses di non-struct
  (`x.y` dengan `x: i64`) ditolak jelas.
- Backend: named LLVM type (`%Person = type { ptr, i64 }`), StructGEP +
  load (skalar) / pointer (nested). Nilai struct = pointer (tanpa load);
  copy via agregat.

## Namespace

Struct hidup di namespace terpisah dari fungsi (ala Rust):
`struct Foo` + `fn Foo` boleh koeksis; pemakaian dibedakan posisi
(`{` vs `(`). Ambiguitas `if c {` vs literal dipecahkan lookahead.

## Batasan (stabil v2.0.0; by-value sejak v1.2.0)

- Tanpa field array, `==`/print struct.
- Param/return struct by-value didukung sejak v1.2.0 (records).
- Struct generik: lihat `types.md`.
- Field selalu publik (visibilitas `_` hanya untuk fungsi/struct).
- Tanpa inherent methods/method generik; perilaku polimorfik via
  `impl Trait for Struct` (lihat `docs/traits.md`). Tanpa tuple.
