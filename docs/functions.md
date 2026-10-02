# Fungsi Nusa — v2.0.0

```nusa
fn add(a: i64, b: i64) -> i64 {
    return a + b;
}

fn main() {
    let result = add(10, 20);
    print(result); // 30
}
```

## Deklarasi

```
FuncDecl := "fn" Ident "(" [Param ("," Param)*] ")" ["->" Type] Block
Param    := Ident ":" Type
```

- Anotasi parameter **wajib**; `-> T` absen berarti `void`.
- `main`: tanpa parameter, return `void`; wajib ada tepat satu;
  tak bisa dipanggil; menjadi `i32 @main` (return 0) di executable.
- Nama reserved: `print`, `len`, `input`, `assert`, `str_*`,
  `parse_int`, `file_*`, `array_*`, `puts`, `printf`, `exit`.
- Duplikat nama fungsi → error. Forward reference dan rekursi bisa
  (two-pass di semantic, checker, dan codegen).

## Panggilan

- Arity dan tipe tiap argumen dicek (`wrong number of arguments`,
  `mismatched argument type`). Hasil = return type terdeklarasi.
- Panggilan bersarang bebas: `add(mul(2, 3), 4)`.
- Panggilan fungsi `void` sebagai statement: `greet("Nusa");`
  (nilainya dibuang).

## Return dan local scope

- Tiap fungsi punya local scope (parameter + `let`); tiap Block scope
  baru (shadowing antar-scope diizinkan, duplikat se-scope ditolak).
- `return` harus terakhir di block-nya, KECUALI bare `return;`
  (fungsi `void`) yang legal di tengah block sejak 2.0 (dulu gagal
  verifikasi IR di `main` — bug, kini keluar dengan 0); fungsi
  non-`void` harus selalu me-return di semua path (`if`+`else`
  dihitung; `while` tidak — konservatif). Statement sesudah `return`
  → `unreachable code`.
- `return;` polos hanya untuk fungsi `void`; `return <expr>` di fungsi
  `void` ditolak, begitu pula sebaliknya.

## Backend

Setiap `fn` menjadi LLVM function (`i64`/`i1`/`ptr`/`void`);
parameter via alloca+store; call → LLVM `call`, diverifikasi
`verifyModule` sebelum emit.

## Batasan (stabil v2.0.0)

- Top-level saja (tanpa nested fn). First-class function values
  didukung: tipe `fn(T) -> T` + lambda/closure (lihat `docs/types.md`,
  `docs/closures.md`).
- Tanpa param/return array (butuh slices — mendatang); struct
  by-value didukung sejak v1.2.0.
- Tanpa default argumen, varargs, overloading. Fungsi generik:
  lihat `docs/types.md`.
