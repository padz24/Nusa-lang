# Array Nusa — v1.0.0

```nusa
fn main() {
    let numbers: [i64] = [1, 2, 3, 4];
    print(numbers[0]); // 1
    print(len(numbers)); // 4
}
```

## Literal dan tipe

- `[1, 2, 3]`: elemen homogen, non-`void`, non-array (nested ditolak).
  `[]` ditolak (`cannot infer element type`).
- Tipe `[T]` — ukuran **bukan** bagian tipe (`[i64]` cocok semua panjang;
  panjang dilacak codegen dari situs alokasi).
- Representasi: alloca statis `[N x T]` di entry block.

## Indexing dan `len`

- `a[i]`: `i` harus `i64`; base harus array (string/struct/skalar
  ditolak jelas). Berantai didukung (`m[0]` dari hasil apapun yang
  bertipe array).
- **Bounds check selalu aktif**: `i < 0 || i >= len` → cetak
  `runtime error: index out of bounds` + exit(1). Tanpa flag.
- `len(a)` → `i64` (konstanta kompilasi, tanpa runtime call).

## Copy dan assignment

- `let b = a;` dan `b = a;` menyalin seluruh agregat (load+store LLVM).
- Tanpa indexed assignment (`a[0] = x`): array fixed-size yang isinya
  tetap setelah konstruksi.

## Utilitas

Karena array tak bisa melewati batas fungsi (tanpa param/return array),
utilitas generik hidup sebagai builtin: `array_sum`, `array_min`,
`array_max` (argumen `[i64]`, loop inline). Lihat
`docs/standard-library.md`.

## Batasan (stabil v1.0.0)

- Fixed-size dari literal; tanpa `push`/resize, nested, param/return.
- Tanpa `==`/print array (print elemen satu per satu).
- Tanpa generic collections (roadmap 1.x/2.x).
