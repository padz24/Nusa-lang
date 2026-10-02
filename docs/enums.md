# Enums & Algebraic Data Types (Phase 26, v1.7.0)

Enum mendeklarasikan tipe jumlah (sum type): sebuah nilai adalah TEPAT
SATU dari varian yang didaftar, masing-masing boleh membawa payload
posisional:

```nusa
enum Color { Red, Green, Blue }
enum Shape { Circle(i64), Rect(i64, i64), Dot }
enum Wrap<T> { Val(T), Empty }
```

Payload ditulis POSISIONAL ala MASTER (`Ok(T)`, bukan `Ok(value: T)`).
Varian tanpa payload (`Red`, `Dot`) = varian unit. Minimal satu varian
(enum kosong ditolak parser).

## Konstruksi: `Enum.Varian(...)`

```nusa
let c: Color = Color.Red;
let s: Shape = Shape.Circle(5);
let w: Wrap<i64> = Wrap.Val(7);
```

- Konstruktor SELALU qualified (`E.V(...)`); bentuk telanjang `V(...)`
  ditolak dengan arahan (`'V' is a variant of enum 'E'; construct it
  qualified`).
- Varian unit boleh tanpa kurung (`Color.Red`); varian bervayload wajib
  dengan arity tepat.
- `E` telanjang bukan nilai (`cannot use enum 'E' as a value`).
- Nama `E` menempati namespace tipe bersama struct/alias (duplikat
  ditolak); `E { ... }` ala struct literal ditolak
  (`cannot construct enum ... as struct`).

## Generic enums

```nusa
enum Result<T, E> { Ok(T), Err(E) }
```

- Inferensi payload: `Wrap.Val(7)` mengikat `T = i64` (cermin struct
  generik). Tparam yang tak terikat payload DITOLAK dengan jelas —
  tulis eksplisit: `Wrap<i64>.Empty`, `Pair<i64, string>.First(4)`.
- Bounds trait (`enum B<T: Printable>`) didukung (cermin struct).
- Tipe payload `void`/array/fn ditolak (cermin field struct).

## Matching

```nusa
match s {
    Circle(r) => { return r; },
    Rect(w, h) => { return w + h; },
    Dot => { return 0 - 1; },
}
```

- Pola varian posisional (`Circle(r)`); varian unit boleh telanjang
  (`Red`, `Dot`) — dibaca sebagai varian bila namanya cocok, BUKAN
  binding.
- Binding Kapital yang BUKAN varian ditolak (`unknown variant`;
  binding lowercase bebas — typo Kapital tak diam-diam mengikat).
- Exhaustiveness: SELURUH varian menutup enum (pesan menyebut yang
  hilang: `missing pattern(s) for 'Blue'`); guard menonaktifkan
  kontribusi seperti biasa.
- `print` nilai enum ditolak (cermin struct) — match lalu print payload.

## Metode & trait atas enum

Enum adalah tipe penuh: `impl Trait for E` dan panggilan metode
(`c.show()`) bekerja via dispatch statis yang sama dengan struct.

## Implementasi: hidden struct (tanpa codegen baru)

`enum E { V0(..), V1(..) }` didesugar checker menjadi hidden struct
`E { tag: i64, v0_0: T, ... }` (slot `v{tag}_{pos}` per payload):

- Seluruh mesin struct dipakai ulang: resolveType, template generik +
  monomorphisasi (`E_i64`), layout LLVM, trait/method, penolakan print.
- Konstruktor ditulis ulang menjadi StructLit SUBSET (`tag` + slot
  aktif); slot non-aktif tak diinisialisasi (undef) — sound karena
  match hanya membaca slot varian yang cocok dengan `tag`, `==` struct
  ditolak checker, dan tempa manual diblokir di dua lapis.
- Pola varian = uji `tag` + destruktur slot (tag dianotasi saat
  validasi; codegen tak tersentuh kecuali toleransi field-hilang dan
  flag layout).
- Representasi LLVM: struct datar `{i64, payload...}` — efisien untuk
  v1 (niche optimization = roadmap).

## Batasan Phase 26

- Konstruktor sintaktis (bukan first-class function; tanpa partial
  application — closure = Phase 27).
- Tanpa payload bernama, tanpa pola or/range, tanpa diskriminan
  eksplisit (`#[repr]`), tanpa exhaustiveness nesting-dalam (hanya
  top-level).
- `Option`/`Result` std: encoding struct v1.5.0 DIGANTI enum natif
  sejak 2.0 (BREAKING B1 — lihat `docs/error-handling.md` dan
  `docs/migration-2.0.md`).
- Struct literal/pattern atas nama enum diblokir; pola struct atas
  hidden struct tak terjangkau user.
