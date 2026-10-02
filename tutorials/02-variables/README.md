# 02 — Variables & types

Tujuan: `let`, anotasi `i64`/`bool`/`string`, array + `len`.
Prasyarat: `tutorials/01-hello/`.

```bash
./build/nusa run tutorials/02-variables/main.ns
# 30
# true
# Nusa
# 1
# 4
```

Aturan: tanpa anotasi tipe disimpulkan (`let x = 10` → `i64`);
salah tipe ditolak checker (`mismatched types`).
Index di luar batas = runtime error + exit 1 (bounds check selalu aktif).

Lihat: `docs/types.md`, `docs/arrays.md`.
Lanjut: `tutorials/03-functions/`.
