# C Interoperability — FFI (Phase 32, v1.13.0)

Nusa memanggil fungsi C native via deklarasi `extern`:

```nusa
extern fn strlen(s: string) -> i64;
extern fn isupper(c: i64) -> bool;

fn main() {
    print(strlen("abcd")); // 4
    print(isupper(65));    // true
}
```

## Aturan signature (batasan eksplisit)

Hanya primitif yang menyeberang:

| Nusa | C | Arah |
|---|---|---|
| `i64` | `long long`/`int64_t` | dua arah |
| `string` | `const char*` (NUL) | param saja |
| `bool` | `int` (truthy) | dua arah |
| `void` | `void` | return saja |

- Parameter: `i64`/`string`/`bool` saja. `void`/`array`/struct/
  function values ditolak dengan arahan (tanpa layout/ABI-nya di v1).
- Return: `i64`/`bool`/`void` saja. **`string` DITOLAK** —
  string hasil-C ber-lifetime tak diketahui (model memori Nusa
  menuntut string program-lifetime); kembalikan `i64`
  (handle/panjang) sebagai gantinya.
- Tanpa generik (`extern fn f<T>` ditolak), tanpa `main` extern,
  tanpa body (`extern fn f(...) { }` ditolak parser).
- Extern hanya panggilan LANGSUNG (`let f = strlen;` ditolak —
  ABI trampolin closure tak berlaku untuk pointer C).

## Linking

```sh
nusa build app.ns -o app --link m --link ./vendor/libfoo.a --link-dir ./lib
```

- `--link <nama|file>` (repeatable): nama → `-l<nama>`, path/`.a`/`.o`/
  `.so` → file langsung. `--link-dir <dir>` (repeatable) → `-L`.
- Manifest paket (`nusa pkg build`):
  ```toml
  [link]
  libraries = ["m"]
  library-dirs = ["./lib"]
  files = ["vendor/libfoo.a"]
  ```
  Path relatif terhadap direktori paket.
- Simbol hilang = error link clang yang jelas (check tetap lolos;
  kegagalan hanya saat link). Pustaka C harus ABI-kompatibel
  (LP64 AArch64; `long` = 64-bit).

## Model keamanan

- `extern` adalah satu-satunya pintu unsafe, dan ia TERBATAS:
  signature di atas ditegakkan checker (bukan konvensi).
- Yang TIDAK ada di v1 (ditolak, bukan UB diam): pointer umum,
  struct by-value, callback Nusa→C, fungsi variadik, string-return,
  `dlsym` dinamis.
- Pustaka yang di-link dipercaya penuh (kode native tanpa sandbox)
  — pin sumbernya (vendor + checksum bila paket; lihat `docs/pkg.md`).

## Batasan Phase 32

- Tanpa `extern struct`/padding query; tanpa bitfield/enum-C;
  tanpa `const`-correctness; tanpa name-mangling C++;
  tanpa lungo/from-C callbacks; tanpa hot-reload.

## Bindgen (Phase 66, v2.17.0)

```sh
nusa bindgen header.h [-o binding.ns]   # stdout bila tanpa -o
```

Menghasilkan deklarasi `extern fn` dari prototipe C (subset §1
`docs/ffi-bindgen-2.17.md`): integer C → `i64`, `_Bool` → `bool`,
`char*` → `string` (param), `void`, typedef umum (`size_t`…),
`extern "C"`, `(void)`. Dilewati + peringatan: float/double,
pointer umum, string-return, variadik, struct/union/enum,
typedef kustom, definisi inline. rc 1 bila nol fungsi. Output
fmt-clean; C++ tetap ditolak (mangling).

## Pola shim (Phase 77, v2.28.0)

API C ber-pointer/callback tak terekspresikan di `extern fn`
(string/i64/bool saja; return string ditolak). Pola ekosistem:
shim C handle-based (pointer disembunyikan di tabel slot i64;
hasil via file/buffer yang dibaca kembali) + wrapper Nusa +
manifest `[link]`. Referensi: `packages/sqlite/` (libsqlite3 →
CRUD `:memory:`; hasil query sebagai CSV → `csv.ns`).
