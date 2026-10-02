# Error Handling Nusa — v2.0 (Phase 45/B1)

Tanpa exception: error adalah NILAI (`Option`/`Result` sebagai enum
natif), diperiksa dengan `match`/`if`, dikombinasikan dengan fungsi,
dipropagasikan dengan `?`.

> BREAKING 2.0: `Option`/`Result` dulu encoding struct (dummy —
> `none(0)`, `ok(5, "")`, field `.value`/`.has_value`/`.is_ok`).
> Kini enum natif. Migrasi: `docs/migration-2.0.md`.

## Option dan Result (enum natif)

```nusa
import option;
import result;
import error;

let o = parse_int_opt("41");      // Option<i64>: Some(41)
let n = parse_int_opt("xx");      // None
print(opt_unwrap_or(n, 0));       // 0

fn parse_age(s: string) -> Result<i64, string> {
    let o = parse_int_opt(s);
    if is_none(o) {
        return Result<i64, string>.Err(str_concat("bad age: ", s));
    }
    match o {
        Some(v) => { return Result<i64, string>.Ok(v); },
        None => { return Result<i64, string>.Err("unreachable"); },
    }
    return Result<i64, string>.Err("unreachable");
}
```

- `enum Option<T> { Some(T), None }`: `some(v)`, `is_some`, `is_none`,
  `opt_unwrap_or`, `opt_map` (tanpa dummy).
- `enum Result<T, E> { Ok(T), Err(E) }`: `is_ok`, `is_err`,
  `res_unwrap_or`, `res_map`, `res_map_err` (tanpa dummy).
- Konstruksi `None`/varian: tipe eksplisit wajib —
  `Option<i64>.None()`, `Result<i64, string>.Ok(v)` (cermin konstruksi
  struct generik `Point<i64> { … }`; `none()` tanpa argumen tak bisa
  diinferensi — konsisten dengan literal `[]` yang butuh anotasi).
- Namespace flat: `import` menggabung nama; `opt_map`/`res_map` dan
  `opt_unwrap_or`/`res_unwrap_or` berprefix karena Nusa tanpa
  overloading (dua `map` akan duplikat). Metode (`o.map`) tak bisa
  generik di v1.3.0 — karenanya kombinator berupa fungsi bebas.

## Propagasi `?`

```nusa
fn get_age(s: string) -> Result<i64, string> {
    let n = parse_age(s)?;   // Err → return awal; Ok → unwrap
    return Result<i64, string>.Ok(n + 1);
}
```

- `expr?` di posisi nilai/statement; menempel (`f()?`, `x?`).
- Aturan: operan `Result<T, E>`; fungsi pembungkus HARUS me-return
  `Result` yang SAMA persis (E sama — tanpa konversi). `?` di `main`
  (void), di `Option`, dan E-beda ditolak dengan pesan jelas.
- Implementasi: uji tag varian Ok + unwrap slot (tanpa overhead
  dibanding tulisan tangan).

## Error standar dan API fallible

- `struct Error { message: string; }` + `fn error(m: string) -> Error`
  (di `std/error.ns`): konvensi E untuk `Result<T, Error>`.
- `parse_int_valid(s) -> bool` (builtin, konsisten `strtoll`):
  `true` bila seluruh input numerik (`"  -7 "` OK; `""`/`"12x"` tidak).
  `parse_int` lama dipertahankan (kompatibilitas: 0 untuk invalid).
- `parse_int_opt(s) -> Option<i64>`, `file_read_opt(p)` (di std,
  murni Nusa di atas builtin): pengganti aman.
- `file_read`/`parse_int` lama TIDAK diubah (backward compatibility).

## Matching dan batasan

- Matching idiomatis = `match` atas varian: `Some(v)`/`None` untuk
  Option, `Ok(v)`/`Err(e)` untuk Result — exhaustiveness varian
  ditegakkan (lihat `docs/pattern-matching.md`). Alternatif
  (`if` atas predikat `is_some`/`is_ok`) tetap valid; akses field
  langsung (`.value`, `.has_value`, `.is_ok`) DITOLAK untuk enum
  (tempa manual tak didukung — gunakan match/kombinator).
- `res_map_err` memanggil `f` hanya di jalur Err (tanpa efek
  samping semu).
- Tanpa exception/try-catch/panic; tanpa konversi error otomatis
  (`From`); tanpa `?` untuk `Option`.
